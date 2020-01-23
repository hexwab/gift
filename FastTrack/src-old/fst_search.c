/*
 * $Id: fst_search.c,v 1.4 2003/06/27 17:29:52 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_search.h"

#include <libgift/proto/share.h>
#include <libgift/proto/share_hash.h>

/*****************************************************************************/

// called by giFT to initiate search
int gift_cb_search (Protocol *p, IFEvent *event, char *query, char *exclude, char *realm, Dataset *meta)
{
	FSTSearch *search = fst_search_create (event, SearchTypeSearch, query, exclude, realm);
	fst_searchlist_add (FST_PLUGIN->searches, search);

	FST_DBG_2 ("sending search query for \"%s\", fst_id = %d", search->query, search->fst_id);

	// FIXME: check result for closed session
	fst_search_send_query (search, FST_PLUGIN->session);

	return TRUE;
}

// called by giFT to initiate browse
int gift_cb_browse (Protocol *p, IFEvent *event, char *user, char *node)
{
	return FALSE;
}

// called by giFT to locate file
int gift_cb_locate (Protocol *p, IFEvent *event, char *htype, char *hash)
{
	FSTSearch *search;

	if (strcmp (htype, "FTH"))
		return FALSE;
	
	search = fst_search_create (event, SearchTypeLocate, hash, NULL, NULL);
	fst_searchlist_add (FST_PLUGIN->searches, search);

	FST_DBG_2 ("sending locate query for \"%s\", fst_id = %d", search->query, search->fst_id);

	// FIXME: check result for closed session
	fst_search_send_query (search, FST_PLUGIN->session);

	return TRUE;
}

// called by giFT to cancel search/locate/browse
void gift_cb_search_cancel (Protocol *p, IFEvent *event)
{
	FSTSearch *search = fst_searchlist_lookup_event (FST_PLUGIN->searches, event);

	if(search)
	{
		FST_DBG_2 ("removing search for \"%s\", fst_id = %d", search->query, search->fst_id);
		fst_searchlist_remove (FST_PLUGIN->searches, search);
		fst_search_free (search);
	}
}

/*****************************************************************************/

// allocate and init new search
FSTSearch *fst_search_create (IFEvent *event, FSTSearchType type, char *query, char *exclude, char *realm)
{
	FSTSearch *search = malloc (sizeof(FSTSearch));

	search->gift_event = event;
	search->fst_id = 0x0000;
	search->type = type;
	search->sent = 0;
	search->replies = 0;
	search->fw_replies = 0;

	search->query = query ? strdup (query) : NULL;
	search->exclude = exclude ? strdup (exclude) : NULL;
	search->realm = realm ? strdup (realm) : NULL;

	return search;
}

// free search
void fst_search_free (FSTSearch *search)
{
	if(!search)
		return;

	if(search->query)
		free (search->query);
	if(search->exclude)
		free (search->exclude);
	if(search->realm)
		free (search->realm);

	free (search);
}

