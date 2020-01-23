/*
 * $Id: fst_search.c,v 1.30 2004/07/08 17:58:44 mkern Exp $
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
#include <math.h> /* for exp used in bandwidth calculation */

/*****************************************************************************/
/*
#define LOG_TAGS
*/
/*****************************************************************************/

/* called by giFT to initiate search */
int fst_giftcb_search (Protocol *p, IFEvent *event, char *query, char *exclude,
					   char *realm, Dataset *meta)
{
	int count;

	FSTSearch *search = fst_search_create (event, SearchTypeSearch, query,
										   exclude, realm);
	fst_searchlist_add (FST_PLUGIN->searches, search);

	if (FST_PLUGIN->stats->sessions == 0)
	{
		FST_DBG_2 ("not connected, queueing query for \"%s\", fst_id = %d",
				   search->query, search->fst_id);
		return TRUE;
	}

	if ((count = fst_search_send_query_to_all (search)) <= 0)
	{
		FST_DBG_2 ("fst_search_send_query_to_all failed for \"%s\", fst_id = %d",
				   search->query, search->fst_id);
		fst_searchlist_remove (FST_PLUGIN->searches, search);
		fst_search_free (search);
		return FALSE;
	}

	FST_DBG_3 ("sent search query for \"%s\" to %d supernodes, fst_id = %d",
			   search->query, count, search->fst_id);

	return TRUE;
}

/* called by giFT to initiate browse */
int fst_giftcb_browse (Protocol *p, IFEvent *event, char *user, char *node)
{
	return FALSE;
}

/* called by giFT to locate file */
int fst_giftcb_locate (Protocol *p, IFEvent *event, char *htype, char *hstr)
{
	FSTSearch *search;
	FSTHash *hash;
	int count;

	if (!htype || !hstr)
		return FALSE;

	/* BEGIN HACK: magic search causes supernode jump */
	if (!gift_strcasecmp (htype, FST_KZHASH_NAME) &&
	    !gift_strcasecmp (hstr, "dance"))
	{
		FST_DBG ("jumping supernode");
		fst_session_disconnect (FST_PLUGIN->session);
		return FALSE;
	}
	/* END HACK */

	if (!(hash = fst_hash_create ()))
		return FALSE;

	if (!gift_strcasecmp (htype, FST_KZHASH_NAME))
	{
		if (!fst_hash_decode16_kzhash (hash, hstr))
		{
			fst_hash_free (hash);	
			FST_DBG_1 ("invalid hash string: %s", hstr);
			return FALSE;
		}
	}
	else if (!gift_strcasecmp (htype, FST_FTHASH_NAME))
	{
		if (!fst_hash_decode64_fthash (hash, hstr))
		{
			fst_hash_free (hash);	
			FST_DBG_1 ("invalid hash string: %s", hstr);
			return FALSE;
		}
	}
	else
	{
		fst_hash_free (hash);	
		FST_HEAVY_DBG_1 ("unknown hash type: %s", htype);
		return FALSE;
	}

	search = fst_search_create (event, SearchTypeLocate, hstr, NULL, NULL);
	search->hash = hash;

	fst_searchlist_add (FST_PLUGIN->searches, search);

	if (FST_PLUGIN->stats->sessions == 0)
	{
		FST_DBG_2 ("not connected, queueing query for \"%s\", fst_id = %d",
				   search->query, search->fst_id);
		return TRUE;
	}
	
	if ((count = fst_search_send_query_to_all (search)) <= 0)
	{
		FST_DBG_2 ("fst_search_send_query_to_all failed for \"%s\", fst_id = %d",
				   search->query, search->fst_id);
		fst_searchlist_remove (FST_PLUGIN->searches, search);
		fst_search_free (search);
		return FALSE;
	}

	FST_DBG_3 ("sent locate query for \"%s\" to %d supernodes, fst_id = %d",
				search->query, count, search->fst_id);

	return TRUE;
}

