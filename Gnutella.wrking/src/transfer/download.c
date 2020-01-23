/*
 * $Id: download.c,v 1.2 2004/04/17 06:06:46 hipnod Exp $
 *
 * Copyright (C) 2004 giFT project (gift.sourceforge.net)
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
#include "transfer/download.h"

/*****************************************************************************/

static Dataset *gt_downloads;

/*****************************************************************************/

void gt_download_add (Transfer *transfer, Source *source)
{
	Dataset *d;

	d = dataset_lookup (gt_downloads, &transfer, sizeof(transfer));
	dataset_insert (&d, &source, sizeof(source), source, 0);

	dataset_insert (&gt_downloads, &transfer, sizeof(transfer), d, 0);
}

void gt_download_remove (Transfer *transfer, Source *source)
{
	Dataset *d;

	d = dataset_lookup (gt_downloads, &transfer, sizeof(transfer));
	dataset_remove (d, &source, sizeof(source));

	if (dataset_length (d) == 0)
	{
		dataset_clear (d);
		dataset_remove (gt_downloads, &transfer, sizeof(transfer));
	}

	if (dataset_length (gt_downloads) == 0)
	{
		dataset_clear (gt_downloads);
		gt_downloads = NULL;
	}
}

/*****************************************************************************/

static int ds_find_hash (ds_data_t *key, ds_data_t *value, void *udata)
{
	Array      *a    = udata;
	char       *sha1;
	Source    **ret;
	Source     *src  = value->data;
	int         n;

	n = array_list (&a, &sha1, &ret, NULL);
	assert (n == 2);

	if (!src->hash)
		return DS_CONTINUE;

	/* NOTE: the hash is prefixed with giftd's hash here */
	if (strcmp (src->hash, sha1) == 0)
	{
		*ret = src;
		return DS_BREAK;
	}

	return DS_CONTINUE;
}

static int ds_traverse_transfer (ds_data_t *key, ds_data_t *value, void *udata)
{
	Dataset *d = value->data;

	dataset_foreach_ex (d, ds_find_hash, udata);
	return DS_CONTINUE;
}

Source *gt_download_lookup (const char *sha1)
{
	Array  *a;
	Source *ret = NULL;

	a = array_new ((void *)sha1, &ret, NULL);

	if (!a)
		return NULL;

	dataset_foreach_ex (gt_downloads, ds_traverse_transfer, a);
	array_unset (&a);

	return ret;
}
