/*
 * $Id: gt_search.c,v 1.56 2004/03/31 08:59:46 hipnod Exp $
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

#include "gt_node.h"
#include "gt_node_list.h"
#include "gt_share.h"
#include "gt_share_file.h"
#include "gt_packet.h"
#include "gt_search.h"
#include "gt_xfer.h"

#include "sha1.h"

#include "encoding/url.h"           /* gt_url_decode */

#include "transfer/download.h"
#include "transfer/source.h"

#include <libgift/mime.h>

/******************************************************************************/

/* how often we check if the search has timed out */
#define   TIMEOUT_CHECK_INTERVAL (20 * SECONDS)

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
 * For hash searches, we wait for 2 * MIN_SUBMIT_WAIT, because the other
 * factors won't come into play.
 *
 * There is also a large timeout for searches that receive no results
 * [ANCIENT_TIME].  Searches that exceed this age and haven't received
 * any results in the same time will automatically be cancelled, regardless of
 * other critieria.
 */
#define MIN_NODES             (3)             /* ultrapeers */
#define MIN_SUBMIT_WAIT       (3 * EMINUTES)
#define MIN_RESULT_WAIT       (1 * EMINUTES)
#define ANCIENT_TIME          (10 * EMINUTES)

/******************************************************************************/

/* active keyword and hash searches from this node */
static List   *active_searches;

/* probability of the next hash search not being dropped */
static double  locate_pass_prob;

/******************************************************************************/

static BOOL finish_search (GtSearch *search)
{
	GT->DBGFN (GT, "search query for \"%s\" timed out", search->query);
	gt_search_free (search);
	return FALSE;
}

static BOOL search_is_ancient (GtSearch *search, time_t now)
{
	if (difftime (now, search->start) < ANCIENT_TIME)
		return FALSE;

	/*
	 * If the search is greater than ANCIENT_TIME and hasn't received
	 * a result in the same time, consider it ancient.
	 */
	if (search->last_result == 0)
		return TRUE;

	if (difftime (now, search->last_result) >= ANCIENT_TIME)
		return TRUE;

	return FALSE;
}

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
static BOOL search_timeout (GtSearch *search)
{
	time_t now;
	double submit_wait;
	double result_wait;

	time (&now);

	/* check if this search is really old and should be expired */
	if (search_is_ancient (search, now))
		return finish_search (search);

	if (search->submitted < MIN_NODES)
		return TRUE;

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

	if (difftime (now, search->last_submit) < submit_wait)
		return TRUE;

	if (difftime (now, search->last_result) < result_wait)
		return TRUE;

	/* the search has timed out */
	return finish_search (search);
}

/*****************************************************************************/

GtSearch *gt_search_new (IFEvent *event, char *query, gt_search_type_t type)
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

	GT->DBGFN (GT, "new search \"%s\"", query);

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
		GT->search_complete (GT, search->event);

	/* NOTE: search_complete may have removed the search by calling
	 * gt_search_disable */
	active_searches = list_remove (active_searches, search);

	free (search->hash);
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

	if (strstr (mime, search->realm))
		return TRUE;

	if (!STRCMP (search->realm, "text"))
	{
		/* HACK: special case application/pdf */
		if (strstr (mime, "pdf"))
			return TRUE;

		/* HACK: special case application/msword */
		if (strstr (mime, "doc"))
			return TRUE;
	}

	return FALSE;
}

static BOOL search_matches_hash (GtSearch *search, Share *file)
{
	Hash *hash;
	char *str;
	int   ret;

	if (search->type != GT_SEARCH_HASH)
		return TRUE;

	GT->DBGFN (GT, "got result for hash query");

	if (!(hash = share_get_hash (file, "SHA1")))
	{
		GT->DBGFN (GT, "bad result for hash query");
		return FALSE;
	}

	if (!(str = hash_dsp (hash)))
		return FALSE;

	ret = strcmp (search->hash, hashstr_data (str));
	GT->DBGFN (GT, "text=%s hash=%s", search->hash, str);

	free (str);

	return (ret == 0);
}

/*
 * We have to filter out backslashes from the name to workaround a bug
 * in lib/file.c.
 */
static void set_display_name (Share *share, const char *path)
{
	char *p;
	char *disp_name;

	if (!(p = disp_name = STRDUP (path)))
		return;

	while (*p)
	{
		if (*p == '\\')
			*p = '_';
		p++;
	}

	/* NOTE: this makes the GtShare->filename invalid because it shares memory
	 * with the Share */
	share_set_path (share, disp_name);
	free (disp_name);
}