// send search request to supernode via this session
int fst_search_send_query (FSTSearch *search, FSTSession *session)
{
	FSTPacket *packet = fst_packet_create();

	 // two unknown bytes
	fst_packet_put_ustr (packet, "\x00\x01", 2);

	// max search results
	fst_packet_put_uint16 (packet, htons(FST_MAX_SEARCH_RESULTS));
	// search id
	fst_packet_put_uint16 (packet, htons(search->fst_id));

	// dunno what this is
	fst_packet_put_uint8 (packet, 0x01);

	// search realm
	if(search->realm) {
		char *p, *realm = strdup(search->realm);
		if((p = strchr(realm, '/')))
			*p = 0;

		if		(!strcasecmp(realm, "audio"))			fst_packet_put_uint8 (packet, QUERY_REALM_AUDIO);
		else if	(!strcasecmp(realm, "video"))			fst_packet_put_uint8 (packet, QUERY_REALM_VIDEO);
		else if	(!strcasecmp(realm, "image"))			fst_packet_put_uint8 (packet, QUERY_REALM_IMAGES);
		else if	(!strcasecmp(realm, "text"))			fst_packet_put_uint8 (packet, QUERY_REALM_DOCUMENTS);
		else if	(!strcasecmp(realm, "application"))		fst_packet_put_uint8 (packet, QUERY_REALM_SOFTWARE);
		else											fst_packet_put_uint8 (packet, QUERY_REALM_EVERYTHING);

		free (realm);
	}
	else
	{
		fst_packet_put_uint8 (packet, QUERY_REALM_EVERYTHING);
	}

	// number of search terms
	fst_packet_put_uint8 (packet, 0x01);

	if(search->type == SearchTypeSearch)
	{
		// cmp type of first term
		fst_packet_put_uint8 (packet, (fst_uint8)QUERY_CMP_SUBSTRING);
		// field to cmp of first term
		fst_packet_put_uint8 (packet, (fst_uint8)FILE_TAG_ANY);
		// length of query string
		fst_packet_put_dynint (packet, strlen(search->query));
		// query string
		fst_packet_put_ustr (packet, search->query, strlen(search->query));
	}
	else if(search->type == SearchTypeLocate)
	{
		unsigned char hash[FST_HASH_LEN];
		// convert hash string to binary
		if(fst_hash_set_string (hash, search->query) == FALSE)
		{
			fst_packet_free (packet);
			return FALSE;
		}

		// cmp type of first term
		fst_packet_put_uint8 (packet, (fst_uint8)QUERY_CMP_EQUALS);
		// field to cmp of first term
		fst_packet_put_uint8 (packet, (fst_uint8)FILE_TAG_HASH);
		// length of query string
		fst_packet_put_dynint (packet, FST_HASH_LEN);
		// query string
		fst_packet_put_ustr (packet, hash, FST_HASH_LEN);
	}
	else
	{
		fst_packet_free (packet);
		return FALSE;
	}

	// now send it
	if(fst_session_send_message (session, SessMsgQuery, packet) == FALSE)
	{
		fst_packet_free (packet);
		return FALSE;
	}

	search->sent++;
	fst_packet_free (packet);

	return TRUE;
}

/*****************************************************************************/

// allocate and init searchlist
FSTSearchList *fst_searchlist_create ()
{
	FSTSearchList *searchlist = malloc (sizeof(FSTSearchList));

	searchlist->searches = NULL;
	searchlist->current_ft_id = 0x00;

	return searchlist;
}

static int searchlist_free_node(FSTSearch *search, void *udata)
{
	fst_search_free (search);
	return TRUE; // remove node
}

// free searchlist
void fst_searchlist_free (FSTSearchList *searchlist)
{
	if(!searchlist)
		return;

	searchlist->searches = list_foreach_remove (searchlist->searches, (ListForeachFunc)searchlist_free_node, NULL);
	free (searchlist);
}

// add search to list
void fst_searchlist_add (FSTSearchList *searchlist, FSTSearch *search)
{
	search->fst_id = searchlist->current_ft_id++;
	searchlist->searches = list_prepend (searchlist->searches, (void*)search);
}

// remove search from list
void fst_searchlist_remove (FSTSearchList *searchlist, FSTSearch *search)
{
	searchlist->searches = list_remove (searchlist->searches, search);
}

static int searchlist_lookup_cmp_id (FSTSearch *a, FSTSearch *b)
{
	return ((int)a->fst_id) - ((int)b->fst_id);
}

// lookup search by FastTrack id
FSTSearch *fst_searchlist_lookup_id (FSTSearchList *searchlist, fst_uint16 fst_id)
{
	List *node;
	FSTSearch *search = malloc (sizeof(FSTSearch));
	search->fst_id = fst_id;

	// only search->fst_id is used in cmp func
	node = list_find_custom (searchlist->searches, (void*)search, (CompareFunc)searchlist_lookup_cmp_id);

	free (search);

	if(!node)
		return NULL;

	return (FSTSearch*)node->data;
}

static int searchlist_lookup_cmp_event (FSTSearch *a, FSTSearch *b)
{
	return ((int)a->gift_event) - ((int)b->gift_event);
}

