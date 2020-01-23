/*
 * $Id: gt_share.c,v 1.25 2003/06/04 03:28:20 hipnod Exp $
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

#include "gt_gnutella.h"

#include "html.h"

#include "gt_share.h"
#include "gt_share_file.h"

#include "gt_query_route.h"

#include "sha1.h"

/******************************************************************************/

static void add_hash    (FileShare *file);
static void remove_hash (FileShare *file);

/******************************************************************************/

/* Keep track of each unique directory path and use that for the
 * file index. */
typedef struct gt_shared_path
{
	char         *dir;
	uint32_t      index;
	int           ref;
} shared_path_t;

/* Gnutella sends an index number with each search result, which nodes will
 * use to get a file if a push request is issued. Each share directory is
 * assigned an index number, and the ones currently in use are kept here. */
static Dataset *indices;

/* This maps the shared dir hpath -> shared_path_t */
static Dataset *paths;

/* Maps binary sha1 hashes -> FileShares */
static Dataset *sha1_hashes;

/* Index of the next shared dir that gets added. */
static uint32_t index_counter;

/* if shares are currently being synced */
static int      sync_done;

/* Whether shares have been completely synchronized yet. */
static int      sync_begun;

/******************************************************************************/

/*
 * These functions get called from p->share_new/free.
 */
static shared_path_t *shared_path_alloc (char *file)
{
	shared_path_t  *spath;

	if (!(spath = MALLOC (sizeof (shared_path_t))))
		return NULL;

	if (!(spath->dir = file_dirname (file)))
	{
		free (spath);
		return NULL;
	}

	spath->ref = 1;
	return spath;
}

static void shared_path_free (shared_path_t *spath)
{
	if (!spath)
		return;

	free (spath->dir);
	free (spath);
}

static shared_path_t *shared_path_find (FileShare *file)
{
	char           *dir;
	shared_path_t  *spath;

	if (!(dir = file_dirname (share_get_hpath (file))))
		return NULL;

	spath = dataset_lookup (paths, dir, STRLEN_0 (dir));
	free (dir);

	return spath;
}

static shared_path_t *add_shared_path (FileShare *file)
{
	shared_path_t  *spath;

	if ((spath = shared_path_find (file)))
	{
		spath->ref++;
		return spath;
	}

	if (!(spath = shared_path_alloc (share_get_hpath (file))))
		return NULL;

	if (!(spath->index = dataset_uniq32 (indices, &index_counter)))
	{
		shared_path_free (spath);
		return NULL;
	}

	if (!indices)
	{
		assert (paths == NULL);
		indices = dataset_new (DATASET_HASH);
		paths   = dataset_new (DATASET_HASH);
	}

	GT->dbg (GT, "++[%s]->[%u]", spath->dir, spath->index);

	dataset_insert (&paths, spath->dir, STRLEN_0 (spath->dir), spath, 0);
	dataset_insert (&indices, &spath->index, sizeof (spath->index), spath, 0);

	return spath;
}

static void remove_shared_path (FileShare *file)
{
	shared_path_t  *spath;

	if (!(spath = shared_path_find (file)))
	{
		GT->err (GT, "removing shared path that doesn't exist");
		return;
	}

	if (--spath->ref > 0)
		return;

	assert (dataset_lookup (indices, &spath->index, 
	                        sizeof (spath->index)) == spath);

	GT->dbg (GT, "--[%s]->[%u]", spath->dir, spath->index);

	dataset_remove (indices, spath->dir, STRLEN_0 (spath->dir));
	dataset_remove (indices, &spath->index, sizeof (spath->index));

	if (dataset_length (indices) == 0)
	{
		dataset_clear (indices);
		indices = NULL;
	}

	if (dataset_length (paths) == 0)
	{
		dataset_clear (paths);
		paths = NULL;
	}

	shared_path_free (spath);
}

/******************************************************************************/

/* TODO: This duplicates memory thats already stored on the FileShare.
 *       Prevent this by maintaining a per-hash algorithm map of FileShares */
static void add_hash (FileShare *file)
{
	Hash       *hash;
	ds_data_t   key;
	ds_data_t   value;

	if (!(hash = share_get_hash (file, "SHA1")))
		return;

	/* This shares the hash memory with the FileShare and also
	 * points directly at it. */
	ds_data_init (&key, hash->data, hash->len, DS_NOCOPY);
	ds_data_init (&value, file, 0, DS_NOCOPY);

	/* We share the key with the FileShare, so remove the old key first
	 * so we don't end up sharing an old FileShare's hash. */
	dataset_remove_ex (sha1_hashes, &key);
	dataset_insert_ex (&sha1_hashes, &key, &value);
}

