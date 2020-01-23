/*
 * $Id: as_search.c,v 1.17 2004/12/24 13:52:48 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static as_bool finish_timer_func (ASSearch *search);

static ASPacket *search_query_packet (ASSearch *search);
static ASPacket *search_locate_packet (ASSearch *search);

/*****************************************************************************/

/* create new search */
ASSearch *as_search_create (as_uint16 id, ASSearchResultCb result_cb,
                            const char *query, ASSearchRealm realm)
{
	ASSearch *search;

	if (!(search = malloc (sizeof (ASSearch))))
		return NULL;

	/* hashtable which copies keys */
	if (!(search->results = as_hashtable_create_mem (TRUE)))
	{
		free (search);
		return NULL;
	}

	/* hashtable which supernode ips we already sent the search to */
	if (!(search->sent_supernodes = as_hashtable_create_int (TRUE)))
	{
		as_hashtable_free (search->results, FALSE);
		free (search);
		return NULL;
	}

	search->type         = SEARCH_QUERY;
	search->id           = id;
	search->intern       = FALSE;
	search->finished     = FALSE;
	search->finish_timer = INVALID_TIMER;
	search->result_cb    = result_cb;
	search->udata        = NULL;
	search->query        = gift_strdup (query);
	search->realm        = realm;

	return search;
}

/* create new hash search */
ASSearch *as_search_create_locate (as_uint16 id, ASSearchResultCb result_cb,
                                   ASHash *hash)
{
	ASSearch *search;

	if (!(search = malloc (sizeof (ASSearch))))
		return NULL;

	/* hashtable which copies keys */
	if (!(search->results = as_hashtable_create_mem (TRUE)))
	{
		free (search);
		return NULL;
	}

	/* hashtable which supernode ips we already sent the search to */
	if (!(search->sent_supernodes = as_hashtable_create_int (TRUE)))
	{
		as_hashtable_free (search->results, FALSE);
		free (search);
		return NULL;
	}

	search->type         = SEARCH_LOCATE;
	search->id           = id;
	search->intern       = FALSE;
	search->finished     = FALSE;
	search->finish_timer = INVALID_TIMER;
	search->result_cb    = result_cb;
	search->udata        = NULL;
	search->hash         = as_hash_copy (hash);

	return search;
}

static int result_free_itr (ASResult *result, void *udata)
{
	as_result_free (result);
	return TRUE;
}

static as_bool result_itr (ASHashTableEntry *entry, void *udata)
{
	/* remove entire result list under this key */
	entry->val = list_foreach_remove (entry->val,
	                                  (ListForeachFunc) result_free_itr,
	                                  NULL);
	return TRUE; /* remove entry */
}

/* free search */
void as_search_free (ASSearch *search)
{
	if (!search)
		return;

	/* clear results */
	as_hashtable_foreach (search->results, result_itr, NULL);
	as_hashtable_free (search->results, FALSE);

	as_hashtable_free (search->sent_supernodes, FALSE);
	timer_remove (search->finish_timer);
	
	switch (search->type)
	{
	case SEARCH_QUERY:
		free (search->query);
		break;
	case SEARCH_LOCATE:
		as_hash_free (search->hash);
		break;
	default:
		abort ();
	}

	free (search);
}

/*****************************************************************************/

/* send a query to the specified supernode */
as_bool as_search_send (ASSearch *search, ASSession *session)
{
	ASPacket *packet;
	ASPacketType type;

	/* Don't send finished searches anywhere. */
	if (search->finished)
		return FALSE;

	if (as_session_state (session) != SESSION_CONNECTED)
	{
		AS_ERR_2 ("Tried to send search query to not connected session %s:%d",
		          net_ip_str (session->host), session->port);
		return FALSE;
	}

	switch (search->type)
	{
	case SEARCH_QUERY:
		AS_HEAVY_DBG_3 ("Sending query search \"%s\" to %s:%d", search->query,
		                net_ip_str (session->host), session->port);
		type = PACKET_SEARCH;
		packet = search_query_packet (search);
		break;
	case SEARCH_LOCATE:
		AS_HEAVY_DBG_3 ("Sending hash search \"%s\" to %s:%d",
		                as_hash_str (search->hash),
		                net_ip_str (session->host), session->port);
		type = PACKET_LOCATE;
		packet = search_locate_packet (search);
		break;
	default:
		abort ();
	}

	if (!packet)
	{
		AS_ERR ("Packet creation failed for search query.");
		return FALSE;
	}

	/* send packet */
	if (!as_session_send (session, type, packet, PACKET_ENCRYPT))
	{
		AS_ERR ("as_session_send failed for search query");
		as_packet_free (packet);
		return FALSE;
	}

	as_packet_free (packet);

	/* Add supernode's ip to hash table. Note the gross hack to make the
	 * value non-NULL.
	 */
	as_hashtable_insert_int (search->sent_supernodes,
	                         (as_uint32)session->host, (void*)0xCCCC);

	/* Start search timeout if not already done. */
	if (search->finish_timer == INVALID_TIMER)
	{
		search->finish_timer = timer_add (AS_CONF_INT (AS_SEARCH_TIMEOUT) * SECONDS,
		                                  (TimerCallback)finish_timer_func,
		                                  search);
	}
	
	return TRUE;
}