// lookup search by giFT event
FSTSearch *fst_searchlist_lookup_event (FSTSearchList *searchlist, IFEvent *event)
{
	List *node;
	FSTSearch *search = malloc (sizeof(FSTSearch));
	search->gift_event = event;

	// only search->fst_id is used in cmp func
	node = list_find_custom (searchlist->searches, (void*)search, (CompareFunc)searchlist_lookup_cmp_event);

	free (search);

	if(!node)
		return NULL;

	return (FSTSearch*)node->data;
}

// send queries for every search in list if search->count == 0 or resent == TRUE
int fst_searchlist_send_queries (FSTSearchList *searchlist, FSTSession *session, int resent)
{
	List *node = searchlist->searches;
	FSTSearch *search;
	int i=0;

	FST_HEAVY_DBG_1 ("resending pending searches to supernode, resent = %d", resent);

	for (; node; node = node->next)
	{
		search = (FSTSearch*)node->data;
		if(!search->sent || resent)
			if(fst_search_send_query (search, session) == FALSE)
				return FALSE;
		i++;
	}

	FST_HEAVY_DBG_1 ("sent %d searches to supernode.", i);

	return TRUE;
}

// process reply and send it to giFT, accepts SessMsgQueryReply and SessMsgQueryEnd
int fst_searchlist_process_reply (FSTSearchList *searchlist, FSTSessionMsg msg_type, FSTPacket *msg_data)
{
	FSTSearch *search;
	fst_uint16 fst_id, port;
	fst_uint32 ip;
	char *username, *netname, *tmp;
	unsigned char *hash;
	unsigned int filesize;
	char *filename, *href;
	int nresults, ntags, i;
	FileShare *file;

	// meta data
	int tag, taglen;
	FSTPacket *tagdata;
	FSTMetaTag *metatag;
	List *metalist = NULL;

	if(msg_type == SessMsgQueryEnd)
	{
		fst_id = ntohs(fst_packet_get_uint16 (msg_data));

		if((search = fst_searchlist_lookup_id (searchlist, fst_id)) == NULL)
		{
			FST_DBG_1 ("received query end for search not in list, fst_id = %d", fst_id);
			return FALSE;
		}

		FST_DBG_3 ("received query end for search with fst_id = %d, got %d replies of which %d are firewalled", fst_id, search->replies, search->fw_replies);

		// remove search from list
		fst_searchlist_remove (searchlist, search);

		// tell giFT we're finished, this makes giFT call gift_cb_search_cancel()
		FST_PROTO->search_complete (FST_PROTO, search->gift_event);

		// free search
		fst_search_free (search);

		return TRUE;
	}
	else if(msg_type != SessMsgQueryReply)
	{
		return FALSE;
	}

	// we got a query result

	// not sure what this first ip and port are, parent supernode maybe?
	ip = fst_packet_get_uint32 (msg_data);
	port = ntohs(fst_packet_get_uint16 (msg_data));

	// get query id and look up search
	fst_id = ntohs(fst_packet_get_uint16 (msg_data));

	if((search = fst_searchlist_lookup_id (searchlist, fst_id)) == NULL)
	{
		FST_HEAVY_DBG_1 ("received query reply for search not in list, fst_id = %d", fst_id);
		return FALSE;
	}

	nresults = ntohs(fst_packet_get_uint16 (msg_data));

	FST_HEAVY_DBG_4 ("query result begin: %s:%d, fst_id = %d, nresults = %d", net_ip_str(ip), port, fst_id, nresults);

	for(;nresults && fst_packet_remaining (msg_data) >= 32; nresults--)
	{
		// ip and port
		ip = fst_packet_get_uint32 (msg_data);
		port = ntohs(fst_packet_get_uint16 (msg_data));
		// bandwidth tag
		fst_packet_get_uint8 (msg_data);

		// user and network name
		// note: a compression is used here which refers back to previous replies
		// since we don't cache replies we just return "<unknown>" as user name in that case
		if(*(msg_data->read_ptr) == 0x02)
		{
			// compressed
			msg_data->read_ptr++;
			username = strdup("<unknown>");
			netname = strdup("<unknown>");
		}
		else
		{
			// user name
			if((i = fst_packet_strlen (msg_data, 0x01)) < 0)
				return FALSE;

			username = fst_packet_get_ustr (msg_data, i+1);
			username[i] = 0;

			// network name
			if((i = fst_packet_strlen (msg_data, 0x00)) < 0)
			{
				free (username);
				return FALSE;
			}
			netname = fst_packet_get_ustr (msg_data, i+1);
			netname[i] = 0;
		}

		// create actual user name sent to giFT
		tmp = malloc(strlen(username) + 32);
		sprintf (tmp, "%s@%s", username, net_ip_str(ip));
		free (username);
		username = tmp;


		FST_HEAVY_DBG_5 ("result (%d): %s:%d \t%s@%s", nresults, net_ip_str(ip), port, username, netname);

		// 20 byte hash
		hash = fst_packet_get_ustr (msg_data, 20);
		FST_HEAVY_DBG_1 ("\thash = %s", fst_hash_get_string (hash));

		// checksum
		fst_packet_get_dynint (msg_data);
		// file size
		filesize = fst_packet_get_dynint (msg_data);
		// number of meta data tags
		ntags = fst_packet_get_dynint (msg_data);

		FST_HEAVY_DBG_2 ("\tfilesize = %d, ntags = %d", filesize, ntags);

		// read tags
		for(;ntags && fst_packet_remaining (msg_data) >= 2; ntags--)
		{
			// tag
			tag = fst_packet_get_dynint (msg_data);
			// tag_len
			taglen = fst_packet_get_dynint (msg_data);
			// tag_data
			tagdata = fst_packet_create_copy (msg_data, taglen);
/*
			{
				char *data;
				data = fst_packet_get_str (tagdata, taglen);
				FST_HEAVY_DBG ("\t\ttag: type = 0x%02x, len = %02d, data = %s", tag, taglen, data);
				free (data);
				fst_packet_rewind (tagdata);
			}
*/
			metatag = fst_metatag_create_from_filetag (tag, tagdata);

			if(metatag)
			{
				if(strcmp(metatag->name, "filename") == 0) // filename is special case
				{
					filename = strdup (metatag->value);
					fst_metatag_free (metatag);
				}
				else
				{
					metalist = list_prepend (metalist, metatag);
				}
			}
			else
			{
				// we sometimes get very weird tags with types like 0x40003 and strange content
				// since i cannot find any problem with the decryption i think they are sent that way
/*
				char *data;
				fst_packet_rewind (tagdata);
				data = fst_packet_get_str (tagdata, taglen);
				FST_DBG ("\tunhandled file tag: type = 0x%02x, len = %02d, data = %s", tag, taglen, data);
				free (data);
*/
			}

			fst_packet_free (tagdata);
		}

		// create FileShare for giFT, we just pass the realm used in the query for now
		file = share_new_ex (FST_PROTO, NULL, 0, filename,
		                     search->realm, filesize, 0);

		// add hash, hash is freed in share_free()
		share_set_hash (file, "FTH", hash, FST_HASH_LEN, FALSE);

		// add meta data
		for(; metalist; metalist = list_remove_link (metalist, metalist))
		{
			share_set_meta (file, ((FSTMetaTag*)metalist->data)->name, ((FSTMetaTag*)metalist->data)->value);
			fst_metatag_free (((FSTMetaTag*)metalist->data));
		}

		// create href for giFT
/*
		{
			char *url_filename = fst_utils_url_encode (filename);
			href = malloc (strlen(url_filename) + 128);
			sprintf(href, "FastTrack://%s:%d/%s", net_ip_str (ip), port, url_filename);
			free (url_filename);
		}
*/
		href = malloc (FST_HASH_STR_LEN + 128);
		sprintf(href, "FastTrack://%s:%d/.hash=%s", net_ip_str (ip), port, fst_hash_get_string(hash));

		// send result to giFT if the ip is not private and port != 0
		if(!fst_utils_ip_private (ip) && port != 0)
		{
			FST_PROTO->search_result (FST_PROTO, search->gift_event, username, netname, href, 1, file);
		}
		else
		{
			search->fw_replies++;
		}

		// increment reply counter
		search->replies++;

		// free remaining stuff
		share_free (file);
		free (filename);
		free (username);
		free (netname);
		free (href);
	}

	return TRUE;
}

/*****************************************************************************/

