/*
 * $Id: gt_search.c,v 1.9 2003/05/04 09:16:13 hipnod Exp $
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

#include "gt_node.h"
#include "gt_netorg.h"

#include "gt_share.h"
#include "gt_share_file.h"

#include "gt_packet.h"
#include "gt_protocol.h"

#include "gt_search.h"

#include "gt_xfer.h"

/******************************************************************************/

/* searches timeout after no results have been received in this time */
#define   TIMEOUT_INTERVAL     (210 * SECONDS)

#define   SEARCH_TTL            5

/* after this many results, no more search submissions will occur */
#define   RESULTS_BACKOFF       200

/******************************************************************************/

static List *active_searches;

/******************************************************************************/

/* Gnutella searches don't notify when they are done.  So close the
 * search after no results have come back for a certain time */
static int search_timeout (Gt_Search *search)
{
	TRACE (("search query '%s' timed out", search->query));

	search->timeout_timer = 0;
	gt_search_free (search);

	return FALSE;
}

Gt_Search *gt_search_new (IFEvent *event, char *query, GtSearchType type)
{
	Gt_Search *search;

	if (!(search = malloc (sizeof (Gt_Search))))
		return NULL;

	memset (search, 0, sizeof (Gt_Search));

	search->event   = event;
	search->type    = type;
	search->guid    = guid_new ();
	search->query   = STRDUP (query);
	search->results = 0;

	search->timeout_timer = timer_add (TIMEOUT_INTERVAL,
	                                   (TimerCallback) search_timeout, search);

	active_searches = list_prepend (active_searches, search);

	return search;
}

void gt_search_free (Gt_Search *search)
{
	if (!list_find (active_searches, search))
	{
		GIFT_ERROR (("couldn't find search %p (query:'%s')",
		              search, search->query));
		return;
	}

	if (search->timeout_timer)
		timer_remove (search->timeout_timer);

	if (search->event)
		gnutella_p->search_complete (gnutella_p, search->event);

	/* NOTE: search_complete may have removed the search by calling
	 * gt_search_disable */
	active_searches = list_remove (active_searches, search);

	free (search->guid);
	free (search->query);
	free (search);
}

static int find_by_event (Gt_Search *search, IFEvent *event)
{
	if (search->event == event)
		return 0;

	return -1;
}

void gt_search_disable (IFEvent *event)
{
	List      *ls;
	Gt_Search *search;

	ls = list_find_custom (active_searches, event,
	                       (CompareFunc) find_by_event);

	if (!ls)
	{
		TRACE (("didnt find search id %p", (long) event));
		return;
	}

	search = ls->data;

	TRACE (("disabled search event %p (query '%s')", event, search->query));
	search->event = NULL;
}

/******************************************************************************/

static int find_by_guid (Gt_Search *a, Gt_Search *b)
{
	return guid_cmp (a->guid, b->guid);
}

Gt_Search *gt_search_find (gt_guid *guid)
{
	Gt_Search key;
	List     *l;

	key.guid = guid;

	l = list_find_custom (active_searches, &key, (CompareFunc) find_by_guid);

	if (!l)
		return NULL;

	return l->data;
}

