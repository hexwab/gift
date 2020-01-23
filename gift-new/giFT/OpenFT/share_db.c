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

#include "openft.h"

#include "file.h"

#include "share_db.h"

/*****************************************************************************/

/* flush data to disk from ram when this ticks */
#define SHARE_DB_TIMER (5 * MINUTES)

/*****************************************************************************/

static char *db_path (FT_HostShare *h_share)
{
	char *path;

	path =
		gift_conf_path ("OpenFT/db/%s", net_ip_str (h_share->host));

	return path;
}

/*****************************************************************************/

static ft_uint32 read_int (FILE *f, size_t integer_size)
{
	ft_uint32 integer;

	assert (integer_size == 4);

	if (fread (&integer, integer_size, 1, f) <= 0)
		return FALSE;

	return ntohl (integer);
}

static size_t write_int (FILE *f, ft_uint32 integer, size_t integer_size)
{
	size_t written;

	assert (integer_size == 4);

	/* network order.  these databases may go over the socket */
	integer = htonl (integer);
	written = fwrite (&integer, integer_size, 1, f);

	return written;
}

/*****************************************************************************/

/* TODO -- file.c BADLY needs to incorporate better file_read/write wrappers
 * for error output */

/* verify that this is an OpenFT shares database :) */
static int verify_db (FILE *f, size_t *nrec)
{
	ft_uint32 recs = 0;
	char      db_check[6];
	size_t    n;

	/* make sure we're positioned at the beginning of the file */
	if (fseek (f, 0, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR ()));
		return FALSE;
	}

	/* read the signature */
	n = fread (db_check, sizeof (char), sizeof (db_check), f);
	if (n <= 0)
	{
		GIFT_ERROR (("fread: %s", GIFT_STRERROR()));
		return FALSE;
	}

	if (strncmp (db_check, "OpenFT", sizeof (db_check)))
	{
		TRACE (("invalid database file"));
		return FALSE;
	}

	/* retrieve the number of records in this db */
	recs = read_int (f, sizeof (recs));

	if (nrec)
		*nrec = recs;

	/* file pointer is now positioned at the first record */

	return TRUE;
}

static FILE *open_db (char *path, char *mode, size_t *nrec)
{
	FILE *f;

	if (!(f = fopen (path, mode)))
		return NULL;

	if (!verify_db (f, nrec))
		return NULL;

	return f;
}

static FILE *create_db (char *path, char *mode, size_t *nrec)
{
	FILE      *f;
	ft_uint32  recs = 0;

	TRACE (("%s", path));

	if (!(f = fopen (path, "w")))
	{
		GIFT_ERROR (("Can't open %s: %s", path, GIFT_STRERROR()));
		return NULL;
	}

	fwrite ("OpenFT", sizeof (char), 6, f);
	write_int (f, recs, sizeof (recs));

	fclose (f);

	return open_db (path, mode, nrec);
}

static void unlink_db (char *path)
{
	unlink (path);
}

/*****************************************************************************/

static off_t new_record_offset (FILE *f, FileShare *file, FT_Share *share)
{
	off_t db_offs = 0;

	if (!file_stat (fileno (f), &db_offs, NULL))
		return FALSE;

	return db_offs;
}

/* RECORD (host order):
 *  4 bytes   - length of record
 *  4 bytes   - size of file
 *  4 bytes   - length of path
 *  str\0     - path
 *  4 bytes   - hpath length
 *  4 bytes   - length of md5
 *  str\0     - md5
 */
