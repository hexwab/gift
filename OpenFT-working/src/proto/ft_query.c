/*
 * $Id: ft_query.c,v 1.15 2003/11/21 16:05:18 jasta Exp $
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

#include "ft_openft.h"

#if 0
#include "src/share_cache.h"           /* ILLEGAL! */
#else
void share_foreach (DatasetForeachExFn foreach_fn, void *data);
#endif

#include "md5.h"

#include "ft_netorg.h"

#include "ft_search.h"
#include "ft_search_exec.h"

#include "ft_transfer.h"

#include "ft_protocol.h"
#include "ft_query.h"

/*****************************************************************************/

/* manages search requests to minimize the possibility of duplicated queries,
 * see active_search() here */
static Dataset *searches = NULL;
static timer_id search_timer = 0;

/* simple caching from ft_conf.c */
static BOOL query_verbose = FALSE;
static BOOL query_verbose_init = FALSE;

/*****************************************************************************/

/* convenience structure for search requests */
typedef struct
{
	ft_guid_t     *guid;
	in_addr_t      orighost;           /* original sender of the search */
	in_port_t      origport;           /* port for testing firewalled state */
	FTNode        *node;               /* node that requested the search */
	uint16_t       ttl;                /* remaining time-to-live */
	uint16_t       nmax;               /* maximum results desired */
	uint16_t       type;               /* query type */
	void          *query;
	void          *exclude;
	char          *realm;
} sparams_t;

/* convenience structure for search and browse replies */
typedef struct
{
	TCPC      *c;                      /* connection to deliver the result to */
	FTStream  *stream;                 /* optional stream context to use */
	sparams_t *params;                 /* search parameters */
} sreply_t;

/*****************************************************************************/

/* used by ft_search_response to coordinate the different meanings of
 * search results */
typedef void (*ResultHandler) (TCPC *c, FTPacket *pkt, ft_guid_t *guid,
                               void *object);

/*****************************************************************************/

static BOOL auth_search_handle (FTNode *node)
{
#if 0
	if (node->version < OPENFT_0_0_9_6)
	{
		FT->DBGSOCK (FT, FT_CONN(node), "incompatible search (%08x)",
		             (unsigned int)node->version);
		return FALSE;
	}
#endif

	return TRUE;
}

static BOOL auth_search_request (FTNode *node)
{
	if (!(openft->ninfo.klass & FT_NODE_SEARCH))
		return FALSE;

	return auth_search_handle (node);
}

static BOOL auth_search_response (FTNode *node)
{
	if (!(node->ninfo.klass & FT_NODE_SEARCH))
		return FALSE;

	return auth_search_handle (node);
}

/*****************************************************************************/

static int clear_search (ds_data_t *key, ds_data_t *value, unsigned int *rem)
{
	int ret = DS_CONTINUE;

	/* hack to use the length as a sort of flag to tell us to remove after
	 * the second tick here */
	if (value->len++ > 0)
	{
		(*rem)++;
		ret |= DS_REMOVE;
	}

	return ret;
}

static int clear_searches (void *udata)
{
	unsigned int rem = 0;

	/* we do this same kind of thing in ft_search_object:fwd_timeout, look
	 * there instead if you want to learn more */
	dataset_foreach_ex (searches, DS_FOREACH_EX(clear_search), &rem);
	FT->DBGFN (FT, "removed %u searches", rem);

	/* keep the timer alive */
	return TRUE;
}

/*
 * Determines whether or not the supplied search is already actively being
 * processed.  This system is in place to prevent searches from being
 * forwarded back to us through search peers.
 */