void gt_search_reply (GtSearch *search, TCPC *c, in_addr_t host,
                      in_port_t gt_port, gt_guid_t *client_guid,
                      int availability, BOOL firewalled,
                      FileShare *file)
{
	char       server[128];
	char      *url;
	char      *host_str;
	char      *path;
	GtShare   *share;
	GtNode    *node;
	BOOL       is_local;

	node = GT_NODE(c);

	if (!search->event)
		return;

	if (gt_is_local_ip (host, node->ip))
		is_local = TRUE;
	else
		is_local = FALSE;

	/* derive firewalled status if the address is local */
	if (is_local)
		firewalled = TRUE;

	/* if they are firewalled and so are we, don't bother.
	 * NOTE: if we have a download proxy, we shouldnt do this */
	if (firewalled && GT_SELF->firewalled)
		return;

	if (!(share = share_get_udata (file, GT->name)))
		return;

	/* check if the mimetype for the result matches the query (i.e. this does
	 * client-side filtering) */
	if (!search_matches_realm (search, share))
		return;

	/* match against the hash if this is a hash search */
	if (!search_matches_hash (search, file))
		return;

	/* get the whole path (result may have '/' separators) */
	path = file->path;
	assert (path != NULL);

	url = gt_source_url_new (path, share->index, host, gt_port,
	                         node->ip, node->gt_port,
	                         firewalled, client_guid);

	if (!url)
		return;

	/* workaround bug in lib/file.c */
	set_display_name (file, path);

	/* print out the server data so we know which connection to
	 * talk to when sending a push request */
	snprintf (server, sizeof (server) - 1, "%s:%hu",
	          net_ip_str (node->ip), node->gt_port);

	if (is_local)
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

	GT->search_result (GT, search->event, host_str, server,
	                   url, availability, file);

	/* update statistics */
	search->results++;
	time (&search->last_result);

	free (host_str);
	free (url);
}

/******************************************************************************/

static uint8_t get_search_ttl (GtNode *node, gt_search_type_t type)
{
	char     *max_ttl;
	uint8_t   ttl     = 0;

	if ((max_ttl = dataset_lookupstr (node->hdr, "x-max-ttl")))
		ttl = ATOI (max_ttl);

	if (ttl > GT_SEARCH_TTL || ttl == 0)
		ttl = GT_SEARCH_TTL;

	/* ok because locates are rate-limited */
#if 0
	if (type == GT_SEARCH_HASH)
		ttl = 1;
#endif

	return ttl;
}

static TCPC *broadcast_search (TCPC *c, GtNode *node, GtSearch *search)
{
	gt_query_flags_t flags;
	uint8_t          ttl;
	char            *hash = NULL;
	GtPacket        *pkt;

	/* set this query as having flags to be interpolated */
	flags = QF_HAS_FLAGS;

	/* request that only non-firewalled nodes respond if we are firewalled
	 * NOTE: if we ever support a download proxy, need to unset this */
	if (GT_SELF->firewalled)
		flags |= QF_ONLY_NON_FW;

#ifdef USE_LIBXML2
	flags |= QF_WANTS_XML;
#endif /* USE_LIBXML2 */

	ttl = get_search_ttl (node, search->type);

	if (search->type == GT_SEARCH_HASH && !search->hash)
	{
		GT->DBGFN (GT, "trying to search for \"%s\" without a hash?!?",
		           search->query);
		return NULL;
	}

	if (!(pkt = gt_packet_new (GT_MSG_QUERY, ttl, search->guid)))
		return NULL;

	gt_packet_put_uint16 (pkt, flags);
	gt_packet_put_str    (pkt, search->query);

	if (search->hash)
		hash = stringf_dup ("urn:sha1:%s", search->hash);

	if (hash)
		gt_packet_put_str (pkt, hash);

	gt_packet_send (c, pkt);
	gt_packet_free (pkt);

	free (hash);

	/* TODO: check error return from gt_packet_send_fmt! */
	search->submitted++;
	time (&search->last_submit);

	return NULL;
}

static BOOL submit_search (GtSearch *search, TCPC *c)
{
	if (search->results >= RESULTS_BACKOFF)
	{
		/* still count the search as submitted to this node */
		search->submitted++;
		return FALSE;
	}

	broadcast_search (c, GT_NODE(c), search);
	return FALSE;
}

static BOOL submit_searches (TCPC *c)
{
	list_foreach (active_searches, (ListForeachFunc)submit_search, c);
	GT_NODE(c)->search_timer = 0;
	return FALSE;
}

static BOOL reset_submit (GtSearch *search, time_t *now)
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