/* called by giFT to cancel search/locate/browse */
void fst_giftcb_search_cancel (Protocol *p, IFEvent *event)
{
	FSTSearch *search = fst_searchlist_lookup_event (FST_PLUGIN->searches, event);

	if(search)
	{
		FST_DBG_2 ("removing search for \"%s\", fst_id = %d",
				   search->query, search->fst_id);
		fst_searchlist_remove (FST_PLUGIN->searches, search);
		fst_search_free (search);
	}
}

/*****************************************************************************/

/* allocate and init new search */
FSTSearch *fst_search_create (IFEvent *event, FSTSearchType type, char *query,
							  char *exclude, char *realm)
{
	FSTSearch *search = malloc (sizeof(FSTSearch));

	search->gift_event = event;
	search->fst_id = 0x0000;
	search->type = type;
	search->sent = 0;
	search->search_more = config_get_int (FST_PLUGIN->conf,
	                                      "main/auto_search_more=0");

	search->banlist_filter = config_get_int (FST_PLUGIN->conf,
	                                         "main/banlist_filter=0");

	search->replies = 0;
	search->fw_replies = 0;
	search->banlist_replies = 0;

	search->query = query ? strdup (query) : NULL;
	search->exclude = exclude ? strdup (exclude) : NULL;
	search->realm = realm ? strdup (realm) : NULL;
	search->hash = NULL;

	return search;
}

// free search
void fst_search_free (FSTSearch *search)
{
	if(!search)
		return;

	free (search->query);
	free (search->exclude);
	free (search->realm);
	fst_hash_free (search->hash);

	free (search);
}

/* send search request to supernode via this session */
int fst_search_send_query (FSTSearch *search, FSTSession *session)
{
	FSTPacket *packet = fst_packet_create();
	fst_uint8 realm = QUERY_REALM_EVERYTHING;

	fst_packet_put_ustr (packet, "\x00\x01", 2);
	fst_packet_put_uint16 (packet, htons(FST_MAX_SEARCH_RESULTS));
	fst_packet_put_uint16 (packet, htons(search->fst_id));
	/* dunno what this is */
	fst_packet_put_uint8 (packet, 0x01);

	/* figure out realm, search->realm may be NULL */
	if (search->realm)
	{
		char *p, *realm_str = strdup (search->realm);
		
		/* ignore everything after '/' so we understand mime types */
		if((p = strchr(realm_str, '/')))
			*p = 0;

		if (!strcasecmp(realm_str, "audio"))			realm = QUERY_REALM_AUDIO;
		else if	(!strcasecmp(realm_str, "video"))		realm = QUERY_REALM_VIDEO;
		else if	(!strcasecmp(realm_str, "image"))		realm = QUERY_REALM_IMAGES;
		else if	(!strcasecmp(realm_str, "text"))		realm = QUERY_REALM_DOCUMENTS;
		else if	(!strcasecmp(realm_str, "application"))	realm = QUERY_REALM_SOFTWARE;

		free (realm_str);
	}

	fst_packet_put_uint8 (packet, realm);

	/* number of search terms */
	fst_packet_put_uint8 (packet, 0x01);

	switch (search->type)
	{
	case  SearchTypeSearch:
		if (!search->query || strlen (search->query) < 1)
		{
			fst_packet_free (packet);
			return FALSE;
		}

		fst_packet_put_uint8 (packet, (fst_uint8)QUERY_CMP_SUBSTRING);
		fst_packet_put_uint8 (packet, (fst_uint8)FILE_TAG_ANY);
		fst_packet_put_dynint (packet, strlen(search->query));
		fst_packet_put_ustr (packet, search->query, strlen(search->query));
		break;

	case SearchTypeLocate:
		assert (search->hash);

		fst_packet_put_uint8 (packet, (fst_uint8)QUERY_CMP_EQUALS);
		fst_packet_put_uint8 (packet, (fst_uint8)FILE_TAG_HASH);
		fst_packet_put_dynint (packet, FST_FTHASH_LEN);
		fst_packet_put_ustr (packet, FST_FTHASH (search->hash), FST_FTHASH_LEN);
		break;

	default:
		fst_packet_free (packet);
		return FALSE;
	}

	/* now send it */
	if(fst_session_send_message (session, SessMsgQuery, packet) == FALSE)
	{
		fst_packet_free (packet);
		return FALSE;
	}

	search->sent++;
	fst_packet_free (packet);

	return TRUE;
}

