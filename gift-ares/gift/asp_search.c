/*
 * $Id: asp_search.c,v 1.11 2005/01/01 22:07:10 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#include "asp_plugin.h"

/*****************************************************************************/

typedef struct
{
	ASSearch *search;
	IFEvent *event;
} find_search_data_t;

static as_bool find_search_itr (ASHashTableEntry *entry, 
                                find_search_data_t *data)
{
	ASSearch *s = entry->val;
	
	if (s->udata == data->event)
	{
		assert (!data->search);
		data->search = s;
	}

	return FALSE; /* Keep entry. */
}

/* Find active search by giFT event. */
static ASSearch *find_search (IFEvent *event)
{
	find_search_data_t data = { NULL, event };
	
	if (!event)
		return NULL;

	as_hashtable_foreach (AS->searchman->searches,
	                      (ASHashTableForeachFunc)find_search_itr,
	                      &data);

	return data.search;
}

/*****************************************************************************/

static as_bool meta_to_gift (ASMetaTag *tag, Share *share)
{
	if (!STRCASECMP (tag->name, "bitrate") && tag->value)
		share_set_meta (share, tag->name, stringf ("%s000", tag->value));
	else
		share_set_meta (share, tag->name, tag->value);

	return TRUE;
}

/* Called by ares library for each result. */
static void result_callback (ASSearch *search, ASResult *r, as_bool duplicate)
{
	Share *share;
	char *url, *filename, *user;

	if (!r)
	{
		AS_DBG_1 ("Search complete. Id: %d.", search->id);

		/* Tell giFT we're finished. */
		PROTO->search_complete (PROTO, search->udata);

		/* The above does _not_ make giFT call asp_cb_search_cancel() so
		 * remove the search now.
		 */
		if (!as_searchman_remove (AS->searchman, search))
		{
			AS_ERR ("Failed to remove complete search");
			assert (0);
		}
		return;
	}

	/* Create a share object for giFT. */
	if (!(share = share_new (NULL)))
		return;

	share->p    = PROTO;
	share->size = r->filesize;
	filename    = r->filename;

	/* Try to find file name and size in hash table if none was returned. */
	if (search->type == SEARCH_LOCATE && (!filename || share->size == 0))
	{
		size_t size;
		char *name;

		/* Lookup this hash in the evil hash map to find the
		 * size and filename for giFT.
		 */
		if (asp_hashmap_lookup (r->hash, &name, &size))
		{
			if (share->size == 0)
				share->size = size;

			if (!filename && name && *name)
				filename = name;
		}
	}

	/* If we still don't have a file name fake one to prevent things from
	 * blowing up elsewhere.
	 */
	if (!filename)
		filename = "<Unknown>";

	share_set_path (share, filename);
	share_set_mime (share, mime_type (filename));
	share_set_hash (share, "SHA1", r->hash->data, AS_HASH_SIZE, FALSE);

	/* Add meta data. */
	if (r->meta)
		as_meta_foreach_tag (r->meta, (ASMetaForeachFunc)meta_to_gift, share);

	/* Create the url giFT will pass back to us on download. */
	if (!(url = as_source_serialize (r->source)))
	{
		AS_ERR_1 ("Couldn't serialize source '%s'",
		          as_source_str (r->source));
		share_free (share);
		return;
	}

	/* Assemble username@ip string. */
	if (STRING_NULL (r->source->username))
		user = stringf_dup ("%s@%s", r->source->username, 
		                    net_ip_str (r->source->host));
	else
		user = gift_strdup (net_ip_str (r->source->host));

	/* Send the result to giFT. */
	PROTO->search_result (PROTO, search->udata, user, NULL, url, 1, share);

	free (user);
	free (url);
	share_free (share);
}

/*****************************************************************************/

/* Called by giFT to initiate search. */
BOOL asp_giftcb_search (Protocol *p, IFEvent *event, char *query,
                        char *exclude, char *realm, Dataset *meta)
{
	ASSearch *search;
	ASSearchRealm r = SEARCH_ANY;

	if (realm)
	{
		struct
		{
			char *name;
			ASSearchRealm realm;
		}
		realms[] =
		{
			{ "image",       SEARCH_IMAGE },
			{ "audio",       SEARCH_AUDIO },
			{ "video",       SEARCH_VIDEO },
			{ "text",        SEARCH_DOCUMENT },
			{ "application", SEARCH_SOFTWARE },
			{ NULL,          SEARCH_ANY }
		}, *ptr;

		for (ptr = realms; ptr->name; ptr++)
		{
			if (!strncasecmp (realm, ptr->name, strlen (ptr->name)))
			{
				r = ptr->realm;
				break;
			}
		}
	}

	if (!(search = as_searchman_search (AS->searchman,
	    (ASSearchResultCb) result_callback, query, r)))
	{
		AS_ERR_1 ("Failed to start search for '%s'.", query);
		return FALSE;
	}

	search->udata = event;

	AS_DBG_3 ("Started search for '%s' in realm '%s'. Id: %d.", query,
	          realm ? realm : "Any", search->id);

	return TRUE;
}

/* Called by giFT to initiate browse. */
BOOL asp_giftcb_browse (Protocol *p, IFEvent *event, char *user, char *node)
{
	return FALSE;
}

/* Called by giFT to locate file. */
int asp_giftcb_locate (Protocol *p, IFEvent *event, char *htype, char *hstr)
{
	ASSearch *search;
	ASHash *hash;

	if (!htype || !hstr)
		return FALSE;

	if (gift_strcasecmp (htype, "SHA1"))
		return FALSE;

	if (!(hash = asp_hash_decode (hstr)))
	{
		AS_DBG_1 ("malformed hash '%s'", as_hash_str (hash));
		return FALSE;
	}

	if (!(search = as_searchman_locate (AS->searchman,
	    (ASSearchResultCb) result_callback, hash)))
	{
		AS_ERR_1 ("Failed to start search for '%s'.", as_hash_str (hash));
		return FALSE;
	}

	search->udata = event;

	AS_DBG_2 ("Started locate for '%s'. Id: %d.", as_hash_str (hash),
	          search->id);

	as_hash_free (hash);

	return TRUE;
}

/* Called by giFT to cancel search/locate/browse. */
void asp_giftcb_search_cancel (Protocol *p, IFEvent *event)
{
	ASSearch *search;

	search = find_search (event);
	assert (search);

	AS_DBG_1 ("Search cancelled. Id: %d.", search->id);
	
	if (!as_searchman_remove (AS->searchman, search))
	{
		AS_ERR ("Failed to remove cancelled search");
		assert (0);
	}
}

/*****************************************************************************/