BOOL gnutella_search (Protocol *p, IFEvent *event, char *query, char *exclude,
                      char *realm, Dataset *meta)
{
	GtSearch *search;

	search = gt_search_new (event, query, GT_SEARCH_KEYWORD);
	search->realm = STRDUP (realm);

	gt_conn_foreach (GT_CONN_FOREACH(broadcast_search), search,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);

	return TRUE;
}

/*****************************************************************************/

/*
 * Using the hash, grab words to stuff in the query section by looking at the
 * download list.
 */
char *get_query_words (char *htype, char *hash)
{
	Source   *src;
	GtSource *gt_src;
	char     *dup;

	if (htype && strcmp (htype, "SHA1") != 0)
	{
		GT->DBGFN (GT, "htype != \"SHA1\"!?: %s", htype);
		return NULL;
	}

	/* HACK: need gift's prefix */
	if (!(dup = stringf_dup ("SHA1:%s", hash)))
		return NULL;

	src = gt_download_lookup (dup);
	free (dup);

	if (!src)
		return NULL;

	if (!(gt_src = src->udata))
	{
		GT->DBGFN (GT, "gt_src == NULL?!?!");
		return NULL;
	}

	return gt_url_decode (gt_src->filename);
}

/*
 * Returns TRUE if the current locate is ok to send and FALSE if it should be
 * dropped to rate-limit locates.  To determine that, we assign the locate a
 * "probability of passage".  Then we roll dice and if it's less than the
 * probability, accept.
 *
 * For each locate attempt the probability of success for the next locate is
 * halved, down to a minimum of 0.01%.  For each minute that passes since the
 * last locate, the probability of the locate succeeding increases by 1%.
 */
static BOOL should_send_locate (void)
{
	static time_t last_locate;
	time_t        now;
	double        n;
	BOOL          passed;

	time (&now);
	locate_pass_prob += difftime (now, last_locate) / (1.0 * EMINUTES);
	last_locate = now;

	if (locate_pass_prob > 100.0)
		locate_pass_prob = 100.0;

	/* hmm, should this be removed? */
	if (locate_pass_prob < 0.01)
		locate_pass_prob = 0.01;

	n = 100.0 * rand() / (RAND_MAX + 1.0);

	GT->DBGFN (GT, "locate_pass_prob=%f n=%f", locate_pass_prob, n);
	passed = BOOL_EXPR (n < locate_pass_prob);

	/* drop next chance of succeeding */
	locate_pass_prob /= 2;

	return passed;
}

BOOL gnutella_locate (Protocol *p, IFEvent *event, char *htype, char *hash)
{
	GtSearch      *search;
	unsigned char *bin;
	char          *fname;

	/* Only locate hashes which are valid on Gnutella. */
	if (STRCMP (htype, "SHA1"))
		return FALSE;

	GT->DBGFN (GT, "new hash search: %s", hash);

	/* sha1_bin() needs a string of at least 32 characters */
	if (STRLEN (hash) < 32)
		return FALSE;

	/* skip the hash if it's not parseable in base32 */
	if (!(bin = sha1_bin (hash)))
		return FALSE;

	free (bin);

	/* rate-limit locate searches to save bandwidth */
	if (should_send_locate () == FALSE)
	{
		GT->DBGFN (GT, "dropping locate for %s "
		           "(too many searches in short period)", hash);
		return FALSE;
	}

	/* make sure the hash is uppercase (canonical form on Gnet) */
	string_upper (hash);

	/*
	 * Look for a download with this hash, to put those words in the query to
	 * reduce the bandwidth consumed through QRP.
	 */
	if (!(fname = get_query_words (htype, hash)))
		fname = STRDUP ("");

	if (!(search = gt_search_new (event, fname, GT_SEARCH_HASH)))
	{
		free (fname);
		return FALSE;
	}

	free (fname);

	search->hash = STRDUP (hash);

	gt_conn_foreach (GT_CONN_FOREACH(broadcast_search), search, GT_NODE_NONE,
	                 GT_NODE_CONNECTED, 0);

	return TRUE;
}

void gnutella_search_cancel (Protocol *p, IFEvent *event)
{
	gt_search_disable (event);
}

/*****************************************************************************/

void gt_search_init (void)
{
	/* nothing */
}

BOOL rm_search (GtSearch *search, void *udata)
{
	gt_search_free (search);

	/* return FALSE here because gt_search_free() removes search from list */
	return FALSE;
}

void gt_search_cleanup (void)
{
	list_foreach_remove (active_searches, (ListForeachFunc)rm_search, NULL);
	assert (active_searches == NULL);
}
