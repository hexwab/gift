/*
 * $Id: file_cache.c,v 1.5 2003/04/25 12:38:03 hipnod Exp $
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

#include "file.h"
#include "file_cache.h"

/*****************************************************************************/

FileCache *file_cache_new (char *file)
{
	FileCache *file_cache;

	if (!(file_cache = malloc (sizeof (FileCache))))
		return NULL;

	memset (file_cache, 0, sizeof (FileCache));

	file_cache->file = STRDUP (file);

	if (!file_cache_load (file_cache))
		TRACE (("failed loading %s", file));

	return file_cache;
}

void file_cache_free (FileCache *cache)
{
	dataset_clear (cache->d);

	free (cache->file);
	free (cache);
}

/*****************************************************************************/

int file_cache_load (FileCache *cache)
{
	struct stat st;
	time_t      mtime;
	char       *line  = NULL;
	FILE       *f;
	int         nlines;

	if (!(f = fopen (cache->file, "r")))
	{
		TRACE (("couldnt open %s for reading: %s", cache->file,
		        GIFT_STRERROR ()));
		return FALSE;
	}

	mtime = 0;

	if (file_stat (cache->file, &st))
		mtime = st.st_mtime;

	dataset_clear (cache->d);

	cache->d     = dataset_new (DATASET_HASH);
	cache->mtime = mtime;

	nlines = 0;

	while (file_read_line (f, &line))
	{
		char *key;
		char *value = line;

		key = string_sep (&value, " ");

		string_trim (key);
		string_trim (value);

		if (!key)
			continue;

		if (!value)
			value = "";

		dataset_insertstr (&cache->d, key, value);

		nlines++;
	}

	TRACE (("loaded filecache for %s. nlines = %i", cache->file, nlines));
	return TRUE;
}

static int sync_one (Dataset *d, DatasetNode *node, FILE *f)
{
	char *key   = node->key;
	char *value = node->value;

	fprintf (f, "%s %s\n", key, value);

	return FALSE;
}

void file_cache_sync (FileCache *cache)
{
	FILE  *f;
	char   tmp_path[PATH_MAX];

	/* ack, cant use stringf () here because it uses static data,
	 * and this gets called from a child thread */
	snprintf (tmp_path, sizeof (tmp_path), "%s.tmp", cache->file);

	if (!(f = fopen (tmp_path, "w")))
	{
		TRACE (("couldnt write to %s: %s", tmp_path, GIFT_STRERROR ()));
		return;
	}

	TRACE (("syncing %s to disk", tmp_path));
	TRACE (("size of data: %lu", dataset_length (cache->d)));

	dataset_foreach (cache->d, DATASET_FOREACH (sync_one), f);

	fclose (f);

	if (!file_mv (tmp_path, cache->file))
		TRACE (("file move %s -> %s failed", tmp_path, cache->file));
	else
		TRACE (("file move %s -> %s succeeded", tmp_path, cache->file));
}

/*****************************************************************************/

char *file_cache_lookup (FileCache *cache, char *key)
{
	char       *cache_file;
	char       *value;
	struct stat st;

	cache_file = cache->file;

	if ((value = dataset_lookupstr (cache->d, key)))
		return value;

	/* should clear the dataset if its non-empty here */
	if (!file_stat (cache_file, &st));
		return FALSE;

	/* reload the cache if the mtime changed */
	if (st.st_mtime != cache->mtime)
		file_cache_load (cache);

	return dataset_lookupstr (cache->d, key);
}

void file_cache_insert (FileCache *cache, char *key, char *value)
{
	dataset_insertstr (&cache->d, key, value);
}

void file_cache_remove (FileCache *cache, char *key)
{
	dataset_removestr (cache->d, key);
}
