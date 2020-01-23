/*
 * share_comp.c - writes a compacted (and possibly compressed) database of
 * locally shared files for submission
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
#include "share_comp.h"
#include "md5.h"

#ifdef USE_ZLIB
#include <zlib.h>
#endif

/*****************************************************************************/

/* path of the compressed shares database */
static char      *uncomp_path      = NULL;
static char      *comp_path        = NULL;

/*****************************************************************************/

#ifdef USE_ZLIB

/* TODO -- mmap */
static int gz_compress (FILE *f_in, gzFile f_out)
{
	char buf[RW_BUFFER];
	int  len;
	int  ret = FALSE;
	int  err;

	for (;;)
	{
		if (!f_in || !f_out)
			break;

		if ((len = fread (buf, sizeof (char), sizeof (buf), f_in)) < 0)
		{
			GIFT_ERROR (("fread: %s", GIFT_STRERROR ()));
			break;
		}

		if (len == 0)
		{
			ret = TRUE;
			break;
		}

		if (gzwrite (f_out, buf, (unsigned int) len) != len)
		{
			GIFT_ERROR (("gzwrite failed: %s", gzerror (f_out, &err)));
			break;
		}
	}

	/* cleanup for the caller */
	if (f_in)
		fclose (f_in);
	if (f_out)
		gzclose (f_out);

	return ret;
}

static char *file_compress (char *path)
{
	FILE  *f_in;
	gzFile f_out;
	char  *path_out;
	int    ret;

	/* open input */
	if (!(f_in = fopen (path, "rb")))
		return NULL;

	/* add gz */
	path_out = malloc (strlen (path) + 4);
	sprintf (path_out, "%s.gz", path);

	/* open output */
	f_out = gzopen (path_out, "wb6");

	/* this function closes inputs for us */
	if (!(ret = gz_compress (f_in, f_out)))
	{
		free (path_out);
		return NULL;
	}

	return path_out;
}

/*****************************************************************************/

static int gz_uncompress (gzFile f_in, FILE *f_out)
{
	char buf[RW_BUFFER];
	int  len;
	int  ret = FALSE;
	int  err;

	for (;;)
	{
		if (!f_in || !f_out)
			break;

		len = gzread (f_in, buf, sizeof (buf));

		if (len < 0)
		{
			GIFT_ERROR (("gzread failed: %s", gzerror (f_in, &err)));
			break;
		}

		if (len == 0)
		{
			ret = TRUE;
			break;
		}

		if (fwrite (buf, sizeof (char), (unsigned int) len, f_out) != len)
		{
			GIFT_ERROR (("fwrite: %s", GIFT_STRERROR () ));
			break;
		}
	}

	if (f_in)
		gzclose (f_in);
	if (f_out)
		fclose (f_out);

	return ret;
}

static char *file_uncompress (char *path)
{
	gzFile f_in;
	FILE  *f_out;
	char  *path_out;
	char  *path_ptr;
	int    ret;

	/* open input */
	if (!(f_in = gzopen (path, "rb")))
		return NULL;

	/* remove .gz or add .db */
	if ((path_ptr = strrchr (path, '.')))
		path_out = STRDUP_N (path, path_ptr - path);
	else
	{
		/* let's add a .db to the name, instead */
		path_out = malloc (strlen (path) + 4);
		sprintf (path_out, "%s.db", path_out);
	}

	/* open output */
	f_out = fopen (path_out, "wb");

	/* again, closes inputs for us */
	if (!(ret = gz_uncompress (f_in, f_out)))
	{
		free (path_out);
		return NULL;
	}

	return path_out;
}

#endif /* USE_ZLIB */

/*****************************************************************************/

/* checks whether ~/.giFT/OpenFT/shares is up to date with ~/.giFT/shares.
 * returns TRUE if no action needs to be performed */
static int comp_updated ()
{
	time_t gift_mtime   = 0;
	time_t comp_mtime   = 0;
	time_t uncomp_mtime = 0;

	/* no need to update it because the file doesnt even exist yet */
	if (!file_exists (gift_conf_path ("shares"), NULL, &gift_mtime))
		return TRUE;

	/* neither the comp path nor uncomp path exist, force a rebuild */
	if (!file_exists (comp_path, NULL, &comp_mtime) &&
		!file_exists (uncomp_path, NULL, &uncomp_mtime))
	{
		return FALSE;
	}

	if (comp_mtime)
		return (gift_mtime < comp_mtime);

	return (gift_mtime < uncomp_mtime);
}

static int flush_file (unsigned long key, FileShare *file, char *path)
{
	/* this takes care of everything for us ... hooray */
	ft_share_flush (file, path, FALSE);

	return TRUE;
}

/* writes ~/.giFT/OpenFT/shares.z for caching */
void share_comp_write ()
{
	Dataset *shares;

	if (comp_updated ())
		return;

	if (!uncomp_path)
		uncomp_path = STRDUP (gift_conf_path ("OpenFT/shares.db"));

	/* cleanup the db first */
	unlink (uncomp_path);

	/* oops, giFT isn't ready yet...back out */
	if (!(shares = share_index (NULL, NULL)))
		return;

	/* create the shares database */
	hash_table_foreach (shares, (HashFunc) flush_file, uncomp_path);

#ifdef USE_ZLIB
	if (comp_path)
	{
		unlink (comp_path);
#ifndef _MSC_VER
		/* why the f*ck is msvc barfing here? */
		free (comp_path);
#endif
	}

	comp_path = file_compress (uncomp_path);
#endif /* USE_ZLIB */
}

char *share_comp_path (int comp_req, int *comp)
{
	char *path = NULL;

	/* make sure it's updated */
	share_comp_write ();

	if (comp_req && file_exists (comp_path, NULL, NULL))
	{
		if (comp)
			*comp = TRUE;

		path = comp_path;
	}
	else if (file_exists (uncomp_path, NULL, NULL))
	{
		if (comp)
			*comp = FALSE;

		path = uncomp_path;
	}

	if (!path)
	{
		TRACE (("no shares path found...cry"));
	}

	return path;
}

/*****************************************************************************/

/* prepare path for reading...ie uncompress it */
char *share_comp_read (char *path)
{
	char *uncomp_path;

	assert (path);

#ifdef USE_ZLIB
	uncomp_path = file_uncompress (path);
#else
	uncomp_path = STRDUP (path);
#endif /* USE_ZLIB */

	return uncomp_path;
}
