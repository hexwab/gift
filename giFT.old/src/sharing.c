/*
 * sharing.c
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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <unistd.h>

#include <errno.h>

#include "gift.h"

#include "hash.h"

#include "parse.h"
#include "conf.h"

#include "file.h"
#include "mime.h"
#include "md5.h"
#include "meta.h"

#include "sharing.h"


/*****************************************************************************/

/**/extern Config *gift_conf;
static Dataset    *local_shares = NULL;
static List       *sroot        = NULL;

/* cache the size of local_shares so that it doesn't need to be calculated
 * everytime OpenFT wants it */
static unsigned long local_files = 0;
static double        local_size  = 0.0; /* as MB */

/*****************************************************************************/

/* number of bytes to read from the file to calculate the MD5 (0 means entire
 * file) */
#define MD5SUM_READ 307200

/*****************************************************************************/

static void destroy_sroot ()
{
	list_foreach_remove (sroot, NULL, NULL);
	sroot = NULL;
}

static void build_sroot ()
{
	char *sharing_root, *sharing_root0;
	char *root;

	destroy_sroot ();

	if (!(sharing_root = config_get_str (gift_conf, "sharing/root")))
	{
		TRACE (("no sharing root found!!  please run giFT-setup!"));
		return;
	}

	sharing_root0 = sharing_root = STRDUP (sharing_root);

	while ((root = string_sep (&sharing_root, ":")))
	{
		char *ptr;

		/* make sure that there are no leading /'s */
		for (ptr = root + strlen (root) - 1; *ptr == '/'; ptr--);

		/* if no / was found, ptr[1] will equal 0 anyway */
		ptr[1] = 0;

		sroot = list_append (sroot, STRDUP (root));
	}

	free (sharing_root0);
}

static char *calculate_root (char *filename)
{
	List *ptr;

	if (!filename)
		return NULL;

	build_sroot ();

	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		/* found root */
		if (!strncmp (filename, ptr->data, strlen (ptr->data)))
			return ptr->data;
	}

	return NULL;
}

/*****************************************************************************/

static char *calculate_hpath (char *root, char *filename)
{
	/* no root means we dont need an hpath */
	if (!root)
		return NULL;

	return (filename + strlen (root));
}

static char *calculate_ext (char *path)
{
	char *ext;

	if (!path)
		return NULL;

	if ((ext = strrchr (path, '.')))
		ext++;

	return ext;
}

FileShare *share_new (char *root, char *path, char *md5,
                      unsigned long size, time_t mtime)
{
	FileShare *file;

	file = malloc (sizeof (FileShare));
	memset (file, 0, sizeof (FileShare));

	/* i should probably have root handled as just a length into path, oh
	 * well */
	file->root  = STRDUP (root);
	file->path  = STRDUP (path);
	file->hpath = calculate_hpath (file->root, file->path);

	file->ext   = calculate_ext   (file->path);
	file->type  = mime_type       (file->path, file->ext);

	file->md5   = STRDUP (md5);
	file->size  = size;
	file->mtime = mtime;
	file->meta  = NULL;

	return file;
}

void share_free (FileShare *file)
{
	assert (file);

	dataset_clear (file->data);

	free (file->root);
	free (file->path);
	free (file->md5);
	free_metadata (file->meta);
	free (file);
}

/*****************************************************************************/

static int share_lookup_entry (char *md5, FileShare *file, char *path)
{
	if (!strcmp (file->path, path))
		return TRUE;

	return FALSE;
}

FileShare *share_lookup (char *path)
{
	FileShare *file;

	if (!path)
		return NULL;

	file = hash_table_find (local_shares, (HashFunc) share_lookup_entry, path);

	return file;
}

/*****************************************************************************/

int share_add (char *root, char *path, char *md5,
               unsigned long size, time_t mtime)
{
	FileShare *file;

	if (!local_shares)
		local_shares = dataset_new ();

	/* check to make sure its not already there */
	if ((file = dataset_lookup (local_shares, md5)))
		share_free (file);

	file = share_new (root, path, md5, size, mtime);

	dataset_insert (local_shares, md5, file);

	local_size += ((float)size / 1024.0 / 1024.0);

	return TRUE;
}

/*****************************************************************************/

static int share_clear_entry (char *md5, FileShare *file, void *udata)
{
	share_free (file);

	return TRUE;
}