/* returns number of supernodes this search was sent to */
unsigned int as_search_sent_count (ASSearch *search)
{
	return as_hashtable_size (search->sent_supernodes);
}

/* returns TRUE if the search was already sent to this supernode */
as_bool as_search_sent_to (ASSearch *search, ASSession *session)
{
	return as_hashtable_lookup_int (search->sent_supernodes,
	                                (as_uint32)session->host) != NULL;
}

/*****************************************************************************/

/* Called by timer set up on first send query. */
static as_bool finish_timer_func (ASSearch *search)
{
	search->finish_timer = INVALID_TIMER;

	/* End search. */
	as_search_add_result (search, NULL);

	return FALSE; /* Remove timer. */
}

/* Add result to search and raise result callback. If result is NULL the
 * search is labeled as finished and the callback is raised a final time to
 * reflect that 
 */
void as_search_add_result (ASSearch *search, ASResult *result)
{
	List *head, *l;
	as_bool duplicate = FALSE;

	/* Don't add results to finished searches. */
	if (search->finished)
		return;

	if (!result)
	{
		/* stop search */
		search->finished = TRUE;

		/* Remove timeout timer if there is one. */
		timer_remove_zero (&search->finish_timer);
	}
	else
	{
		/* find list of results for this hash if there is one */
		head = as_search_get_results (search, result->hash);

		/* Ignore duplicate results from same user */
		for (l = head; l; l = l->next)
		{
			if (as_source_equal (result->source, ((ASResult *)l->data)->source))
			{
#if 0
				AS_HEAVY_DBG_1 ("Ignoring duplicate result from %s",
				                net_ip_str (result->source->host));
#endif
				as_result_free (result);
				return;
			}
		}

		duplicate = head ? TRUE : FALSE;

		/* Link in this result or create list.
		 * FIXME: Can optimize this by inserting as second link for multiple
		 * results so we do not have to change the hashtable again below.
		 */
		head = list_prepend (head, result);

		/* and insert/update the hash table since head has changed */
		if (!as_hashtable_insert (search->results, result->hash->data,
		                          AS_HASH_SIZE, head))
		{
			AS_ERR ("Hashtable insert failed for search result");
			/* Remove link from front leaving the list in the same state it
			 * was before so the hashtable entry still points to the head.
			 */
			head = list_remove_link (head, head); 
			/* Free result to prevent memory leaks. */
			as_result_free (result);
	
			assert (0);
			return;
		}
	}

	/* raise callback */
	if (search->result_cb)
	{
		/* the callback may free the search */
		search->result_cb (search, result, duplicate);
	}
}

/* Get list of search result for this file hash. Caller must not modify list
 * in any way.
 */
List *as_search_get_results (ASSearch *search, ASHash *hash)
{
	List *head;

	/* find list of results for this hash if there is one */
	head = as_hashtable_lookup (search->results, hash->data, AS_HASH_SIZE);

	return head;
}

/*****************************************************************************/

static ASPacket *search_query_packet (ASSearch *search)
{
	ASPacket *packet;

	assert (search->type == SEARCH_QUERY);

	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory.");
		return NULL;
	}

	/* realm */
	as_packet_put_8 (packet, (as_uint8) search->realm);
	/* no idea what this magic number is */
	as_packet_put_8 (packet, 0x0f);
	as_packet_put_le16 (packet, search->id);

	/* Add query tokens.
	 * TODO: Pass realm somehow. Check if current realm constants are actually
	 * media type and search realms are something else.
	 */
	if (as_tokenize_search (packet, search->query) == 0)
	{
		/* Still send the query for now so we do not have to propagate this
		 * special case up the call chain to the search manager.
		 */
#if 0
		/* if we have no tokens we can abort the search
		 * immediately (unlike Ares, which sends the packet
		 * regardless...)
		 */	
		as_packet_free (packet);
		return NULL;
#endif
	}

	return packet;
}

static ASPacket *search_locate_packet (ASSearch *search)
{
	ASPacket *packet;

	assert (search->type == SEARCH_LOCATE);

	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory.");
		return NULL;
	}

	/* hash */
	as_packet_put_ustr (packet, search->hash->data, AS_HASH_SIZE);
	as_packet_put_8 (packet, 0x00);

	return packet;
}

/*****************************************************************************/