int fst_search_send_query_to_all (FSTSearch *search)
{
	FSTSession *sess;
	List *item = FST_PLUGIN->sessions;
	int i;

	/* send to primary supernode */
	if (FST_PLUGIN->session->state == SessEstablished)
		if (!fst_search_send_query (search, FST_PLUGIN->session))
			return 0;

	/* send to additional supernodes */
	for (i = 1; item; item = item->next)
	{
		sess = (FSTSession*)item->data;

		if (sess->state == SessEstablished)
		{
			if (!fst_search_send_query (search, sess))
				return i;
			i++;
		}
	}

	return i;
}

/*****************************************************************************/

/* allocate and init searchlist */
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
	return TRUE; /* remove node */
}

/* free search list */
void fst_searchlist_free (FSTSearchList *searchlist)
{
	if(!searchlist)
		return;

	searchlist->searches = list_foreach_remove (searchlist->searches,
												(ListForeachFunc)searchlist_free_node,
												NULL);
	free (searchlist);
}

/* add search to list */
void fst_searchlist_add (FSTSearchList *searchlist, FSTSearch *search)
{
	search->fst_id = searchlist->current_ft_id++;
	searchlist->searches = list_prepend (searchlist->searches, (void*)search);
}

/* remove search from list */
void fst_searchlist_remove (FSTSearchList *searchlist, FSTSearch *search)
{
	searchlist->searches = list_remove (searchlist->searches, search);
}

static int searchlist_lookup_cmp_id (FSTSearch *a, FSTSearch *b)
{
	return ((int)a->fst_id) - ((int)b->fst_id);
}

/* lookup search by FastTrack id */
FSTSearch *fst_searchlist_lookup_id (FSTSearchList *searchlist,
									 fst_uint16 fst_id)
{
	List *node;
	FSTSearch *search = malloc (sizeof(FSTSearch));
	search->fst_id = fst_id;

	/* only search->fst_id is used in cmp func */
	node = list_find_custom (searchlist->searches, (void*)search,
							 (CompareFunc)searchlist_lookup_cmp_id);

	free (search);

	if(!node)
		return NULL;

	return (FSTSearch*)node->data;
}

static int searchlist_lookup_cmp_event (FSTSearch *a, FSTSearch *b)
{
	return ((int)a->gift_event) - ((int)b->gift_event);
}

/* lookup search by giFT event */
FSTSearch *fst_searchlist_lookup_event (FSTSearchList *searchlist,
										IFEvent *event)
{
	List *node;
	FSTSearch *search = malloc (sizeof(FSTSearch));
	search->gift_event = event;

	/* only search->fst_id is used in cmp func */
	node = list_find_custom (searchlist->searches, (void*)search,
							 (CompareFunc)searchlist_lookup_cmp_event);

	free (search);

	if(!node)
		return NULL;

	return (FSTSearch*)node->data;
}

/* send queries for every search in list if search->count == 0
 * or resent == TRUE
 */
int fst_searchlist_send_queries (FSTSearchList *searchlist,
								 FSTSession *session, int resent)
{
	List *node = searchlist->searches;
	FSTSearch *search;
	int i=0;

	for (; node; node = node->next)
	{
		search = (FSTSearch*)node->data;
		if (!search->sent || resent)
			if (!fst_search_send_query (search, session))
				return FALSE;
		i++;
	}

	if (resent)
		FST_DBG_1 ("resent %d pending searches to supernode", i);
	else
		FST_HEAVY_DBG_1 ("sent %d searches to supernode", i);

	return TRUE;
}

/* remove tag */
static int result_free (FSTSearchResult *result, void *udata)
{
	fst_searchresult_free (result);
	return TRUE;
}

/* process reply and send it to giFT
 * accepts SessMsgQueryReply and SessMsgQueryEnd 
 */
