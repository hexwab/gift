/*
 * $Id: share_db.c,v 1.24 2003/05/04 06:55:49 jasta Exp $
 *
 * TODO: Add versioning in the headers so that giFT knows when the shares
 * need to be completely rebuilt.
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "share_file.h"
#include "file.h"
#include "meta.h"

#include "share_db.h"

/*****************************************************************************/

#define MAX_RECSIZE 4096               /* total size of a single share entry */

/*****************************************************************************/

/**
 * Main API handle.  Provided opaquely from share_db.h and defined here.
 */
struct sdb_t
{
	char *path;                        /**< Full path to the opened db */
	FILE *f;                           /**< Opened file handle */
}; /* SDB */

/**
 * Simple record handle to simplify the internal API.
 */
typedef struct
{
	SDB          *sdb;                 /**< Parent db handle */
	unsigned char data[MAX_RECSIZE];   /**< Record data */
	size_t        offs;                /**< Data offset in lieu of a ptr */
	size_t        len;                 /**< Data length */
} sdbrec_t;

/**
 * Accessor macro for cleanliness only.  rec->sdb->f looks ugly.
 */
#define RECDB(rec) ((rec)->sdb)

/*****************************************************************************/

static SDB *sdb_new (char *path)
{
	SDB *sdb;

	if (!(sdb = MALLOC (sizeof (SDB))))
		return NULL;

	sdb->path = STRDUP (path);

	return sdb;
}

static void sdb_free (SDB *sdb)
{
	if (!sdb)
		return;

	free (sdb->path);
	free (sdb);
}

/*****************************************************************************/

SDB *sdb_open (char *path, char *mode)
{
	SDB *sdb;

	if (!(sdb = sdb_new (path)))
		return NULL;

	if (!(sdb->f = file_open (path, mode)))
	{
		sdb_close (sdb);
		return NULL;
	}

	return sdb;
}

SDB *sdb_create (char *path, char *mode)
{
	FILE *f;

	if (!(f = file_open (path, mode)))
		return NULL;

	file_close (f);

	return sdb_open (path, mode);
}

void sdb_close (SDB *sdb)
{
	if (!sdb)
		return;

	file_close (sdb->f);
	sdb_free (sdb);
}

/*****************************************************************************/

static int rec_init (sdbrec_t *rec, SDB *sdb)
{
	if (!sdb)
		return FALSE;

	rec->sdb     = sdb;
	rec->data[0] = 0;
	rec->offs    = 0;
	rec->len     = 0;

	return TRUE;
}

/*****************************************************************************/

static size_t rec_write (sdbrec_t *rec, void *buf, size_t len)
{
	if (rec->offs + len > sizeof (rec->data))
		return 0;

	/* use both rec->offs and rec->len for consistency, it could be done with
	 * simply rec->len */
	memcpy (rec->data + rec->offs, buf, len);
	rec->offs += len;
	rec->len += len;

	return len;
}

/* API consistency, readability, whatever */
static size_t rec_writestr (sdbrec_t *rec, char *str)
{
	return rec_write (rec, str, STRLEN_0 (str));
}

static int rec_wrbegin (sdbrec_t *rec)
{
	/* API hook */
	return TRUE;
}

static int rec_wrfinish (sdbrec_t *rec)
{
	size_t n;

	assert (rec->len > 0);

	/* write the record length first */
	if ((n = fwrite (&rec->len, sizeof (rec->len), 1, RECDB(rec)->f)) <= 0)
	{
		GIFT_ERROR (("error writing record header: %s", GIFT_STRERROR()));
		return FALSE;
	}

	/* write the record data */
	n = fwrite (rec->data, sizeof (char), rec->len, RECDB(rec)->f);
	if (n < rec->len)
	{
		GIFT_ERROR (("error committing record: %s", GIFT_STRERROR()));
		return FALSE;
	}

	return TRUE;
}

static int write_hash (Dataset *d, DatasetNode *node, sdbrec_t *rec)
{
	Hash *hash = node->value;
	char *key;

	key = stringf ("H-%s", node->key);
	assert (key != NULL);

	rec_writestr (rec, key);
	rec_write    (rec, &hash->len, sizeof (hash->len));
	rec_write    (rec, hash->data, hash->len);

	return FALSE;
}

static int write_meta (Dataset *d, DatasetNode *node, sdbrec_t *rec)
{
	char *key;

	key = stringf ("X-%s", node->key);
	assert (key != NULL);

	rec_writestr (rec, key);
	rec_writestr (rec, node->value);

	return FALSE;
}

int sdb_write (SDB *sdb, FileShare *file)
{
	sdbrec_t   rec;
	ShareData *sdata;

	if (!sdb || !file)
		return FALSE;

	sdata = SHARE_DATA(file);
	assert (sdata != NULL);

	rec_init (&rec, sdb);
	rec_wrbegin (&rec);

	rec_write    (&rec, &sdata->mtime, sizeof (sdata->mtime));
	rec_write    (&rec, &file->size,   sizeof (file->size));
	rec_writestr (&rec,  file->mime);
	rec_writestr (&rec,  sdata->root);
	rec_writestr (&rec,  sdata->path);

	/* write the block of hashes */
	share_hash_foreach (file, DATASET_FOREACH(write_hash), &rec);
	rec_writestr (&rec, "");

	/* write the block of meta data */
	meta_foreach (file, DATASET_FOREACH(write_meta), &rec);
	rec_writestr (&rec, "");

	return rec_wrfinish (&rec);
}