static int active_search (sparams_t *params)
{
	unsigned char data[20];
	DatasetNode  *node;

	assert (params->guid != NULL);
	assert (params->orighost != 0);

	/* make sure we're not being asked to reply to a search we requested
	 * as a regular user */
	if (ft_search_find (params->guid))
		return TRUE;

	memcpy (data, params->guid, FT_GUID_SIZE);
	memcpy (data + FT_GUID_SIZE, &params->orighost, sizeof (params->orighost));

	if (dataset_lookup (searches, data, sizeof (data)))
		return TRUE;

	node = dataset_insert (&searches, data, sizeof (data), "guid_orighost", 0);
	assert (node != NULL);

#if 0
	/* the logic here is that we want to timeout md5 searches faster so that
	 * they dont pollute the dataset as much, because avoiding replies isnt
	 * really that important, considering the low expected result set and
	 * quick lookup time */
	if (FT_SEARCH_TYPE(params->type) == FT_SEARCH_MD5)
		timeout = 30 * SECONDS;
	else
		timeout = 4 * MINUTES;
#endif

	/* every so often we need to clear the searches we added here to
	 * avoid wasting memory */
	if (search_timer == 0)
	{
		search_timer =
			timer_add (5 * MINUTES, (TimerCallback)clear_searches, NULL);
	}

	return FALSE;
}

/*****************************************************************************/

static void sreply_init (sreply_t *reply, TCPC *c, FTStream *stream,
                         sparams_t *params)
{
	reply->c      = c;
	reply->stream = stream;
	reply->params = params;
}

static void sreply_finish (sreply_t *reply)
{
	ft_stream_finish (reply->stream);
}

static int sreply_send (sreply_t *reply, FTPacket *pkt)
{
	int ret;

	/* TODO: this API should really be unified somewhere in ft_packet.c */
	if (reply->stream)
		ret = ft_stream_send (reply->stream, pkt);
	else
		ret = ft_packet_send (reply->c, pkt);

	return ret;
}

static void result_add_meta (ds_data_t *key, ds_data_t *value, FTPacket *pkt)
{
	ft_packet_put_str (pkt, key->data);
	ft_packet_put_str (pkt, value->data);
}

