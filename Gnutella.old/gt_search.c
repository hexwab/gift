/*
 * $Id: gt_search.c,v 1.25 2003/07/13 07:12:34 hipnod Exp $
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

/* how often we check if the search has timed out */
#define   TIMEOUT_CHECK_INTERVAL (20 * SECONDS)

#define   SEARCH_TTL             (5)

/* after this many results, no more search submissions will occur */
#define   RESULTS_BACKOFF        (200)

/* 
 * Gnutella searches don't notify when they are done. So, we close the
 * search after the following critieria are met:
 *
 *   - we have submitted the search to at least 3 Ultrapeers 
 *     [MIN_NODES]
 *   - at least 3 minutes have passed since we last submitted to an ultrapeer
 *     [MIN_SUBMIT_WAIT]
 *   - no results have been seen in the last minute
 *     [MIN_RESULT_WAIT]
 *
 * This means the fastest we'll time out a search is 3 minutes if
 * we submit to 3 ultrapeers immediately and get no results within
 * 1 minute of the 3 minute time limit.
 *
 * For hash searches, we wait for 2 * MIN_AGE, because the other factors
 * won't come into play.
 */
#define MIN_NODES             (3)             /* ultrapeers */
#define MIN_SUBMIT_WAIT       (3 * EMINUTES)
#define MIN_RESULT_WAIT       (1 * EMINUTES)

/******************************************************************************/

static List *active_searches;

/******************************************************************************/

/* 
 * search_timeout: check if the search needs to be closed.
 *
 * Its impossible to guarantee this will not close the search too early.
 * It is more likely to miss results if bandwidth is being dedicated to
 * other purposes besides reading Gnutella messages, or if the TTL and
 * consequently the latency of the search is high.
 *
 * TODO: this should take into account that we may have disconnected
 *       from the nodes we submitted the search to. Perhaps, have
 *       a list of the submitted nodes, and make sure the list len >=
 *       MIN_NODES (but this may run into trouble with not submitting
 *       searches with results >= RESULTS_BACKOFF...)
 */
static int search_timeout (GtSearch *search)
{
	time_t now;
	time_t submit_wait;
	time_t result_wait;

	if (search->submitted < MIN_NODES)
		return TRUE;

	now = time (NULL);

	submit_wait = MIN_SUBMIT_WAIT;
	result_wait = MIN_RESULT_WAIT;
	
	/* hash searches get very few results, so give them a longer base time */
	if (search->type == GT_SEARCH_HASH)
		submit_wait *= 2;

	/* 
	 * If the search has lots of results, don't wait as long.
	 *
	 * RESULTS_BACKOFF is a conservative value for not submitting to other
	 * nodes when we already have plenty of results, and we want to be a
	 * little less conservative here, so multiply RESULTS_BACKOFF by 2.
	 */
	if (search->results >= 2 * RESULTS_BACKOFF)
	{
		submit_wait /= 2;
		result_wait /= 2;
	}

	if (now - search->last_submit < submit_wait)
		return TRUE;

	if (now - search->last_result < result_wait)
		return TRUE;

	/* remove the timer by returning FALSE from this function, 
	 * instead of in gt_search_free */
	search->timeout_timer = 0;

	GT->DBGFN (GT, "search query '%s' timed out", search->query);
	gt_search_free (search);

	return FALSE;
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
	search->start   = time (NULL);

	search->timeout_timer = timer_add (TIMEOUT_CHECK_INTERVAL, 
	                                   (TimerCallback)search_timeout,
	                                   search);

	active_searches = list_prepend (active_searches, search);

	return search;
}

void gt_search_free (GtSearch *search)
{
	if (!search)
		return;

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
	if (firewalled && GT_SELF->firewalled)
		return;

	if (!(share = share_get_udata (file, GT->name)))
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

	gnutella_p->search_result (gnutella_p, search->event, host_str, server,
	                           url, /*"SHA1",*/ availability, file);

	/* update statistics */
	search->results++;
	search->last_result = time (NULL);

	free (host_str);
	free (url);
}

/******************************************************************************/

static TCPC *broadcast_search (TCPC *c, GtNode *node, GtSearch *search)
{
	GtQueryFlags flags;

	/* set this query as having flags to be interpolated */
	flags = QF_HAS_FLAGS;

	/* set ourselves as firewalled so firewalled servents do not reply
	 * NOTE: if we ever support a download proxy, need to unset this */
	if (GT_SELF->firewalled)
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

	/* TODO: check error return from gt_packet_send_fmt! */
	search->submitted++;
	search->last_submit = time (NULL);

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

static int reset_submit (GtSearch *search, time_t *now)
{
	if (search->results >= RESULTS_BACKOFF)
		return FALSE;

	search->last_submit = *now;
	return FALSE;
}

void gt_searches_submit (TCPC *c, time_t delay_ms)
{
	time_t now;

	/* reset each search timeout because we will submit each search soon */
	time (&now);
	list_foreach (active_searches, (ListForeachFunc)reset_submit, &now);

	/* submit the searches once after a delay */
	if (!GT_NODE(c)->search_timer)
	{
		GT_NODE(c)->search_timer = timer_add (delay_ms,
		                                   (TimerCallback)submit_searches, c);
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

	gt_conn_foreach ((ConnForeachFunc) broadcast_search, search, GT_NODE_NONE,
	                 GT_NODE_CONNECTED, 0);

	return TRUE;
}

void gnutella_search_cancel (Protocol *p, IFEvent *event)
{
	gt_search_disable (event);
}

/* vi: set ts=4 sw=4: */
