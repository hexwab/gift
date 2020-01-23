/*
 * $Id: share_local.c,v 1.1 2003/06/21 19:07:23 jasta Exp $
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

#include "giftd.h"

#include "plugin/share.h"

#include "share_cache.h"
#include "share_local.h"

/*****************************************************************************/

static BOOL find_hash (ds_data_t *key, ds_data_t *value, void **args)
{
	Share      *share = value->data;
	Hash       *hash;
	const char *htype    = args[0];
	const char *hash_cmp = args[1];
	size_t     *len_cmp  = args[2];

	if (!(hash = share_get_hash (share, htype)))
		return FALSE;

	assert (hash->len == *len_cmp);

	return (memcmp (hash->data, hash_cmp, hash->len) == 0);
}

static Share *lookup_hash (Dataset *shares, const char *htype,
                           unsigned char *hash, size_t len)
{
	void *args[] = { (void *)htype, (void *)hash, (void *)&len };

	return dataset_find (shares, DS_FIND(find_hash), args);
}

static BOOL find_hpath (ds_data_t *key, ds_data_t *value, const char *hpath_cmp)
{
	Share *share = value->data;
	char  *hpath;

	if (!(hpath = share_get_hpath (share)))
		return FALSE;

	return (strcmp (hpath_cmp, hpath) == 0);
}

static Share *lookup_hpath (Dataset *shares, const char *hpath)
{
	return dataset_find (shares, DS_FIND(find_hpath), (void *)hpath);
}

/* implemented as Protocol::share_lookup */
Share *share_local_lookupv (int command, va_list args)
{
	Dataset *shares;
	Share   *share;

	if (!(shares = share_index (NULL, NULL)))
		return NULL;

	switch (command)
	{
	 case SHARE_LOOKUP_HASH:
		share = lookup_hash (shares,
		                     va_arg (args, const char *),
		                     va_arg (args, unsigned char *),
		             (size_t)va_arg (args, long));
		break;
	 case SHARE_LOOKUP_HPATH:
		share = lookup_hpath (shares,
		                      va_arg (args, const char *));
		break;
	 default:
		abort ();
		break;
	}

	return share;
}

Share *share_local_lookup (int command, ...)
{
	Share  *share;
	va_list args;

	va_start (args, command);
	share = share_local_lookupv (command, args);
	va_end (args);

	return share;
}
