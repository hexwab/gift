/*
 * $Id: gt_share.c,v 1.35 2004/03/26 11:53:18 hipnod Exp $
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
#include "sha1.h"

#include "gt_share.h"
#include "gt_share_file.h"
#include "gt_query_route.h"
#include "gt_search.h"
#include "gt_search_exec.h"

#include "encoding/url.h"

/******************************************************************************/

#define  SHARE_DEBUG          gt_config_get_int("share/debug=0")

/*****************************************************************************/

/* each share is assigned an index here */
static Dataset *indices;

/* maps binary sha1 hashes -> FileShares */
static Dataset *sha1_hashes;

/* stored index of the last index assigned */
static uint32_t index_counter;

/* whether shares have been completely synchronized yet */
static BOOL     sync_begun;

/* if shares are currently being synced */
static BOOL     sync_done;

/******************************************************************************/

static void add_hash    (FileShare *file);
static void remove_hash (FileShare *file);

/******************************************************************************/

/*
 * Find the old index, using the SHA1 hash as a key.
 */
static uint32_t get_old_index (Hash *hash)
{
	Share   *old_share;
	uint32_t index     = 0;

	old_share = dataset_lookup (sha1_hashes, hash->data, SHA1_BINSIZE);
	if (old_share != NULL)
	{
		GtShare *gt_share = share_get_udata (old_share, GT->name);

		if (gt_share)
			index = gt_share->index;
	}

	return index;
}

/*
 * Find an unused index, but reuse the existing index if possible.
 */
static uint32_t get_share_index (Share *share)
{
	Hash     *hash;
	uint32_t  index;

	if ((hash = share_get_hash (share, "SHA1")) != NULL)
	{
		uint32_t hash_tmp;

		/* if a Share with the same hash has another index, re-use that one */
		if ((index = get_old_index (hash)) != 0)
			return index;

		memcpy (&hash_tmp, hash->data, 4);

		/* use the first 24 bits of the SHA1 hash to seed the file index */
		index_counter = hash_tmp & 0xfffffff;
	}

	if (!(index = dataset_uniq32 (indices, &index_counter)))
		return 0;

	return index;
}

static void add_index (Share *share, GtShare *gt_share)
{
	uint32_t index;

	if (SHARE_DEBUG)
		GT->dbg (GT, "++[%d]->%s", gt_share->index, gt_share->filename);

	index = get_share_index (share);
	dataset_insert (&indices, &index, sizeof(index), share, 0);
}

static void remove_index (Share *share, GtShare *gt_share)
{
	uint32_t index = gt_share->index;

	assert (index > 0);

	/*
	 * Check if the index is pointing at a different share.  This case happens
	 * for every Share that is not removed on a resync, due to the weird way
	 * giftd 0.11.x use the new/add/remove/free interface.
	 */
	if (dataset_lookup (indices, &index, sizeof(index)) != share)
		return;

	if (SHARE_DEBUG)
		GT->dbg (GT, "--[%d]->%s", gt_share->index, gt_share->filename);

	index = gt_share->index;
	dataset_remove (indices, &index, sizeof(index));

	if (dataset_length (indices) == 0)
	{
		dataset_clear (indices);
		indices = NULL;
	}
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
	GtShare *share;
	uint32_t index;

	if (share_get_udata (file, GT->name))
		return NULL;

	index = get_share_index (file);

	if (!(share = gt_share_new_data (file, index)))
		return NULL;

	add_hash (file);
	add_index (file, share);

	return share;
}

static void gt_share_local_remove (FileShare *file, GtShare *share)
{
	remove_index (file, share);
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

	if (!file || !(share = share_get_udata (file, GT->name)))
		return DS_CONTINUE;

	if (share->index == *index &&
	    (!filename || !strcmp (filename, share->filename)))
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
	/* add this share to the data structures for searching */
	gt_search_exec_add (file);

	return gt_share_local_add (file);
}

void gnutella_share_free (Protocol *p, FileShare *file, void *data)
{
	/* remove data structures for searching */
	gt_search_exec_remove (file);

	gt_share_local_remove (file, data);
}

int gnutella_share_add (Protocol *p, FileShare *file, void *data)
{
	/* add to query routing tables */
	gt_query_router_self_add (file);

	return TRUE;
}

int gnutella_share_remove (Protocol *p, FileShare *file, void *data)
{
	/* remove from query routing tables */
	gt_query_router_self_remove (file);

	return TRUE;
}

void gnutella_share_sync (Protocol *p, int begin)
{
	gt_query_router_self_sync (begin);

	if (begin)
	{
		sync_begun = TRUE;
	}
	else if (sync_begun)
	{
		sync_begun = FALSE;
		sync_done = TRUE;

		/* synchronize the search structures (possibly, to disk) */
		gt_search_exec_sync ();
	}
}