static BOOL sreply_result (sreply_t *reply, ft_nodeinfo_t *ninfo,
                           Share *file, unsigned int avail, BOOL verified)
{
	sparams_t *params = reply->params;
	FTPacket  *pkt;
	Hash      *hash;
	char      *path;

	if (!(hash = share_get_hash (file, "MD5")))
		return FALSE;

	/* just in case :) */
	assert (hash->len == 16);

	/*
	 * If an hpath was supplied (local share) we want to keep the output
	 * consistent.  To elaborate, this function will be used with Share
	 * objects pointing directly into our own local memory from
	 * src/share_cache.c, or we have just created a new structure for this
	 * function.  In the event that we are working with a local share, hpath
	 * will be set, otherwise path will be set accordingly.
	 */
	if (!(path = share_get_hpath (file)))
		path = SHARE_DATA(file)->path;

	/* begin packet */
	if (!(pkt = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return FALSE;

	/* add the requested id the search response so that the searcher knows
	 * what this result is */
	ft_packet_put_ustr   (pkt, params->guid, FT_GUID_SIZE);

	/* add space for the nodes forwarding this query to add our ip address, so
	 * that the original parent that replied to the search may be tracked */
	ft_packet_put_ip     (pkt, 0);
	ft_packet_put_uint16 (pkt, openft->ninfo.port_openft, TRUE);

	/* add the address and contact information of the user who owns this
	 * particular search result */
	ft_packet_put_ip     (pkt, ninfo->host);

	if (ninfo->indirect || verified == FALSE)
		ft_packet_put_uint16 (pkt, 0, TRUE);
	else
		ft_packet_put_uint16 (pkt, ninfo->port_openft, TRUE);

	ft_packet_put_uint16 (pkt, ninfo->port_http, TRUE);
	ft_packet_put_str    (pkt, ninfo->alias);

	/* add this users "availability", that is, how many open upload slots thye
	 * currently have */
	ft_packet_put_uint32 (pkt, avail, TRUE);

	/* add the result file information */
	ft_packet_put_uint32 (pkt, (uint32_t)file->size, TRUE);
	ft_packet_put_ustr   (pkt, hash->data, hash->len);
	ft_packet_put_str    (pkt, file->mime);
	ft_packet_put_str    (pkt, path);

	/* add meta data */
	share_foreach_meta (file, DS_FOREACH(result_add_meta), pkt);

	/* off ya go */
	if (sreply_send (reply, pkt) < 0)
		return FALSE;

	return TRUE;
}

static void empty_result (TCPC *c, ft_guid_t *guid)
{
	FTPacket *pkt;

	if (!c)
		return;

	assert (guid != NULL);

	if (!(pkt = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return;

	ft_packet_put_ustr (pkt, guid, FT_GUID_SIZE);
	ft_packet_send (c, pkt);
}

static BOOL search_result_logic (Share *file, sreply_t *reply)
{
	FTShare       *share;
	ft_nodeinfo_t *searchee;
	ft_nodeinfo_t *searcher;
	sparams_t     *params;
	unsigned int   avail;
	BOOL           verified;

	if (!(share = share_get_udata (file, "OpenFT")))
		return FALSE;

	searchee = share->ninfo;
	assert (searchee != NULL);

	/* access the availability, which depends on whether or not this is a
	 * real remote peer sharing the file, or our own local search node */
	if (share->node)
	{
		avail    = share->node->session->avail;
		verified = share->node->session->verified;
	}
	else
	{
		avail    = openft->avail;
		verified = TRUE;
	}

	searcher = FT_NODE_INFO(reply->c);
	assert (searcher != NULL);

	params = reply->params;
	assert (params != NULL);

	/* first drop all results that we matched from the node who requested
	 * the search directly */
	if (searchee->host == searcher->host)
		return FALSE;

	/* next drop all the results that matched the original source of the
	 * search, which may be the same as the condition above, but we cant
	 * guarantee that */
	if (searchee->host == params->orighost)
		return FALSE;

	/* drop firewalled results when we are delivering to firewalled users */
	if (params->origport == 0 && (searchee->indirect || verified == FALSE))
		return FALSE;

	/* now actually send the result */
	return sreply_result (reply, searchee, file, avail, verified);
}

static BOOL search_result (Share *file, sreply_t *reply)
{
	BOOL ret;

	/* special exception */
	if (!file)
		return TRUE;

	/* calling result_reply for better flow control :) */
	ret = search_result_logic (file, reply);
	ft_share_unref (file);

	/* the return value determines whether or not to "accept" this result as
	 * part of the searches nmax */
	return ret;
}

static ft_search_flags_t get_search_type (sparams_t *params)
{
	FTNode            *chk_parent;
	ft_search_flags_t  type;

	type = (ft_search_flags_t)params->type;

	/*
	 * Determine the "real" search type to pass to ft_search.  This logic is
	 * used to optimize away FT_SEARCH_LOCAL when our parent is the searching
	 * node, as we know they can already provide all those results
	 * themselves.  It would be extremely wasteful to ask the network to
	 * carry that along.
	 */
	if (!(chk_parent = ft_netorg_lookup (params->orighost)) ||
	    !(chk_parent->ninfo.klass & FT_NODE_PARENT))
		type |= FT_SEARCH_LOCAL;

	return type;
}

static int exec_search (TCPC *c, sparams_t *params)
{
	sreply_t reply;
	int      n;                        /* number of search results */

	/* initialize the search reply object */
	sreply_init (&reply, c, ft_stream_get (c, FT_STREAM_SEND, NULL), params);

	/* execute the search, calling search_result after each successful
	 * match */
	n = ft_search (params->nmax, (FTSearchResultFn)search_result, &reply,
	               get_search_type (params), params->realm,
	               params->query, params->exclude);

	/* provide debugging information */
	if (FT_SEARCH_METHOD(params->type) == FT_SEARCH_FILENAME)
	{
		char *query_str;

		if (params->type & FT_SEARCH_HIDDEN)
			query_str   = "*hidden*";
		else
			query_str   = params->query;

		/* ugh, i really hate this configuration system we've established
		 * here */
		if (query_verbose_init == FALSE)
		{
			query_verbose_init = TRUE;
			query_verbose = FT_CFG_SEARCH_NOISY;
		}

		if (query_verbose)
		{
			FT->DBGSOCK (FT, c, "[%s:%i]: '%s'...%i/%i result(s)",
			             ft_guid_fmt (params->guid), (int)params->ttl,
			             query_str, n, params->nmax);
		}
	}

	sreply_finish (&reply);

	/* return the number of results found, or -1 on error (as ft_search
	 * dictates) */
	return n;
}

static int forward_search_peer (FTNode *node, sparams_t *params)
{
	FTSearchFwd *sfwd;
	FTPacket    *pkt;

	/* refuse to send the search back to absolute original originator of
	 * the search, that would just be evil */
	if (params->orighost == node->ninfo.host)
		return FALSE;

	/* refuse to send the search back to the person that asked us to forward
	 * the search (if src != orighost) */
	if (params->node == node)
		return FALSE;

	/*
	 * Create the data structure and associatations needed to coordinate the
	 * response from `node' (and send it to srch->src).  We're not really
	 * going to use the handle provided, but we will trust that it was added
	 * to the main list which is managed by a single timer.  This is mostly
	 * used by active_search to ignore forwards we are already processing.
	 */
	sfwd = ft_search_fwd_new (params->guid, params->node->ninfo.host,
	                          node->ninfo.host);

	if (!sfwd)
		return FALSE;

	/* actually send the search */
	if (!(pkt = ft_packet_new (FT_SEARCH_REQUEST, 0)))
		return FALSE;

	/* see ft_search.c:send_search for more information about what all this
	 * jazz means */
	ft_packet_put_ustr   (pkt, params->guid, FT_GUID_SIZE);
	ft_packet_put_ip     (pkt, params->orighost);

	if (node->version >= OPENFT_0_2_0_1)
		ft_packet_put_uint16 (pkt, params->origport, TRUE);

	ft_packet_put_uint16 (pkt, params->ttl, TRUE);
	ft_packet_put_uint16 (pkt, params->nmax, TRUE);
	ft_packet_put_uint16 (pkt, params->type, TRUE);

	if (params->type & FT_SEARCH_HIDDEN)
	{
		ft_packet_put_uarray (pkt, 4, params->query, TRUE);
		ft_packet_put_uarray (pkt, 4, params->exclude, TRUE);
	}
	else
	{
		ft_packet_put_str (pkt, params->query);
		ft_packet_put_str (pkt, params->exclude);
	}

	ft_packet_put_str (pkt, params->realm);

	/* deliver the search query */
	if (ft_packet_send (FT_CONN(node), pkt) < 0)
	{
		ft_search_fwd_finish (sfwd);
		return FALSE;
	}

	return TRUE;
}

/* returns TRUE if the parameters were adjusted, otherwise FALSE */
static BOOL clamp_params (sparams_t *params)
{
	BOOL ret = FALSE;

	if (params->nmax > FT_CFG_MAX_RESULTS)
	{
#if 0
		FT->DBGSOCK (FT, FT_CONN(params->node), "%s: clamped nmax (%hu to %hu)",
		             ft_guid_fmt (params->guid),
		             (unsigned short)params->nmax, FT_CFG_MAX_RESULTS);
#endif

		params->nmax = FT_CFG_MAX_RESULTS;
		ret = TRUE;
	}

	if (params->ttl > FT_CFG_MAX_TTL)
	{
#if 0
		FT->DBGSOCK (FT, FT_CONN(params->node), "%s: clamped ttl (%hu to %hu)",
		             ft_guid_fmt (params->guid),
		             (unsigned short)params->ttl, FT_CFG_MAX_TTL);
#endif

		params->ttl = FT_CFG_MAX_TTL;
		ret = TRUE;
	}

	return ret;
}

static BOOL create_result (TCPC *c, FTPacket *packet, int browse,
                           Share *share, ft_nodeinfo_t *parent, ft_nodeinfo_t *owner,
                           unsigned int *retavail)
{
	uint32_t       host;
	uint16_t       port;
	uint16_t       http_port;
	char          *alias;
	uint32_t       avail;
	uint32_t       size;
	unsigned char *md5;
	char          *filename;
	char          *mime;
	char          *meta_key;
	char          *meta_value;

	if (!browse)
	{
		host      = ft_packet_get_ip     (packet);
		port      = ft_packet_get_uint16 (packet, TRUE);
		http_port = ft_packet_get_uint16 (packet, TRUE);
		alias     = ft_packet_get_str    (packet);
	}
	else
	{
		host      = FT_NODE_INFO(c)->host;
		port      = FT_NODE_INFO(c)->port_openft;
		http_port = FT_NODE_INFO(c)->port_http;
		alias     = FT_NODE_INFO(c)->alias;
	}

	avail     = ft_packet_get_uint32 (packet, TRUE);
	size      = ft_packet_get_uint32 (packet, TRUE);
	md5       = ft_packet_get_ustr   (packet, 16);
	mime      = ft_packet_get_str    (packet);
	filename  = ft_packet_get_str    (packet);

	/* if host is 0, assume that these are local shares from that node */
	if (host == 0)
	{
		FT->DBGSOCK (FT, c, "alias '%s', hash %s, parent %s", alias, md5_fmt (md5), net_ip_str (parent->host));
#if 0
		if ((host = FT_NODE_INFO(c)->host) == 0)
#else
		if ((host = parent->host) == 0)
#endif
		{
			FT->DBGSOCK (FT, c, "invalid remote node registration");
			return FALSE;
		}
	}

	if (!filename || !mime || !md5)
	{
		FT->DBGSOCK (FT, c, "invalid search result");
		return FALSE;
	}

	memset (owner, 0, sizeof (ft_nodeinfo_t));
	owner->host        = host;
	owner->port_openft = port;
	owner->port_http   = http_port;
	owner->alias       = alias;        /* careful, direct ptr on the pkt! */
	owner->indirect    = BOOL_EXPR (port == 0 || http_port == 0);

	if (retavail)
		*retavail = avail;

	/* similar to share_new() but will not malloc the object, rather it will
	 * use the storage we provide */
	share_init (share, filename);

	/* setup a couple of manual parameters for the search reply object */
	share->mime = mime;                /* circumventing API for efficiency */
	share->size = size;

	/* we are using another direct ptr on the pkt structure again... */
	if (!(share_set_hash (share, "MD5", md5, 16, TRUE)))
	{
		FT->err (FT, "unable to set hash on search reply object");
		share_finish (share);

		return FALSE;
	}

	/* add the OpenFT-specific data so that ft_search_cmp (from
	 * ft_search_reply) will work */
	share_set_udata (share, "OpenFT", ft_share_new_data (share, NULL, NULL));

	while ((meta_key = ft_packet_get_str (packet)))
	{
		if (!(meta_value = ft_packet_get_str (packet)))
			break;

		share_set_meta (share, meta_key, meta_value);
	}

	return TRUE;
}

static void destroy_result (Share *share, ft_nodeinfo_t *owner)
{
	assert (share != NULL);
	assert (owner != NULL);

	/* remove the OpenFT-specific data we were forced to add for the token
	 * list */
	ft_share_free_data (share, share_get_udata (share, "OpenFT"));
	share_set_udata (share, "OpenFT", NULL);

	/* no allocation on owner, so dont worry about it */
	share_finish (share);
}

ft_search_flags_t get_search_request_type (ft_search_flags_t request)
{
	ft_search_flags_t type;

	/* extract only the method, ignoring any potentially dangerous internal
	 * modifiers */
	type = FT_SEARCH_METHOD(request);

	/* make sure a valid request type was made */
	if (type != FT_SEARCH_FILENAME && type != FT_SEARCH_MD5)
		return 0x00;

	/* add back FT_SEARCH_HIDDEN explicitly */
	if (request & FT_SEARCH_HIDDEN)
		type |= FT_SEARCH_HIDDEN;

	return type;
}

static int fill_params (sparams_t *params, FTNode *node, FTPacket *pkt)
{
	ft_search_flags_t type;

	memset (params, 0, sizeof (sparams_t));

	params->node     = node;
	params->guid     = ft_packet_get_ustr   (pkt, 16);
	params->orighost = ft_packet_get_ip     (pkt);

	/* added port to detect firewalled status of the original node for
	 * the purpose of result filtering */
	if (node->version >= OPENFT_0_2_0_1)
		params->origport = ft_packet_get_uint16 (pkt, TRUE);
	else
		params->origport = 1216;

	params->ttl      = ft_packet_get_uint16 (pkt, TRUE);
	params->nmax     = ft_packet_get_uint16 (pkt, TRUE);
	type             = ft_packet_get_uint16 (pkt, TRUE);
	params->type     = get_search_request_type (type);

	/* the original node that executes this search will record the src as 0,
	 * leaving the innermost nodes (us) to fill in the blank */
	if (params->orighost == 0)
	{
		params->orighost = node->ninfo.host;
		params->origport = node->ninfo.port_openft;
	}

    if (params->type & FT_SEARCH_HIDDEN)
	{
		params->query   = ft_packet_get_arraynul (pkt, 4, TRUE);
		params->exclude = ft_packet_get_arraynul (pkt, 4, TRUE);
	}
	else
	{
		params->query   = ft_packet_get_str (pkt);
		params->exclude = ft_packet_get_str (pkt);
	}

	params->realm = ft_packet_get_str (pkt);

	/* ensure that the search params supplied are sane according to our
	 * local nodes max configuration */
	clamp_params (params);

	/* check for invalid input */
	if (!params->guid)
		return FALSE;

	if (params->type == 0 || params->nmax == 0)
		return FALSE;

	assert (params->orighost != 0);

	return TRUE;
}

static int forward_search (TCPC *c, sparams_t *params, int new_nmax)
{
	int n;
	int peers;

	assert (new_nmax <= params->nmax);

	/* do not forward the search if it's gone too far already */
	if (params->ttl < 1 || new_nmax <= 0)
		return 0;

	/* adjust the new header */
	params->ttl--;
	params->nmax = new_nmax;

	peers = FT_CFG_SEARCH_MAXPEERS;
	n = ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, peers,
	                       FT_NETORG_FOREACH(forward_search_peer), params);

	return n;
}

FT_HANDLER (ft_search_request)
{
	sparams_t params;
	int       results;
	int       peers;

	if (!(auth_search_request (FT_NODE(c))))
		return;

	/* read the params off the packet and fill the params object */
	if (!fill_params (&params, FT_NODE(c), packet))
	{
		FT->DBGSOCK (FT, c, "incomplete search request");
		return;
	}

	/* detect duplicate queries (ones that we've already responded to
	 * with the same guid) and refuse to send the search again, but
	 * gracefully tack on the search sentinel so that the remote node at
	 * least knows we're alive */
	if (active_search (&params))
	{
		empty_result (c, params.guid);
		return;
	}

	/* execute a search on our local node before forwarding begins */
	if ((results = exec_search (c, &params)) < 0)
	{
		FT->DBGSOCK (FT, c, "%s: search error", ft_guid_fmt (params.guid));
		return;
	}

	/* forward the search along to our peers if it still has time-to-live */
	if ((peers = forward_search (c, &params, params.nmax - results)) > 0)
	{
#if 0
		if (FT_SEARCH_TYPE(params.type) == FT_SEARCH_FILENAME)
		{
			FT->DBGSOCK (FT, c, "%s[%i:%i]: %s => %i peers",
			             ft_guid_fmt (params.guid),
			             (int)params.ttl, (int)params.nmax,
			             net_ip_str (params.orighost), peers);
		}
#endif
	}
	else
	{
		/*
		 * Send the search sentinel as we did not actually forward the
		 * results anywhere.  Otherwise, we will determine when to send our
		 * sentinel back as forwarded results come through
		 * ft_search_response.
		 */
		empty_result (c, params.guid);
	}
}

/*****************************************************************************/

static ft_nodeinfo_t *get_parent_info (FTNode *node, FTPacket *pkt)
{
	FTNode   *parent;
	in_addr_t addr;
	in_port_t port;

	/* grab the parent contact information, that is, the address info for
	 * the node that responded to the result we are about to process */
	addr = ft_packet_get_ip     (pkt);
	port = ft_packet_get_uint16 (pkt, TRUE);

	/* if this is the first time the search is being forward back, we
	 * need to adjust the header sent out by the parent to reflect their
	 * actual address before we forward it along */
	if (addr == 0)
	{
		addr = node->ninfo.host;
		port = node->ninfo.port_openft;
	}

	/* permanently register this node information so that we may use an FTNode
	 * handle with the API */
	if (!(parent = ft_node_register (addr)))
		return NULL;

	/* check to make sure our previous information about this node does not
	 * disagree with the forward header */
	if (parent->ninfo.port_openft == 0)
		ft_node_set_port (parent, port);
	else if (parent->ninfo.port_openft != port)
	{
		/* those bastards! */
		FT->DBGSOCK (FT, FT_CONN(node), "port mismatch, %hu (old) vs %hu (new)",
		             (unsigned short)parent->ninfo.port_openft,
		             (unsigned short)port);
	}

	return &parent->ninfo;
}

static void handle_search_result (TCPC *c, FTPacket *pkt, ft_guid_t *guid,
                                  FTSearch *srch)
{
	Share          share;
	ft_nodeinfo_t  owner;
	ft_nodeinfo_t *parent;
	unsigned int   avail;

	/* do not bother handling results that cant be replied to any longer,
	 * and also do not bother with browse searches, which are not valid in
	 * this context */
	if (!srch->event)
		return;

	/* get parent node info */
	if (!(parent = get_parent_info (FT_NODE(c), pkt)))
		return;

	if (!(create_result (c, pkt, FALSE, &share, parent, &owner, &avail)))
		return;

	ft_search_reply (srch, &owner, parent, &share, avail);
	destroy_result (&share, &owner);
}

static void handle_forward_result (TCPC *c, FTPacket *pkt, ft_guid_t *guid,
                                   FTSearchFwd *sfwd)
{
	FTNode        *src;
	FTPacket      *fwd;
	ft_nodeinfo_t *parent;
	in_addr_t      host;
	unsigned char *data;
	size_t         len = 0;

	if (!(parent = get_parent_info (FT_NODE(c), pkt)))
		return;

	if ((host = ft_packet_get_ip (pkt)) == 0)
		host = FT_NODE_INFO(c)->host;

	if (!(src = ft_netorg_lookup (sfwd->src)))
	{
		/* TODO: do something more clever to abort the search */
		FT->DBGSOCK (FT, c, "cant find %s, route lost!");
		return;
	}

	/* create the packet that will be sent back with a slightly modified
	 * search response header */
	if (!(fwd = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return;

	/* add back the part that we have already parsed and possibly
	 * manipulated */
	ft_packet_put_ustr   (fwd, guid, FT_GUID_SIZE);
	ft_packet_put_ip     (fwd, parent->host);
	ft_packet_put_uint16 (fwd, parent->port_openft, TRUE);
	ft_packet_put_ip     (fwd, host);

	/* copy the rest of the message verbatim */
	if ((data = ft_packet_get_raw (pkt, &len)))
		ft_packet_put_raw (fwd, data, len);

	ft_packet_send (FT_CONN(src), fwd);
}

static void handle_search_sentinel (TCPC *c, FTPacket *pkt, ft_guid_t *guid,
                                    FTSearch *srch)
{
	/* file=NULL is a special condition in ft_search_reply that terminates the
	 * search from this parent, and possibly the entire search once there
	 * are no parents left */
	ft_search_reply (srch, NULL, FT_NODE_INFO(c), NULL, 0);
}

static void handle_forward_sentinel (TCPC *c, FTPacket *pkt, ft_guid_t *guid,
                                     FTSearchFwd *sfwd)
{
	unsigned int n;
	FTNode      *srcnode;
	in_addr_t    src;

	/* preserve the source address in case we need it to deliver our
	 * sentinel back to the original node that requested the search */
	src = sfwd->src;

	/* terminate this particular forward connection */
	n = ft_search_fwd_finish (sfwd);

	/* when the number of active forward sessions for this guid reaches 0,
	 * we know that we have effectively returned all results possible */
	if (n == 0)
	{
		/* lookup the node we will reply back to */
		if (!(srcnode = ft_netorg_lookup (src)) || !FT_CONN(srcnode))
		{
			FT->DBGSOCK (FT, c, "cant locate %s, *sigh*", net_ip_str (src));
			return;
		}

		/* send the sentinel back indicating that all forwarded results have
		 * been delivered, and the search is effectively being terminated
		 * here */
		empty_result (FT_CONN(srcnode), guid);
	}
}

FT_HANDLER (ft_search_response)
{
	FTSearch     *srch = NULL;
	FTSearchFwd  *sfwd = NULL;
	ft_guid_t    *guid;
	ResultHandler handle_fn;

	if (!(auth_search_response (FT_NODE(c))))
		return;

	if (!(guid = ft_packet_get_ustr (packet, FT_GUID_SIZE)))
	{
		FT->DBGSOCK (FT, c, "bogus search result, no guid");
		return;
	}

	/* locate the appropriate search object that will help us proceed */
	if (!(srch = ft_search_find (guid)))
	{
		if (!(sfwd = ft_search_fwd_find (guid, FT_NODE_INFO(c)->host)))
		{
#if 0
			FT->DBGSOCK (FT, c, "%s: %i: search result ignored",
			             ft_guid_fmt (guid),
			             (int)(ft_packet_length (packet)));
#endif
			return;
		}
	}

	handle_fn = NULL;

	/* determine whether this response is a sentinel (end-of-search) or a full
	 * result that must be either returned to the interface or forwarded
	 * along the network */
	if (ft_packet_length (packet) > FT_GUID_SIZE)
	{
		if (srch)
			handle_fn = (ResultHandler)handle_search_result;
		else
			handle_fn = (ResultHandler)handle_forward_result;
	}
	else
	{
		if (srch)
			handle_fn = (ResultHandler)handle_search_sentinel;
		else
			handle_fn = (ResultHandler)handle_forward_sentinel;
	}

	/* oh this is absolutely awful... */
	assert (handle_fn != NULL);

	if (srch)
		handle_fn (c, packet, guid, srch);
	else
		handle_fn (c, packet, guid, sfwd);
}

/*****************************************************************************/

static int send_browse (ds_data_t *key, ds_data_t *value, sreply_t *reply)
{
	sparams_t *params = reply->params;
	FTPacket  *pkt;
	Share     *file;
	Hash      *hash;
	char      *hpath;

	if (!(file = value->data))
		return DS_CONTINUE;

	hpath = share_get_hpath (file);
	assert (hpath != NULL);

	if (!(pkt = ft_packet_new (FT_BROWSE_RESPONSE, 0)))
		return DS_CONTINUE;

	if (!(hash = share_get_hash (file, "MD5")))
		return DS_CONTINUE;

	ft_packet_put_ustr   (pkt, params->guid, FT_GUID_SIZE);
	ft_packet_put_uint32 (pkt, (uint32_t)ft_upload_avail(), TRUE);
	ft_packet_put_uint32 (pkt, (uint32_t)file->size, TRUE);
	ft_packet_put_ustr   (pkt, hash->data, hash->len);
	ft_packet_put_str    (pkt, file->mime);
	ft_packet_put_str    (pkt, hpath);

	share_foreach_meta (file, DS_FOREACH(result_add_meta), pkt);

	assert (reply->stream != NULL);
	sreply_send (reply, pkt);

	return DS_CONTINUE;
}

FT_HANDLER (ft_browse_request)
{
	sreply_t   reply;
	sparams_t  params;
	FTPacket  *pkt;
	ft_guid_t *guid;

	if (!(guid = ft_packet_get_ustr (packet, FT_GUID_SIZE)))
		return;

	/* hack the params object to include only the fields we know we will
	 * be using */
	params.guid = guid;

	/* initialize the reply object */
	sreply_init (&reply, c, ft_stream_get (c, FT_STREAM_SEND, NULL), &params);

	/* send shares */
	share_foreach (DS_FOREACH_EX(send_browse), &reply);
	sreply_finish (&reply);

	/* terminate */
	if (!(pkt = ft_packet_new (FT_BROWSE_RESPONSE, 0)))
		return;

	ft_packet_put_ustr (pkt, guid, FT_GUID_SIZE);
	ft_packet_send (c, pkt);
}

FT_HANDLER (ft_browse_response)
{
	FTBrowse     *browse;
	Share         share;
	ft_nodeinfo_t owner;
	unsigned int  avail;
	ft_guid_t    *guid;

	if (!(guid = ft_packet_get_ustr (packet, FT_GUID_SIZE)))
		return;

	if (!(browse = ft_browse_find (guid, FT_NODE_INFO(c)->host)) ||
	    !browse->event)
		return;

	if (ft_packet_length (packet) > FT_GUID_SIZE)
	{
		if (!(create_result (c, packet, TRUE, &share, NULL, &owner, &avail)))
			return;

		ft_browse_reply (browse, &owner, &share, avail);
		destroy_result (&share, &owner);
	}
}