void share_clear ()
{
	hash_table_foreach_remove (local_shares, (HashFunc) share_clear_entry,
	                           NULL);
	local_shares = NULL;
	local_files  = 0;
	local_size   = 0.0;
}

/*****************************************************************************/

static int add_share (FILE *f, char *root, char *path,
                      unsigned long size, time_t mtime)
{
	FileShare *share;
	char      *md5 = NULL;

	share = share_lookup (path);

	if (share && share->mtime == mtime)
		md5 = share->md5;

	if (!md5)
	{
		/* calculate md5 */
		if (!(md5 = md5_checksum (path, MD5SUM_READ)))
			return FALSE;

		share_add (root, path, md5, size, mtime);
	}

	fprintf (f, "%lu %s %lu %s\n", mtime, md5, size, path);

	return TRUE;
}

static int path_traverse (FILE *f, char *root, char *path)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	size_t         path_len = 0;
	int            total    = 0;

	if (!path)
		return FALSE;

	/* calculate the length here for optimization purposes */
	path_len = strlen (path);

	dir = opendir (path);
	if (!dir)
	{
		GIFT_WARN (("cannot open dir %s", path));
		return FALSE;
	}

	while ((d = readdir (dir)))
	{
		char *newpath;

		if (!d->d_name || !d->d_name[0])
			continue;

		/* ignore '.' and '..' */
		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		newpath = malloc (path_len + strlen (d->d_name) + 2);
		sprintf (newpath, "%s/%s", path, d->d_name);

		/*
		 * Might want to use lstat() instead of stat() or opendir(),
		 * because otherwise a symbolic link can cause a loop
		 * in directory structure.  Unfortunately, no symlinks in
		 * shares-dir then :-(
		 *
		 * NOTE <jasta> - there is a way to use lstat and stat, gnapster does
		 * it...but i cant remember how
		 */
		if (stat (newpath, &st) == -1)
		{
			TRACE (("cannot open %s: %s", newpath, strerror (errno)));
			free (newpath);
			continue;
		}

		if (S_ISDIR (st.st_mode))
			total += path_traverse (f, root, newpath);
		else if (S_ISREG (st.st_mode))
			total += add_share (f, root, newpath, st.st_size, st.st_mtime);

		free (newpath);
	}

	closedir (dir);

	return total;
}

Dataset *share_build_index ()
{
	FILE *f;
	char *buf = NULL;

	TRACE_FUNC ();

	if (!(f = fopen (gift_conf_path ("shares"), "r")))
	{
		TRACE (("*** creating new shares file"));
		return NULL;
	}

	share_clear ();

	while (file_read_line (f, &buf))
	{
		char *pos = buf;
		char *md5, *filename;
		unsigned long size;
		time_t        mtime;

		trim_whitespace (pos);

		mtime    = ATOUL (string_sep (&pos, " "));
		md5      =        string_sep (&pos, " ");
		size     = ATOUL (string_sep (&pos, " "));
		filename = pos;

		if (!filename)
			continue;

		share_add (calculate_root (filename), filename, md5, size, mtime);
	}

	fclose (f);

	return local_shares;
}

/*****************************************************************************/

Dataset *share_update_index ()
{
	FILE *f;
	List *ptr;
	int   total = 0;

	TRACE_FUNC ();

	build_sroot ();
	share_build_index ();

	f = fopen (gift_conf_path ("shares"), "w");

	for (ptr = sroot; ptr; ptr = list_next (ptr))
	{
		char *root = ptr->data;

		TRACE (("descending %s...", root));
		total += path_traverse (f, root, root);
	}

	fclose (f);

	local_files = total;
	TRACE (("total shares: %i (%.02fMB)", total, local_size));

	return local_shares;
}

void share_clear_index ()
{
	share_clear ();
}

/*****************************************************************************/

Dataset *share_index (unsigned long *files, double *size)
{
	if (files)
		*files = local_files;
	if (size)
		*size = local_size;

	return local_shares;
}

/*****************************************************************************/

static int index_sort_cmp (FileShare *a, FileShare *b)
{
	return strcmp (a->path, b->path);
}

/* WARNING!  This is slow as hell! */
List *share_index_sorted ()
{
	List *index;

	index = list_sort (hash_flatten (local_shares),
	                   (CompareFunc) index_sort_cmp);

	return index;
}

/*****************************************************************************/

void share_foreach (HashFunc foreach_func, void *data)
{
	if (!local_shares)
		return;

	hash_table_foreach (local_shares, foreach_func, data);
}