int fst_searchlist_process_reply (FSTSearchList *searchlist,
								  FSTSessionMsg msg_type, FSTPacket *msg_data)
{
	FSTSearch *search;
	fst_uint16 fst_id;
	in_addr_t sip;
	in_port_t sport;
	int nresults, ntags, i;
	List *results = NULL, *item;

	if (msg_type == SessMsgQueryEnd)
	{
		int good_replies;
		fst_id = ntohs(fst_packet_get_uint16 (msg_data));

		if (! (search = fst_searchlist_lookup_id (searchlist, fst_id)))
		{
			FST_DBG_1 ("received query end for search not in list, fst_id = %d",
					   fst_id);
			return FALSE;
		}

		good_replies = search->replies - search->fw_replies - search->banlist_replies;

		FST_DBG_4 ("received end of search for fst_id %d, %d replies, %d firewalled, %d banned",
				   fst_id, search->replies, search->fw_replies, search->banlist_replies);

		/* check if we need to auto search more */
		if (search->search_more > 0 &&
		    search->type == SearchTypeSearch &&
			good_replies < FST_MAX_SEARCH_RESULTS)
		{
			/* send off another query */
			FST_DBG_2 ("auto searching more (%d) for fst_id %d",
			           search->search_more - 1, search->fst_id);

			if (!fst_search_send_query (search, FST_PLUGIN->session))
			{
				FST_DBG_2 ("fst_search_send_query failed for \"%s\", fst_id = %d",
						search->query, search->fst_id);
				/* return, we will search again on next supernode */
				return FALSE;
			}

			search->search_more--;

			return TRUE;
		}

		/* remove search from list */
		fst_searchlist_remove (searchlist, search);

		/* tell giFT we're finished, this makes giFT call gift_cb_search_cancel() */
		FST_PROTO->search_complete (FST_PROTO, search->gift_event);

		/* free search */
		fst_search_free (search);

		return TRUE;
	}

	assert (msg_type == SessMsgQueryReply);
	/* we got a query result */

	/* supernode ip an port */
	sip = fst_packet_get_uint32 (msg_data);
	sport = ntohs(fst_packet_get_uint16 (msg_data));

	/* get query id and look up search */
	fst_id = ntohs(fst_packet_get_uint16 (msg_data));

	if (! (search = fst_searchlist_lookup_id (searchlist, fst_id)))
	{
		FST_HEAVY_DBG_1 ("received query reply for search not in list, fst_id = %d",
						 fst_id);
		return FALSE;
	}

	/* get number of results */
	nresults = ntohs(fst_packet_get_uint16 (msg_data));

	for(;nresults && fst_packet_remaining (msg_data) >= 32; nresults--)
	{
		FSTSearchResult *result;

		if (! (result = fst_searchresult_create ()))
		{
			list_foreach_remove (results, (ListForeachFunc)result_free, NULL);			
			return FALSE;
		}

		/* add new result to list */
		results = list_prepend (results, (void*) result);
		result->source->snode_ip = sip;
		result->source->snode_port = sport;
		/* save our current supernode ip */
		result->source->parent_ip = FST_PLUGIN->session->tcpcon->host;

		result->source->ip = fst_packet_get_uint32 (msg_data);
		result->source->port = ntohs(fst_packet_get_uint16 (msg_data));

		/* convert bandwidth back from log scale to kbit/sec*/
		result->source->bandwidth = fst_packet_get_uint8 (msg_data);

		if (result->source->bandwidth > 0)
			result->source->bandwidth = exp(((double)result->source->bandwidth) * 0.0495105 - 2.9211202);

		/* user and network name */
		if(*(msg_data->read_ptr) == 0x02)
		{
			/* compressed, look up names based on ip and port */
			msg_data->read_ptr++;

			/* start with results->next because results is us */
			for (item=results->next; item; item = item->next)
			{
				FSTSearchResult *res = (FSTSearchResult*) item->data;
				if (res->source->ip == result->source->ip &&
				    res->source->port == result->source->port)
				{
					result->source->username = STRDUP (res->source->username);
					result->source->netname = STRDUP (res->source->netname);
					FST_HEAVY_DBG_2 ("decompressed %s@%s",
					                 result->source->username,
					                 result->source->netname);
					break;
				}
			}

			if (!result->source->username)
				result->source->username = STRDUP ("<unknown>");
			if (!result->source->netname)
				result->source->netname = STRDUP ("<unknown>");
		}
		else
		{
			/* user name */
			if((i = fst_packet_strlen (msg_data, 0x01)) < 0)
			{
				list_foreach_remove (results, (ListForeachFunc)result_free, NULL);			
				return FALSE;
			}

			result->source->username = fst_packet_get_ustr (msg_data, i+1);
			result->source->username[i] = 0;

			/* network name */
			if((i = fst_packet_strlen (msg_data, 0x00)) < 0)
			{
				list_foreach_remove (results, (ListForeachFunc)result_free, NULL);			
				return FALSE;
			}

			result->source->netname = fst_packet_get_ustr (msg_data, i+1);
			result->source->netname[i] = 0;
		}

		if (fst_packet_remaining (msg_data) >= FST_FTHASH_LEN)
		{
			fst_hash_set_raw (result->hash, msg_data->read_ptr, FST_FTHASH_LEN);
			msg_data->read_ptr += FST_FTHASH_LEN;
		}

		result->file_id = fst_packet_get_dynint (msg_data);
		result->filesize = fst_packet_get_dynint (msg_data);

		ntags = fst_packet_get_dynint (msg_data);

#ifdef HEAVY_DEBUG
		{
		FST_HEAVY_DBG_2 ("result %d: %s", nresults,
		                 fst_hash_encode64_fthash (result->hash));
		FST_HEAVY_DBG_2 ("  address: %s:%d", net_ip_str(result->source->ip),
		                 result->source->port);
		FST_HEAVY_DBG_2 ("  name: %s@%s", result->source->username,
		                 result->source->netname);
		FST_HEAVY_DBG_2 ("  filesize: %d, ntags: %d", result->filesize, ntags);
		}
#endif

		/* read tags */
		for(;ntags && fst_packet_remaining (msg_data) >= 2; ntags--)
		{
			int	tag = fst_packet_get_dynint (msg_data);
			int taglen = fst_packet_get_dynint (msg_data);
			FSTPacket *tagdata;
			FSTMetaTag *metatag;

			/* Large tags are a sure sign of broken packets.
			 * These packets have random junk inserted or miss some bytes while
			 * the surrounding packet data is ok. For example there was an
			 * artist tag with a correct length of 0x08 but the following
			 * string was "Cldplay" shifting the entire packet by one byte.
			 * This is _not_ a problem on our end! Looks like a memory
			 * corruption issue on the sender side.
			 */
			if (tag > 0x40)
			{
				FST_WARN ("Corrupt search result detected. Bitch to the Kazaa developers.");
#ifdef LOG_TAGS
				FST_DBG ("*** Dump of corrupt result saved ***");
				save_bin_data (msg_data->data, msg_data->used);
#endif
			}

			if (! (tagdata = fst_packet_create_copy (msg_data, taglen)))
			{
				list_foreach_remove (results, (ListForeachFunc)result_free, NULL);			
				return FALSE;
			}

#ifdef LOG_TAGS
			{
			char *buf = fst_packet_get_str (tagdata, taglen);
			FST_HEAVY_DBG_4 ("    tag: 0x%02x (%s), len: %02d, data: %s",
							 tag, fst_meta_name_from_tag (tag), taglen, buf);
			free (buf);
			fst_packet_rewind (tagdata);
			}
#endif

			if ((metatag = fst_metatag_create_from_filetag (tag, tagdata)))
			{		
				/* filename is special case */
				if (!strcmp(metatag->name, "filename"))
				{
					result->filename = strdup (metatag->value);
					fst_metatag_free (metatag);
				}
				else
				{
					fst_searchresult_add_tag (result, metatag);
				}
			}

			fst_packet_free (tagdata);
		}
	}

	/* we parsed the packet, send all results to gift now */
	for (item=results; item; item = item->next)
	{
		FSTSearchResult *result = (FSTSearchResult*) item->data;

		/* send result to giFT if the ip is not on ban list
		 * and we are able to receive push replies for private ips 
		 */
		if (fst_source_firewalled (result->source) &&
			(
			  !FST_PLUGIN->server ||
			  (FST_PLUGIN->external_ip != FST_PLUGIN->local_ip && !FST_PLUGIN->forwarding)
			)
		   )
		{
			search->fw_replies++;
		}
		else if (search->banlist_filter &&
				 fst_ipset_contains (FST_PLUGIN->banlist, result->source->ip))
		{
			search->banlist_replies++;
		}
		else
		{
			fst_searchresult_write_gift (result, search);
		}	
			
		search->replies++;
	}

	list_foreach_remove (results, (ListForeachFunc)result_free, NULL);			

	return TRUE;
}

