/*
 * share_db.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "gift.h"

#include "file.h"
#include "mime.h"
#include "md5.h"

#include "network.h"

#include "share_file.h"
#include "share_db.h"

#include "meta.h"

/*****************************************************************************/

static ft_uint32 read_int (SDB *db, size_t integer_size)
{
	ft_uint32 integer;

	if (integer_size != 4)
		return FALSE;

	if (fread (&integer, integer_size, 1, db->f) <= 0)
		return FALSE;

	return ntohl (integer);
}

static size_t write_int (SDB *db, ft_uint32 integer, size_t integer_size)
{
	size_t written;

	assert (integer_size == 4);

	/* network order.  these databases may go over the socket */
	integer = htonl (integer);
	written = fwrite (&integer, integer_size, 1, db->f);

	return written;
}

/*****************************************************************************/

/* TODO -- file.c BADLY needs to incorporate better file_read/write wrappers
 * for error output */
static int seek_db (SDB *db, off_t offs)
{
	if (fseek (db->f, offs, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
		return FALSE;
	}

	return TRUE;
}

/* verify that this is an giFT shares database :) */
static int verify_db (SDB *db, size_t *nrec)
{
	ft_uint32 recs = 0;
	char      db_check[4];
	size_t    n;

	if (!db || !db->f)
		return FALSE;

	/* make sure we're positioned at the beginning of the file */
	if (!seek_db (db, 0))
		return FALSE;

	/* read the signature */
	if ((n = fread (db_check, sizeof (char), sizeof (db_check), db->f)) <= 0)
	{
		GIFT_ERROR (("cannot read %s: %s", db->path, GIFT_STRERROR()));
		return FALSE;
	}

	if (strncmp (db_check, "giFT", sizeof (db_check)))
	{
		TRACE (("invalid database file"));
		return FALSE;
	}

	/* retrieve the number of records in this db */
	recs = read_int (db, sizeof (recs));

	if (nrec)
		*nrec = recs;

	/* file pointer is now positioned at the first record */

	return TRUE;
}

SDB *sdb_new (char *path, char *mode)
{
	SDB *db;

	if (!(db = malloc (sizeof (SDB))))
		return NULL;

	db->f    = NULL;
	db->path = STRDUP (path);
	db->mode = STRDUP (mode);
	db->nrec = 0;

	return db;
}

static void sdb_free (SDB *db)
{
	if (!db)
		return;

	if (db->f)
		fclose (db->f);

	free (db->path);
	free (db->mode);
	free (db);
}

int sdb_open (SDB *db)
{
	FILE  *f;
	size_t nrec = 0;

	if (!(f = fopen (db->path, db->mode)))
		return FALSE;

	db->f = f;

	if (!verify_db (db, &nrec))
	{
		fclose (db->f);
		db->f = NULL;
		return FALSE;
	}

	db->nrec = nrec;

	return TRUE;
}

int sdb_create (SDB *db)
{
	FILE      *f;
	ft_uint32  recs = 0;

	TRACE (("%s", db->path));

	if (!(f = fopen (db->path, "w")))
	{
		GIFT_ERROR (("Can't open %s: %s", db->path, GIFT_STRERROR()));
		return FALSE;
	}

	db->f    = f;
	db->nrec = recs;

	fwrite ("giFT", sizeof (char), 4, db->f);
	write_int (db, recs, sizeof (recs));

	return TRUE;
}

void sdb_unlink (SDB *db)
{
	char *path;

	path = STRDUP (db->path);

	sdb_free (db);

	if (path)
	{
		unlink (path);
		free (path);
	}
}

void sdb_close (SDB *db)
{
	sdb_free (db);
}

/*****************************************************************************/

static off_t new_record_offset (SDB *db)
{
	off_t db_offs = 0;

	fseek (db->f, 0, SEEK_END);
	db_offs = (off_t) ftell (db->f);

	return db_offs;
}

static SDBRecord *sdb_record_new (SDB *db, off_t start, off_t written, int nrecs)
{
	SDBRecord *rec;

	if (!(rec = malloc (sizeof (SDBRecord))))
		return NULL;

	rec->db      = db;
	rec->start   = start;
	rec->written = written;
	rec->nrecs   = nrecs;
	rec->fatal   = FALSE;

	return rec;
}

static off_t sdb_record_free (SDBRecord *rec)
{
	off_t written = rec->written;

	free (rec);

	return written;
}

SDBRecord *sdb_record_start (SDB *db)
{
	off_t start;

	if (!(start = new_record_offset (db)))
		return NULL;

	if (!seek_db (db, 4))
		return NULL;

	if (write_int (db, ++db->nrec, sizeof (ft_uint32)) <= 0)
		return NULL;

	if (!seek_db (db, start))
		return NULL;

	/* pad the record header so that we do not need to buffer data in
	 * memory */
	if (write_int (db, 0, sizeof (ft_uint32)) <= 0)   /* total size */
		return NULL;

	if (write_int (db, 0, sizeof (ft_uint32)) <= 0)   /* number of writes */
		return NULL;

	/* create the record object */
	return sdb_record_new (db, start, 0, 0);
}

off_t sdb_record_stop (SDBRecord *rec)
{
	SDB  *db;
	off_t start;
	off_t written;
	int   nrecs;

	if (rec->fatal)
	{
		sdb_record_free (rec);
		return 0;
	}

	/* destroy the record object */
	db      = rec->db;
	start   = rec->start;
	written = rec->written;
	nrecs   = rec->nrecs;

	sdb_record_free (rec);

	/* write the real record header */
	if (!seek_db (db, start))
		return 0;

	if (write_int (db, (ft_uint32) written, sizeof (ft_uint32)) <= 0)
		return 0;

	if (write_int (db, (ft_uint32) nrecs, sizeof (ft_uint32)) <= 0)
		return 0;

	return written;
}

static int rec_write (SDBRecord *rec, SDBRecordType t, void *data, ft_uint32 len)
{
	size_t l;
	size_t w;

	if (!len || rec->fatal)
		return FALSE;

	if (write_int (rec->db, len, sizeof (len)) <= 0)
	{
		rec->fatal = TRUE;
		return FALSE;
	}

	rec->written += sizeof (len);

	if (write_int (rec->db, (ft_uint32) t, sizeof (ft_uint32)) <= 0)
	{
		rec->fatal = TRUE;
		return FALSE;
	}

	rec->written += sizeof (ft_uint32);

	switch (t)
	{
	 case SDBRECORD_UINT32:
		l = 1;
		w = write_int (rec->db, (*((ft_uint32 *) data)), (size_t) len);
		break;
	 case SDBRECORD_STR:
		l = (size_t) len;
		w = fwrite (data, sizeof (char), l, rec->db->f);
		break;
	 default:
		rec->fatal = TRUE;
		return FALSE;
	}

	if (w < l)
	{
		rec->fatal = TRUE;
		return FALSE;
	}

	rec->written += len;

	return TRUE;
}

int sdb_record_write (SDBRecord *rec,
                      SDBRecordType keyt, void *key, ft_uint32 key_len,
                      SDBRecordType datat, void *data, ft_uint32 data_len)
{
	int ret = 0;

	/* NOTE: this does NOT set rec->fatal as a convenience feature */
	if (!key_len || !data_len)
		return FALSE;

	ret += rec_write (rec, keyt, key, key_len);
	ret += rec_write (rec, datat, data, data_len);

	rec->nrecs++;

	return (ret == 2) ? TRUE : FALSE;
}

/*****************************************************************************/

SDBRecord *sdb_record_get (SDB *db, off_t start)
{
	ft_uint32 size = 0;
	ft_uint32 nrec = 0;

	if (!db || !db->f)
		return NULL;

	if (start == 0)
	{
		/* man page says it returns long, but i think its lying :) */
		start = (off_t) ftell (db->f);
	}

	if (!(size = read_int (db, sizeof (size))))
		return NULL;

	if (!(nrec = read_int (db, sizeof (nrec))))
		return NULL;

	return sdb_record_new (db, start, size, nrec);
}

static int rec_read (SDBRecord *rec, SDBRecordType *t, void **key, ft_uint32 *len)
{
	if (rec->fatal)
		return FALSE;

	/* sanity check on the size of this record */
	if (!(*len = read_int (rec->db, sizeof (*len))) || *len > 8192)
	{
		TRACE (("str_len = %u", (unsigned int) *len));
		return FALSE;
	}

	*t = (SDBRecordType) read_int (rec->db, sizeof (ft_uint32));

	switch (*t)
	{
	 case SDBRECORD_UINT32:
		*key = I_PTR (read_int (rec->db, *len));
		break;
	 case SDBRECORD_STR:
		if (!(*key = malloc (*len)))
			return FALSE;
		fread (*key, sizeof (char), (size_t) *len, rec->db->f);
		break;
	}

	if (rec->written < (SDBRECORD_TYPE_HEADER + *len))
	{
		rec->fatal = TRUE;
		return FALSE;
	}

	/* abuse! abuse! */
	rec->written -= (SDBRECORD_TYPE_HEADER + *len);
	return TRUE;
}

int sdb_record_read (SDBRecord *rec,
					 SDBRecordType *keyt, void **key, ft_uint32 *key_len,
					 SDBRecordType *datat, void **data, ft_uint32 *data_len)
{
	int ret = 0;

	if (rec->nrecs <= 0)
		return FALSE;

	ret += rec_read (rec, keyt, key, key_len);
	ret += rec_read (rec, datat, data, data_len);

	rec->nrecs--;

	return (ret == 2) ? TRUE : FALSE;
}

/*****************************************************************************/

static int write_meta (Dataset *d, DatasetNode *node, SDBRecord *rec)
{
	char *mkey;

	if (!(mkey = stringf ("X-%s", node->key)))
		return FALSE;

	sdb_record_write (rec,
	                  SDBRECORD_STR, mkey, STRLEN_0 (mkey),
	                  SDBRECORD_STR, node->value, STRLEN_0 (node->value));

	return FALSE;
}

int sdb_write_file (SDB *db, FileShare *file)
{
	SDBRecord *rec;
	off_t      start;
	ShareHash *sh;
	ft_uint32  mtime;
	ft_uint32  size;

	/* lots of shit can happen here ;) */
	if (!db || !db->f || !file || !SHARE_DATA(file) || !SHARE_DATA(file)->path ||
		!(sh = share_hash_get (file, "MD5")))
		return FALSE;

	if (!(rec = sdb_record_start (db)))
		return FALSE;

	start = rec->start;

	mtime = (ft_uint32) SHARE_DATA(file)->mtime;
	size  = (ft_uint32) file->size;

	sdb_record_write (rec,
	                  SDBRECORD_STR,    "mtime", 6,
	                  SDBRECORD_UINT32, &mtime,                  sizeof (mtime));
	sdb_record_write (rec,
	                  SDBRECORD_STR,    "mime",  5,
	                  SDBRECORD_STR,    file->mime,              STRLEN_0 (file->mime));
	sdb_record_write (rec,
	                  SDBRECORD_STR,    "md5",   4,
	                  SDBRECORD_STR,    sh->hash,                sh->len);
	sdb_record_write (rec,
	                  SDBRECORD_STR,    "root",  5,
	                  SDBRECORD_STR,    SHARE_DATA(file)->root,  STRLEN_0 (SHARE_DATA(file)->root));
	sdb_record_write (rec,
	                  SDBRECORD_STR,    "path",  5,
	                  SDBRECORD_STR,    SHARE_DATA(file)->path,  STRLEN_0 (SHARE_DATA(file)->path));
	sdb_record_write (rec,
	                  SDBRECORD_STR,    "hpath", 6,
	                  SDBRECORD_STR,    SHARE_DATA(file)->hpath, STRLEN_0 (SHARE_DATA(file)->hpath));
	sdb_record_write (rec,
	                  SDBRECORD_STR,    "size",  5,
	                  SDBRECORD_UINT32, &size,                   sizeof (size));

	meta_foreach (file, DATASET_FOREACH (write_meta), rec);

	return (sdb_record_stop (rec) ? start : 0);
}

/*****************************************************************************/

static int add_meta (Dataset *d, DatasetNode *node, FileShare *file)
{
	meta_set (file, node->key, node->value);
	return FALSE;
}

static FileShare *merge_file (char *root, char *path,
                              char *hpath, char *mime, off_t size,
                              time_t mtime, unsigned char *md5,
                              Dataset *meta)
{
	FileShare *file;

	/* TODO -- use this eventually */
	free (hpath);

	if (!path || !md5 || !size || !mime)
	{
		free (root);
		free (path);
		free (mime);
		free (md5);
		dataset_clear (meta);

		return NULL;
	}

	file = share_new (NULL, root, STRLEN (root), path, mime,
	                  size, mtime);

	if (!share_hash_set (file, "MD5", md5, 16))
		free (md5);

	free (root);
	free (path);
	free (mime);

	/* add meta data */
	if (file)
	{
		meta_clear (file);
		dataset_foreach (meta, DATASET_FOREACH (add_meta), file);
	}

	dataset_clear (meta);

	return file;
}

FileShare *sdb_read_file (SDB *db, FileShare *file)
{
	SDBRecord     *rec;
	off_t          rec_start;
	off_t          rec_len;
	SDBRecordType  keyt;
	void          *key      = NULL;
	ft_uint32      key_len  = 0;
	SDBRecordType  datat;
	void          *data     = NULL;
	ft_uint32      data_len = 0;
	char          *mime     = NULL;
	unsigned char *md5      = NULL;
	char          *root     = NULL;
	char          *path     = NULL;
	char          *hpath    = NULL;
	ft_uint32      size     = 0;
	ft_uint32      mtime    = 0;
	Dataset       *meta     = NULL;

	/* we no longer support the merge method */
	assert (file == NULL);

	if (!(rec = sdb_record_get (db, 0)))
		return NULL;

	rec_len = rec->written;

	/* this code block SUCKS!!!!!!!! */
	while (sdb_record_read (rec,
	                        &keyt, &key, &key_len,
	                        &datat, &data, &data_len))
	{
		char      *key_str   = NULL;
		char      *data_str  = NULL;
		ft_uint32  data_ui32 = 0;

		switch (keyt)
		{
		 case SDBRECORD_UINT32:
			break;
		 case SDBRECORD_STR:
			key_str = (char *) key;

			if (key_len <= 0 || key_str[key_len - 1] != 0)
				key_str = NULL;

			break;
		}

		if (!key_str)
		{
			TRACE (("invalid db key %p", key));
			continue;
		}

		switch (datat)
		{
		 case SDBRECORD_UINT32:
			if (data_len == sizeof (data_ui32))
				data_ui32 = P_INT (data);
			break;
		 case SDBRECORD_STR:
			data_str = (char *) data;

			/* make sure the sentinel is present */
			if (data_len <= 0 || data_str[data_len - 1] != 0)
				data_str = NULL;

			break;
		}

		/* someone shoot me for this, please */
		if (!strcasecmp (key_str, "md5") && data_len == 16)
			md5 = data;
		else if (!strcasecmp (key_str, "path"))
			path = data_str;
		else if (!strcasecmp (key_str, "hpath"))
			hpath = data_str;
		else if (!strcasecmp (key_str, "mime"))
			mime = data_str;
		else if (!strcasecmp (key_str, "root"))
			root = data_str;
		else if (!strcasecmp (key_str, "size"))
			size = data_ui32;
		else if (!strcasecmp (key_str, "mtime"))
			mtime = data_ui32;
		else if (data_str && !strncasecmp (key_str, "X-", 2))
		{
			dataset_insertstr (&meta, key_str + 2, data_str);
			free (data);
		}
		else
		{
			/* wth is this? */
			free (data);
		}

		free (key);
	}

	rec_start = rec->start;
	sdb_record_free (rec);

	/* this takes care of memory cleanup as well as the logic for
	 * both new file imports and sdata merging from disk */
	file = merge_file (root, path, hpath, mime, (off_t) size,
	                   (time_t) mtime, md5, meta);

	/* lying son of a bitch */
	if (!share_complete (file))
	{
		share_free (file);
		return NULL;
	}

	return file;
}