void gt_search_reply (Gt_Search *search, Connection *c, unsigned long host,
                      unsigned short gt_port, gt_guid *client_guid,
                      int availability, int firewalled,
                      FileShare *file)
{
	char       server[128];
	char      *url;
	char      *host_str;
	Gt_Share  *share;

	if (!search->event)
		return;

	/* if they are firewalled and so are we, don't bother.
	 * NOTE: if we have a download proxy, we shouldnt do this */
	if (firewalled && gt_self->firewalled)
		return;

	if (!(share = share_lookup_data (file, gnutella_p->name)))
		return;

#if 0
	if (!(url = gt_share_url_new (share, host, gt_port, client_guid)))
		return;
#endif
	url = gt_share_url_new (share, host, gt_port,
	                        NODE(c)->ip, NODE(c)->gt_port, 
	                        firewalled, client_guid);

	if (!url)
		return;

	/* print out the server data so we know which connection to
	 * talk to when sending a push request */
	sprintf (server, "%s:%hu", net_ip_str (NODE(c)->ip), NODE(c)->gt_port);

	if (gt_is_local_ip (host, GT_NODE(c)->ip))
	{
		/* use the Client GUID for the user if the remote connection is 
		 * on the Internet and the host is 0 or local */
		host_str = stringf_dup ("%s@%s", net_ip_str (host), 
		                        guid_str (client_guid));
	}
	else
	{
		/* Just use a plain host for cleanliness */
		host_str = stringf_dup ("%s", net_ip_str (host));
	}

	search->results++;
	gnutella_p->search_result (gnutella_p, search->event, host_str, server,
	                           url, /*"SHA1",*/ availability, file);

	timer_reset (search->timeout_timer);

	free (host_str);
	free (url);
}

/******************************************************************************/

static Connection *broadcast_search (Connection *c, Gt_Node *node,
                                     Gt_Search *search)
{
	GtQueryFlags flags;

	/* reset the timeout for this search */
	timer_reset (search->timeout_timer);

	/* set this query as having flags to be interpolated */
	flags = QF_HAS_FLAGS;

	/* set ourselves as firewalled so firewalled servents do not reply
	 * NOTE: if we ever support a download proxy, need to unset this */
	if (gt_self->firewalled)
		flags |= QF_FIREWALLED;

	/* TODO: parse search->query to make sure its valid */
	switch (search->type)
	{
	 case GT_SEARCH_KEYWORD:
		gt_packet_send_fmt (c, GT_QUERY_REQUEST, search->guid, SEARCH_TTL,
		                    0, "%hu%s%s", flags, search->query, "urn::");
		break;
	 case GT_SEARCH_HASH:
		gt_packet_send_fmt (c, GT_QUERY_REQUEST, search->guid, SEARCH_TTL,
		                    0, "%hu%s%s", flags, "",
		                    stringf ("urn:sha1:%s", search->query));
		break;
	}

	return NULL;
}

static int submit_search (Gt_Search *search, Connection *c)
{
	/* don't submit the search if we already got "enough" results */
	if (search->results >= RESULTS_BACKOFF)
		return FALSE;

	broadcast_search (c, NODE(c), search);
	return FALSE;
}

static int submit_searches (Connection *c)
{
	list_foreach (active_searches, (ListForeachFunc) submit_search, c);
	NODE(c)->search_timer = 0;
	return FALSE;
}

void gt_searches_submit (Connection *c, time_t delay_ms)
{
	/* submit the searches once after a delay */
	if (!NODE(c)->search_timer)
	{
		NODE(c)->search_timer = timer_add (delay_ms,
		                                   (TimerCallback) submit_searches, c);
	}
}

/******************************************************************************/

int gnutella_search (Protocol *p, IFEvent *event, char *query, char *exclude,
                     char *realm, Dataset *meta)
{
	Gt_Search *search;

	search = gt_search_new (event, query, GT_SEARCH_KEYWORD);

	gt_conn_foreach ((ConnForeachFunc) broadcast_search, search,
	                 NODE_NONE, NODE_CONNECTED, 0);

	return TRUE;
}

int gnutella_locate (Protocol *p, IFEvent *event, char *htype, char *hash)
{
	Gt_Search *search;

	/* make sure the hash is uppercase (canonical form on Gnet) */
	string_upper (hash);

	if (!(search = gt_search_new (event, hash, GT_SEARCH_HASH)))
		return FALSE;

	gt_conn_foreach ((ConnForeachFunc) broadcast_search, search, NODE_NONE,
	                 NODE_CONNECTED, 0);

	return TRUE;
}

void gnutella_search_cancel (Protocol *p, IFEvent *event)
{
	gt_search_disable (event);
}

/* vi: set ts=4 sw=4: */