/*****************************************************************************/

/* alloc and init result */
FSTSearchResult *fst_searchresult_create ()
{
	FSTSearchResult *result;

	if (!(result = malloc (sizeof (FSTSearchResult))))
		return NULL;

	if (!(result->source = fst_source_create ()))
	{
		free (result);
		return NULL;
	}

	if (!(result->hash = fst_hash_create ()))
	{
		fst_source_free (result->source);
		free (result);
		return NULL;
	}

	result->filename = NULL;
	result->filesize = 0;
	result->file_id = 0;
	result->metatags = NULL;

	return result;
}

/* remove tag */
static int searchresult_free_tag (FSTMetaTag *tag, void *udata)
{
	fst_metatag_free (tag);
	return TRUE;
}

/* free result */
void fst_searchresult_free (FSTSearchResult *result)
{
	if (!result)
		return;

	fst_source_free (result->source);
	fst_hash_free (result->hash);
	free (result->filename);

	list_foreach_remove (result->metatags,
						 (ListForeachFunc)searchresult_free_tag,
						 NULL);

	free (result);
}

/* add meta data tag to result */
void fst_searchresult_add_tag (FSTSearchResult *result, FSTMetaTag *tag)
{
	if (!result || !tag)
		return;

	result->metatags = list_prepend (result->metatags, (void*) tag);
}

