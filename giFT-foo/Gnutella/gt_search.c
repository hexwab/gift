/*
 * $Id: gt_search.c,v 1.24 2003/06/07 05:46:31 hipnod Exp $
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

#include "src/mime.h"      /* mime_type() */

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
static int search_timeout (GtSearch *search)
{
	GT->DBGFN (GT, "search query '%s' timed out", search->query);

	search->timeout_timer = 0;
	gt_search_free (search);

	return FALSE;
}

static void reset_timeout (GtSearch *search, time_t interval)
{
	timer_remove (search->timeout_timer);

	search->timeout_timer = timer_add (interval, (TimerCallback)search_timeout,
	                                   search);
}

GtSearch *gt_search_new (IFEvent *event, char *query, GtSearchType type)
{
	GtSearch *search;

	if (!(search = malloc (sizeof (GtSearch))))
		return NULL;

	memset (search, 0, sizeof (GtSearch));

	search->event   = event;
	search->type    = type;
	search->guid    = gt_guid_new ();
	search->query   = STRDUP (query);
	search->results = 0;

	reset_timeout (search, TIMEOUT_INTERVAL);

	active_searches = list_prepend (active_searches, search);

	return search;
}

void gt_search_free (GtSearch *search)
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

	free (search->realm);
	free (search->guid);
	free (search->query);
	free (search);
}

static int find_by_event (GtSearch *search, IFEvent *event)
{
	if (search->event == event)
		return 0;

	return -1;
}

void gt_search_disable (IFEvent *event)
{
	List      *ls;
	GtSearch  *search;

	ls = list_find_custom (active_searches, event,
	                       (CompareFunc) find_by_event);

	if (!ls)
	{
		GT->DBGFN (GT, "didnt find search id %p", (long) event);
		return;
	}

	search = ls->data;

	GT->DBGFN (GT, "disabled search event %p (query '%s')", event, search->query);
	search->event = NULL;
}

/******************************************************************************/

static int find_by_guid (GtSearch *a, GtSearch *b)
{
	return gt_guid_cmp (a->guid, b->guid);
}

GtSearch *gt_search_find (gt_guid_t *guid)
{
	GtSearch  key;
	List     *l;

	key.guid = guid;

	l = list_find_custom (active_searches, &key, (CompareFunc) find_by_guid);

	if (!l)
		return NULL;

	return l->data;
}

static BOOL search_matches_realm (GtSearch *search, GtShare *share)
{
	char *mime;

	if (!search->realm)
		return TRUE;

	if (!(mime = mime_type (share->filename)))
		return FALSE;

	if (!strstr (mime, search->realm))
		return FALSE;

	return TRUE;
}

void gt_search_reply (GtSearch *search, TCPC *c, in_addr_t host,
                      in_port_t gt_port, gt_guid_t *client_guid,
                      int availability, int firewalled,
                      FileShare *file)
{
	char       server[128];
	char      *url;
	char      *host_str;
	GtShare   *share;

	if (!search->event)
		return;

	/* if they are firewalled and so are we, don't bother.
	 * NOTE: if we have a download proxy, we shouldnt do this */
	if (firewalled && gt_self->firewalled)
		return;

	if (!(share = share_get_udata (file, gnutella_p->name)))
		return;

	/* check if the mimetype for the result matches the query 
	 * (i.e. this does client-side filtering) */
	if (!search_matches_realm (search, share))
		return;

#if 0
	if (!(url = gt_share_url_new (share, host, gt_port, client_guid)))
		return;
#endif
	url = gt_share_url_new (share, host, gt_port,
	                        GT_NODE(c)->ip, GT_NODE(c)->gt_port,
	                        firewalled, client_guid);

	if (!url)
		return;

	/* print out the server data so we know which connection to
	 * talk to when sending a push request */
	sprintf (server, "%s:%hu", net_ip_str (GT_NODE(c)->ip), GT_NODE(c)->gt_port);

	if (gt_is_local_ip (host, GT_NODE(c)->ip))
	{
		/* use the Client GUID for the user if the remote connection is
		 * on the Internet and the host is 0 or local */
		host_str = stringf_dup ("%s@%s", net_ip_str (host),
		                        gt_guid_str (client_guid));
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

static TCPC *broadcast_search (TCPC *c, GtNode *node, GtSearch *search)
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

static int submit_search (GtSearch *search, TCPC *c)
{
	/* don't submit the search if we already got "enough" results */
	if (search->results >= RESULTS_BACKOFF)
		return FALSE;

	broadcast_search (c, GT_NODE(c), search);
	return FALSE;
}

static int submit_searches (TCPC *c)
{
	list_foreach (active_searches, (ListForeachFunc) submit_search, c);
	GT_NODE(c)->search_timer = 0;
	return FALSE;
}

void gt_searches_submit (TCPC *c, time_t delay_ms)
{
	/* submit the searches once after a delay */
	if (!GT_NODE(c)->search_timer)
	{
		GT_NODE(c)->search_timer = timer_add (delay_ms,
		                                   (TimerCallback) submit_searches, c);
	}
}

/******************************************************************************/

int gnutella_search (Protocol *p, IFEvent *event, char *query, char *exclude,
                     char *realm, Dataset *meta)
{
	GtSearch *search;

	search = gt_search_new (event, query, GT_SEARCH_KEYWORD);
	search->realm = STRDUP (realm);

	gt_conn_foreach ((ConnForeachFunc) broadcast_search, search,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);

	return TRUE;
}

int gnutella_locate (Protocol *p, IFEvent *event, char *htype, char *hash)
{
	GtSearch *search;

	/* make sure the hash is uppercase (canonical form on Gnet) */
	string_upper (hash);

	if (!(search = gt_search_new (event, hash, GT_SEARCH_HASH)))
		return FALSE;

	/* source searches get almost no results. So, make the timeout on 
	 * them longer. */
	reset_timeout (search, 7 * MINUTES);

	gt_conn_foreach ((ConnForeachFunc) broadcast_search, search, GT_NODE_NONE,
	                 GT_NODE_CONNECTED, 0);

	return TRUE;
}

void gnutella_search_cancel (Protocol *p, IFEvent *event)
{
	gt_search_disable (event);
}

/* vi: set ts=4 sw=4: */