static void remove_hash (FileShare *file)
{
	Hash       *hash;

	if (!(hash = share_get_hash (file, "SHA1")))
		return;

	/*
	 * If a FileShare is already present at this hash, and it isn't
	 * this FileShare, then don't remove it. This _will_ happen
	 * due to the way FileShares get added and removed on resyncs.
	 */
	if (dataset_lookup (sha1_hashes, hash->data, hash->len) != file)
		return;

	dataset_remove (sha1_hashes, hash->data, hash->len);

	if (dataset_length (sha1_hashes) == 0)
	{
		dataset_clear (sha1_hashes);
		sha1_hashes = NULL;
	}
}

/******************************************************************************/

static GtShare *gt_share_local_add (FileShare *file)
{
	GtShare        *share;
	shared_path_t  *spath;

	if (share_get_udata (file, gnutella_p->name))
		return NULL;

	/* keep track of the shared root as an index */
	spath = add_shared_path (file);
	assert (spath != NULL);

	add_hash (file);

	if (!(share = gt_share_new_data (file, spath->index)))
		return NULL;

	return share;
}

static void gt_share_local_remove (FileShare *file, GtShare *share)
{
	remove_shared_path (file);
	remove_hash (file);

	gt_share_free_data (file, share);
}

static int find_by_index (ds_data_t *key, ds_data_t *value, void **args)
{
	uint32_t    *index    = args[0];
	char        *filename = args[1];
	FileShare  **ret      = args[2];
	FileShare   *file     = value->data;
	GtShare     *share;

	if (!file || !(share = share_get_udata (file, gnutella_p->name)))
		return DS_CONTINUE;

	if (share->index == *index && !STRCMP (filename, share->filename))
	{
		*ret = file;
		return DS_BREAK;
	}

	return DS_CONTINUE;
}

FileShare *gt_share_local_lookup_by_index (uint32_t index, char *filename)
{
	FileShare *ret    = NULL;
	void      *args[] = { &index, filename, &ret };

	share_foreach (DS_FOREACH_EX(find_by_index), args);

	return ret;
}

static FileShare *lookup_sha1 (char *urn)
{
	char           *str, *str0;
	char           *prefix;
	unsigned char  *bin;
	FileShare      *file;

	if (!(str0 = str = STRDUP (urn)))
		return NULL;

	/* TODO: consolidate with gt_protocol.c:parse_extended_data */
	string_upper (str0);
	string_sep (&str, "URN:");

	prefix = string_sep (&str, ":");

	/* Only support urn:sha1 or urn:sha-1 urns now */
	if (STRCMP (prefix, "SHA1") != 0 && STRCMP (prefix, "SHA-1") != 0)
	{
		free (str0);
		return NULL;
	}

	string_trim (str);

	if (strlen (str) != 32)
	{
		free (str0);
		return NULL;
	}

	if (!(bin = sha1_bin (str)))
	{
		free (str0);
		return NULL;
	}

	file = dataset_lookup (sha1_hashes, bin, SHA1_BINSIZE);

	free (str0);
	free (bin);

	return file;
}

FileShare *gt_share_local_lookup_by_urn (char *urn)
{
	return lookup_sha1 (urn);
}

static char *get_sha1 (FileShare *file)
{
	Hash *hash;
	char *urn;
	char *str;

	if (!(hash = share_get_hash (file, "SHA1")))
		return NULL;

	assert (hash->len == SHA1_BINSIZE);

	if (!(str = sha1_string (hash->data)))
		return NULL;

	urn = stringf_dup ("urn:sha1:%s", str);
	free (str);

	return urn;
}

char *gt_share_local_get_urns (FileShare *file)
{
	char *urn;

	urn = get_sha1 (file);

	return urn;
}

int gt_share_local_sync_is_done (void)
{
	return sync_done;
}

/******************************************************************************/

void *gnutella_share_new (Protocol *p, FileShare *file)
{
	return gt_share_local_add (file);
}

void gnutella_share_free (Protocol *p, FileShare *file, void *data)
{
	gt_share_local_remove (file, data);
}

int gnutella_share_add (Protocol *p, FileShare *file, void *data)
{
	GtShare *share;
	uint32_t index;

	share = data;
	index = share->index;

	/* add to query routing tables */
	gt_query_router_self_add (file);

	/* add sha1 hash */
	add_hash (file);

	return TRUE;
}

int gnutella_share_remove (Protocol *p, FileShare *file, void *data)
{
	GtShare *share;

	share = data;

	/* remove from query routing tables */
	gt_query_router_self_remove (file);

	return TRUE;
}

void gnutella_share_sync (Protocol *p, int begin)
{
	if (begin)
	{
		sync_begun = TRUE;
	}
	else if (sync_begun)
	{
		sync_begun = FALSE;
		sync_done = TRUE;
	}
}