/* send result to gift */
int fst_searchresult_write_gift (FSTSearchResult *result, FSTSearch *search)
{
	FileShare *share;
	List *item;
	char *href, *username;
	unsigned int avail = 0;

	if (!result || !search)
		return FALSE;

	/* create FileShare for giFT */
	if (! (share = share_new (NULL)))
		return FALSE;

	share->p = FST_PROTO;
	share->size = result->filesize;

	share_set_path (share, result->filename);
	share_set_mime (share, mime_type (result->filename));

	/* If this is a source search and we know the full kzhash report it back to
	 * giFT. Otherwise only report the fthash so integrity verification works
	 * correctly.
	 */
	if (search->hash &&
	    fst_hash_have_md5tree (search->hash) &&
	    fst_hash_equal (result->hash, search->hash))
	{
		share_set_hash (share, FST_KZHASH_NAME, FST_KZHASH (search->hash),
	                    FST_KZHASH_LEN, TRUE);
	}
	else
	{
		share_set_hash (share, FST_FTHASH_NAME, FST_FTHASH (result->hash),
	                    FST_FTHASH_LEN, TRUE);
	}

	/* add meta data */
	for (item = result->metatags; item; item = item->next)
	{
		FSTMetaTag *metatag = (FSTMetaTag*) item->data;
		share_set_meta (share, metatag->name, metatag->value);
	}

	/* create href for giFT */
	href = fst_source_encode (result->source);

	/* create actual user name sent to giFT */
	username = stringf_dup ("%s@%s", result->source->username,
	                        net_ip_str(result->source->ip));

	/* Calculate avilability based on reported bandwidth. */
	if (result->source->bandwidth > 0)
	{
		avail = 1 + result->source->bandwidth / 1680 * 5;
		if (avail > 7)	avail = 7;
	}

	/* notify giFT */
	FST_PROTO->search_result (FST_PROTO, search->gift_event, username,
	                          NULL, href, avail, share);

	free (username);
	free (href);
	share_free (share);

	return TRUE;
}

/*****************************************************************************/