static int write_record (FILE *f, FileShare *file, FT_Share *share)
{
	size_t    written = 0;
	off_t     db_offs;
	ft_uint32 size;
	ft_uint32 rec_size = 0;
	ft_uint32 hpath_len = 0;
	ft_uint32 path_len;
	ft_uint32 md5_len;

	/* lots of shit can happen here ;) */
	if (!f || !file || !share ||
	    !file->sdata || !file->sdata->path || !file->sdata->md5)
	{
		return FALSE;
	}

	/* locate this shares offset in the database */
	if (!(db_offs = new_record_offset (f, file, share)))
		return FALSE;

	if (fseek (f, db_offs, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
		return FALSE;
	}

	/* determine total record size */
	path_len = ((file->sdata->hpath) ?
	            strlen (file->sdata->hpath) : strlen (file->sdata->path)) + 1;
	md5_len  = strlen (file->sdata->md5)  + 1;
	rec_size = sizeof (size) +
	           sizeof (path_len) + path_len +
	           sizeof (hpath_len) +
	           sizeof (md5_len) + md5_len;

	/* write the record size */
	written += write_int (f, rec_size, sizeof (rec_size));

	/* file->size */
	size = (ft_uint32) file->size;
	written += write_int (f, size, sizeof (size));

	/* file->path or file->hpath */
	written += write_int (f, path_len, sizeof (path_len));
	written += fwrite (file->sdata->hpath ? file->sdata->hpath : file->sdata->path,
	                   sizeof (char), path_len, f);

#if 0
	/* file->hpath */
	if (file->hpath)
		hpath_len = file->hpath - file->path;
#endif

	written += write_int (f, hpath_len, sizeof (hpath_len));

	/* file->md5 */
	written += write_int (f, md5_len, sizeof (md5_len));
	written += fwrite (file->sdata->md5, sizeof (char), md5_len, f);

	/* int writes only return 1 for their item count */
	if (written < rec_size - 16)
	{
		GIFT_ERROR (("database corruption occurred...you suck"));
		return FALSE;
	}

	return db_offs;
}

static size_t inc_record (FILE *f, FileShare *file, FT_Share *share,
						  size_t nrec_curr)
{
	ft_uint32 nrec;

	nrec = nrec_curr + 1;

	if (fseek (f, 6, SEEK_SET) == -1)
		return nrec_curr;

	write_int (f, nrec, sizeof (nrec));

	return nrec;
}

/* flush this share out of memory and insert it into the database
 * TODO -- optimize this for multiple transactions */
int ft_share_flush (FileShare *file, char *path, int purge)
{
	FT_Share *share;
	FILE     *f;
	off_t     db_offs;
	size_t    nrec = 0;

	/* this data is being referenced somewhere, we cannot flush it out */
	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	if (!path)
		path = db_path (share->host_share);

	/* do not purge from mem if this data is actively used */
	if (purge && file->ref > 1)
		return FALSE;

	if (file->flushed)
		return TRUE;

	if (!(f = open_db (path, "rb+", &nrec)))
	{
		if (!(f = create_db (path, "rb+", &nrec)))
			return FALSE;
	}

	db_offs = write_record (f, file, share);

	nrec = inc_record (f, file, share, nrec);

	fclose (f);

	if (!db_offs)
		return FALSE;

	if (purge)
	{
		/* reset the data in memory */
		free (file->sdata->root);
		free (file->sdata->path);
		free (file->sdata->md5);
		free (file->sdata);
		file->sdata = NULL;

		/* TODO -- if we're not purging, should we still set flushed? */
		file->flushed = db_offs;
	}

	return TRUE;
}

/*****************************************************************************/

static int read_str (FILE *f, char **str)
{
	ft_uint32 str_len;

	/* odd. */
	if (*str)
	{
		TRACE (("*str = %p???", *str));
		free (*str);
		*str = NULL;
	}

	if (!(str_len = read_int (f, sizeof (str_len))) || str_len > 8192)
	{
		if (str_len > 8192)
			TRACE (("str_len = %u", str_len));

		return FALSE;
	}

	if (!(*str = malloc (str_len)))
		return FALSE;

	if (fread (*str, sizeof (char), str_len, f) < str_len)
	{
		free (*str);
		*str = NULL;

		return FALSE;
	}

	return TRUE;
}

/* this function needs to call ft_share_new () directly due to the fact that
 * the function requirse all data be complete when the object is created.
 * this requires us to pass along the host share object.  it will only be
 * used when file is NULL */
static FileShare *read_record (FILE *f, FT_HostShare *h_share,
                               FileShare *file, FT_Share *share)
{
	char      *path  = NULL;
	char      *hpath = NULL;
	char      *md5   = NULL;
	ft_uint32  size      = 0;
	ft_uint32  rec_len   = 0;
	ft_uint32  hpath_len = 0;

	if (file)
	{
		/* move to the record we want */
		if (fseek (f, file->flushed, SEEK_SET) == -1)
		{
			GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
			return NULL;
		}
	}

	if (!(rec_len = read_int (f, sizeof (rec_len))))
		return NULL;

	/* TODO -- error checking */

	size = read_int (f, sizeof (size));

	read_str (f, &path);

	hpath_len = read_int (f, sizeof (hpath_len));
	hpath = ((hpath_len) ? path + hpath_len : NULL);

	read_str (f, &md5);

	/* ignore incomplete records */
	if (!path || !md5)
	{
		free (path);
		free (md5);

		return NULL;
	}

	if (!file)
	{
		file = ft_share_new (h_share, size, md5, path);

		/* ft_share_new duped for us */
		free (md5);
		free (path);
	}
	else
	{
		if (!file->sdata)
		{
			file->sdata = malloc (sizeof (ShareData));
			memset (file->sdata, 0, sizeof (ShareData));
		}

		/* make sure we don't leak if we are unflushing twice...for some
		 * strange reason :) */
		free (file->sdata->path);
		free (file->sdata->md5);

		file->sdata->path = path;
		file->sdata->md5  = md5;

#ifdef DEBUG
		/* zero the record */
		{
			unsigned char buf[4096] = { 0 };

			fseek (f, file->flushed, SEEK_SET);
			fwrite (&buf, sizeof (char), rec_len + sizeof (rec_len), f);
		}
#endif /* DEBUG */
	}

	/* lying son of a bitch */
	if (!ft_share_complete (file))
	{
		ft_share_free (file);
		return NULL;
	}

	file->sdata->root  = NULL;
#if 0
	file->sdata->hpath = NULL; /* hmm?  is this right? */
#endif

	return file;
}

static size_t dec_record (FILE *f, FileShare *file, FT_Share *share,
                          size_t nrec_curr)
{
	ft_uint32 nrec;

	if (nrec_curr == 0)
		return 0;

	nrec = nrec_curr - 1;

	if (fseek (f, 6, SEEK_SET) == -1)
		return nrec_curr;

	write_int (f, nrec, sizeof (nrec));

	return nrec;
}

/* pull this shares data back from the database :) */
int ft_share_unflush (FileShare *file, char *path)
{
	FILE     *f;
	FT_Share *share;
	size_t    nrec = 0;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	if (!path)
		path = db_path (share->host_share);

	/* reset the timeout to flush...give this share another 10 minutes :) */
	/* timer_reset (share->host_share->db_timeout); */

	/* that was easy */
	if (!file->flushed)
		return TRUE;

	if (!(f = open_db (path, "rb+", &nrec)))
		return FALSE;

	if (!read_record (f, NULL, file, share))
	{
		fclose (f);
		return FALSE;
	}

	/* decrement the total record count in the db */
	nrec = dec_record (f, file, share, nrec);

	fclose (f);

	/* db has now active entries, drop it */
	if (nrec == 0)
	{
		TRACE (("unlinking empty database %s", path));
		unlink_db (path);
	}

	file->flushed = FALSE;

	/* there are elements in this database that are not flushed now */
	share->host_share->db_flushed = FALSE;

	return TRUE;
}

/*****************************************************************************/

/* loads an entire database into a dataset
 * TODO -- do not require a host share object for parenting */
int ft_share_import (FT_HostShare *h_share, char *path)
{
	FILE      *f;
	FileShare *file;

	if (!h_share)
		return FALSE;

	if (!path)
	{
		/* this isn't supposed to occur yet */
		path = db_path (h_share);
		GIFT_WARN (("importing from %s", path));
	}

	if (!(f = open_db (path, "rb+", NULL)))
		return FALSE;

	/* TODO -- should we flush the list if it already exists? Should we at
	 * least bark? */
	if (!h_share->dataset)
		h_share->dataset = dataset_new ();

	while ((file = read_record (f, h_share, NULL, NULL)))
	{
		if (file->sdata && file->sdata->md5)
			dataset_insert (h_share->dataset, file->sdata->md5, file);
	}

	h_share->db_flushed = FALSE;

	fclose (f);

	return TRUE;
}

/*****************************************************************************/

static int share_flush_node (unsigned long key, FileShare *file, int *err)
{
	ft_share_flush (file, NULL, TRUE);

	return TRUE;
}

/* flush this hosts data out to the database if it is currently unref'd */
static int host_share_flush (FT_HostShare *h_share)
{
	int err = 0;

	/* if this flag is set we know that absolutely every share from this
	 * host is flushed...don't touch a thing :) */
	if (h_share->db_flushed)
		return TRUE;

	hash_table_foreach (h_share->dataset, (HashFunc) share_flush_node,
						&err);

	if (err)
	{
		GIFT_WARN (("some errors occurred while flushing..."
		            "not a clue what they were ;)"));
		return TRUE;
	}

	h_share->db_flushed = TRUE;

	return TRUE;
}

/* should be used when we know that this is a new host share going into the
 * ft_shares structure */
void ft_host_share_new_db (FT_HostShare *h_share)
{
	if (h_share->db_timeout)
	{
		TRACE (("db %p already exists", h_share));
		return;
	}

	/* once this timer runs out all data held at each leaf in the dataset will
	 * be written out to a database to conserve memory
	 * NOTE: this timer is never actually disabled so it wastes a little bit
	 * of overhead.  I don't feel like hunting down all the locations in the
	 * source that would need to have sanity checks... */
	h_share->db_timeout =
		timer_add (SHARE_DB_TIMER, (TimerCallback) host_share_flush, h_share);
	h_share->db_flushed = FALSE;

	/* make sure this database is flushed before we work with it */
	unlink_db (db_path (h_share));
}

void ft_host_share_del_db (FT_HostShare *h_share)
{
	unlink_db (db_path (h_share));
}