/*****************************************************************************/

static size_t rec_read (sdbrec_t *rec, void *buf, size_t len)
{
	if (rec->offs + len > rec->len)
		return 0;

	memcpy (buf, rec->data + rec->offs, len);
	rec->offs += len;

	return len;
}

/* special strlen function design for safety within the parameters of the
 * record type */
static size_t rec_readstrlen (sdbrec_t *rec)
{
	char  *begin;
	char  *end;
	size_t len = 0;

	begin = rec->data + rec->offs;
	end   = rec->data + rec->len;

	/* here is the slow part */
	for (; *begin; begin++)
	{
		/* here is the safe part */
		if (begin >= end)
			return 0;

		len++;
	}

	/* the entire string length includes the \0 that was written to the
	 * data buffer */
	return len + 1;
}

static size_t rec_readstr (sdbrec_t *rec, char **str)
{
	size_t len;

	/* efficiently avoid a copy */
	*str = rec->data + rec->offs;

	/* safely determine how many bytes to move beyond */
	len = rec_readstrlen (rec);
	rec->offs += len;

	return len;
}

static int rec_rdbegin (sdbrec_t *rec)
{
	size_t n;
	size_t size = 0;

	if ((n = fread (&size, sizeof (size), 1, RECDB(rec)->f)) <= 0)
		return FALSE;

	/* naughty naughty shares file */
	if (size > MAX_RECSIZE)
		return FALSE;

	if ((n = fread (rec->data, sizeof (char), size, RECDB(rec)->f)) < size)
		return FALSE;

	rec->len = size;

	return TRUE;
}

static int rec_rdfinish (sdbrec_t *rec)
{
	/* make sure we handled the exact number of bytes read from the file */
	assert (rec->offs == rec->len);
	return TRUE;
}

static int read_hash (sdbrec_t *rec, FileShare *file)
{
	char          *key;
	unsigned char *data;
	size_t         len = 0;

	/* the list of hashes is terminated by a single empty key, so we can
	 * safely loop through all keys until sentinel is reached */
	while ((rec_readstr (rec, &key)) && *key)
	{
		if (strncmp (key, "H-", 2))
			continue;

		/* read the hash length that follows */
		rec_read (rec, &len, sizeof (len));

		/* allocate storage for the hash as our little NUL-terminated
		 * pointer trick with regular strings will not work here */
		data = malloc (len);
		assert (data != NULL);

		/* read the actual hash */
		rec_read (rec, data, len);

		share_hash_set (file, key + 2, data, len);
	}

	return TRUE;
}

static int read_meta (sdbrec_t *rec, FileShare *file)
{
	char *key;

	while ((rec_readstr (rec, &key)) && *key)
	{
		char *value;

		if (strncmp (key, "X-", 2))
			continue;

		if (!rec_readstr (rec, &value))
			break;

		meta_set (file, key + 2, value);
	}

	return TRUE;
}

FileShare *sdb_read (SDB *sdb)
{
	sdbrec_t   rec;
	FileShare *file;
	time_t     mtime;
	off_t      size;
	char      *mime;
	char      *root;
	char      *path;

	rec_init (&rec, sdb);

	if (!rec_rdbegin (&rec))
		return NULL;

	rec_read    (&rec, &mtime, sizeof (mtime));
	rec_read    (&rec, &size, sizeof (size));
	rec_readstr (&rec, &mime);
	rec_readstr (&rec, &root);
	rec_readstr (&rec, &path);

	if ((file = share_new (NULL, root, STRLEN(root), path, mime, size, mtime)))
	{
		/* rebuild hashes */
		read_hash (&rec, file);

		/* rebuild meta data */
		read_meta (&rec, file);
	}

	rec_rdfinish (&rec);

	return file;
}

/*****************************************************************************/

#if 0
/*
 * Simple interface for reading a shares database file.
 *
 * Build with:
 * gcc -o share_db share_db.c share_file.c share_hash.c meta*.c mime.c -g
 *   -Wall -DHAVE_CONFIG_H -I.. -I../lib -I../plugin -lgiFT -lvorbisfile
 *   -lvorbis -lid3
 */
int main (int argc, char **argv)
{
	FileShare *file;
	SDB       *sdb;

	if (argc < 2)
		return 1;

	libgift_init ("share_db", GLOG_STDERR, NULL);

	if (!(sdb = sdb_open (argv[1], "rb")))
	{
		printf ("cannot open %s", argv[1]);
		return 1;
	}

	while ((file = sdb_read (sdb)))
		printf ("%s\n", SHARE_DATA(file)->path);

	sdb_close (sdb);

	libgift_finish ();

	return 0;
}
#endif
