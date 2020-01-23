/*
 * $Id: gt_xfer.c,v 1.103 2005/01/05 14:08:40 mkern Exp $
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

#include "gt_xfer_obj.h"
#include "gt_xfer.h"
#include "gt_http_client.h"
#include "gt_http_server.h"
#include "gt_share.h"
#include "gt_share_file.h"
#include "gt_packet.h"
#include "gt_node.h"
#include "gt_node_list.h"
#include "gt_netorg.h"
#include "gt_connect.h"
#include "gt_bind.h"
#include "gt_utils.h"

#include "encoding/url.h"

#include "transfer/source.h"

/******************************************************************************/

/* maximum number of push connections in limbo each remote user */
#define PUSH_MAX_LIMBO        gt_config_get_int("transfer/push_max_in_limbo=5")

/******************************************************************************/

/* an alternative to carrying the push TTL around in the source url */
#define PUSH_MAX_TTL              12

/* maximum time to keep push connections in "limbo" while awaiting giftd
 * to reissue Chunks */
#define PUSH_LIMBO_TIMEOUT        (4 * MINUTES)

/* how long to wait for a PUSH reply before timing out in order to free the
 * Chunk */
#define PUSH_WAIT_INTERVAL        (30 * SECONDS)

/* minimum interval between pushes */
#define PUSH_MIN_DEFER_TIME       (30 * ESECONDS)

/* maximum amount of time a push will be forced to wait before being sent */
#define PUSH_MAX_DEFER_TIME       (10 * EMINUTES)

/*****************************************************************************/

/* this stores information about an indirect ("pushed") download */
typedef struct gt_push_source
{
	gt_guid_t  *guid;
	in_addr_t   ip;
	in_addr_t   src_ip;      /* whether this push was to a local source */
	List       *xfers;       /* xfers for this source */
	List       *connections; /* connection for this source */
	time_t      last_sent;   /* time of last push sent to this source */
	double      defer_time;  /* min time to wait before sending another push
	                          * doubles every push to PUSH_MAX_DEFER_TIME */
} GtPushSource;

/*****************************************************************************/

/* Maps guid->{list of unique GtPushSources} */
static Dataset *gt_push_requests;

/******************************************************************************/

static void push_source_reset_last_sent (GtPushSource *push_src);
static void push_source_set_last_sent   (gt_guid_t *guid, in_addr_t ip);
static BOOL push_source_should_send     (gt_guid_t *guid, in_addr_t ip);

/*****************************************************************************/

/*
 * The source URL is stored on disk and could be outdated if the format has
 * changed. This updates it with any changes when we first read it from
 * the state file on startup.
 */
static void replace_url (Source *src, GtSource *gt)
{
	char *url;

	if (!(url = gt_source_serialize (gt)))
		return;

	/* swap urls */
	FREE (src->url);
	src->url = url;
}

/******************************************************************************/

static FileShare *lookup_index (GtTransfer *xfer, char *request)
{
	FileShare *file;
	char      *index_str;
	char      *filename;
	char      *decoded;
	uint32_t   index;

	filename  = request;
	index_str = string_sep (&filename, "/");

	if (!filename || !index_str)
		return NULL;

	index = ATOUL (index_str);

	decoded = gt_url_decode (filename);
	file = gt_share_local_lookup_by_index (index, decoded);

	free (decoded);

	/* the filename may or may not be url encoded */
	if (!file)
		file = gt_share_local_lookup_by_index (index, filename);

	return file;
}

static FileShare *lookup_urns (GtTransfer *xfer, char *urns)
{
	FileShare *file = NULL;
	char      *urn;

	/*
	 * Try to lookup all urns provided in the header,
	 * until one is found.
	 */
	while (!file && !string_isempty (urns))
	{
		urn = string_sep_set (&urns, ", ");
		file = gt_share_local_lookup_by_urn (urn);
	}

	return file;
}

static FileShare *lookup_uri_res (GtTransfer *xfer, char *request)
{
	FileShare  *file     = NULL;
	char       *resolver = NULL;
	char       *urn;

	resolver = string_sep (&request, "?");
	urn      = string_sep (&request, " ");

	if (resolver && !strcasecmp (resolver, "N2R"))
	{
		string_trim (request);
		file = lookup_urns (xfer, urn);
	}

	if (file && HTTP_DEBUG)
		GT->dbg (GT, "file=%s", share_get_hpath (file));

	return file;
}

static Share *lookup_hpath (char *namespace, GtTransfer *xfer, char *request)
{
	char   *hpath;
	Share  *share;

	/*
	 * Reconstruct the hpath
	 */
	if (!(hpath = stringf_dup ("/%s/%s", namespace, request)))
		return NULL;

	if (HTTP_DEBUG)
		GT->dbg (GT, "request by hpath: %s", hpath);

	share = GT->share_lookup (GT, SHARE_LOOKUP_HPATH, hpath);
	free (hpath);

	return share;
}

/* Take a request for a file, i.e. everything after GET in:
 *
 * "GET /get/1279/filename.mp3"
 *
 * and convert it to a localized path to a file.
 *
 * Path has been "secured" already if necessary.
 *
 * The path returned must be static.
 * TODO: this interface is a bit bizarre */
char *gt_localize_request (GtTransfer *xfer, char *s_path, int *authorized)
{
	static char open_path[PATH_MAX];
	char       *namespace;
	char       *path, *path0;
	char       *content_urns;
	FileShare  *file;

	/* TODO: use authorized for Browse Host (BH) requests */
	if (!STRCMP (s_path, "/"))
	{
		/* *authorized = TRUE; */
		if (HTTP_DEBUG)
			GT->DBGFN (GT, "received unimplemented Browse Host request");

		return NULL;
	}

	if (authorized)
		*authorized = FALSE;

	if (!(path0 = path = STRDUP (s_path)))
		return NULL;

	if (HTTP_DEBUG)
		GT->dbg (GT, "path=%s", path);

	/* get rid of leading slash */
	string_sep (&path, "/");
	namespace = string_sep (&path, "/");

	if (!namespace || !path)
	{
		GT->DBGFN (GT, "null namespace or path: %s %s\n", namespace, path);
		free (path0);
		return NULL;
	}

	/*
	 * If the client supplied "X-Gnutella-Content-URN: ", lookup
	 * by that instead of the request.
	 */
	content_urns = dataset_lookupstr (xfer->header, "x-gnutella-content-urn");

	if (content_urns)
		file = lookup_urns (xfer, content_urns);
	else if (!strcasecmp (namespace, "get"))
		file = lookup_index (xfer, path);
	else if (!strcasecmp (namespace, "uri-res"))
		file = lookup_uri_res (xfer, path);
	else
		file = lookup_hpath (namespace, xfer, path);

	/*
	 * Set xfer->content_urn [which replies with 'X-Gnutella-Content-URN']
	 * to a comma-separated list of all URNs for this file.
	 */
	xfer->content_urns = gt_share_local_get_urns (file);

	if (!file)
	{
		if (HTTP_DEBUG)
			GT->DBGFN (GT, "bad request: /%s/%s", namespace, path);

		free (path0);
		return NULL;
	}

	free (path0);

	if (!share_complete (file))
		return NULL;

	/* argh, need to return static data */
	snprintf (open_path, sizeof (open_path) - 1, "%s", share_get_hpath (file));

	/* try to fill in the hash */
	xfer->hash = share_dsp_hash (file, "SHA1");

	return open_path;
}

/******************************************************************************/

static char *index_request (char *request, size_t size,
                            uint32_t index, const char *filename)
{
	/*
	 * The filename may not have ever been set.  Fail the request in
	 * this case.
	 */
	if (!filename || string_isempty (filename))
		return NULL;

	/*
	 * Filename is encoded, we don't support sending unecoded requests
	 * anymore.  NOTE: filename doesnt have leading '/' here, that may change
	 */
	snprintf (request, size - 1, "/get/%u/%s", index, filename);
	return request;
}

/*
 * Setup a request string. Try request-by-hash (/uri-res/N2R?urn:sha1:..),
 * but if there are any problems, fallback to a "/get/<index>/<filename>"
 * request.
 */
static char *request_str (Source *source, uint32_t index, char *filename)
{
	static char request[RW_BUFFER];
	char       *hash = source->hash;
	GtSource   *gt;

	gt = source->udata;
	assert (gt != NULL);

	/*
	 * Do a uri-res request unless one has failed already or
	 * if we have no filename and thus no choice but to use the hash.
	 * (doesn't happen currently but will for download-mesh sources
	 * that only have the hash and no filename)
	 */
	if (hash && (!gt->uri_res_failed || string_isempty (filename)))
	{
		char *str0, *str;

		if (!(str0 = STRDUP (hash)))
			return index_request (request, sizeof (request), index, filename);

		str = str0;
		string_sep (&str, ":");

		/* hashes are canonically uppercase on the gnet */
		string_upper (str);

		if (str)
		{
			snprintf (request, sizeof (request) - 1,
			          "/uri-res/N2R?urn:sha1:%s", str);
			free (str0);
			return request;
		}

		free (str0);
	}

	return index_request (request, sizeof (request), index, filename);
}

/*****************************************************************************/
/* PUSH HANDLING */

/*
 * This code has to deal with some tricky race conditions involving
 * chunk timeouts and pushes.
 */

static GtPushSource *gt_push_source_new (gt_guid_t *guid, in_addr_t ip,
                                         in_addr_t src_ip)
{
	GtPushSource *src;

	if (!(src = MALLOC (sizeof (GtPushSource))))
		return NULL;

	src->guid        = gt_guid_dup (guid);
	src->ip          = ip;
	src->src_ip      = src_ip;
	src->xfers       = NULL;
	src->connections = NULL;

	push_source_reset_last_sent (src);

	return src;
}

static void gt_push_source_free (GtPushSource *src)
{
	if (!src)
		return;

	assert (src->xfers == NULL);
	assert (src->connections == NULL);

	free (src->guid);
	free (src);
}

/*****************************************************************************/

/* TODO: break this into two parts, first part looks in the
 *       list for matching ip. If none is found, look for
 *       a firewalled (local ip) push source.  */
static int find_ip (GtPushSource *src, in_addr_t *ip)
{
	/* If the source is a local IP address behind a non-local one,
	 * authorize by just the client guid. Otherwise, use the IP. */
	if (gt_is_local_ip (src->ip, src->src_ip) || src->ip == *ip)
		return 0;

	return -1;
}

static List *lookup_source_list (gt_guid_t *guid)
{
	List   *src_list;

	if (!(src_list = dataset_lookup (gt_push_requests, guid, 16)))
		return NULL;

	return src_list;
}

static GtPushSource *push_source_lookup (gt_guid_t *guid, in_addr_t ip)
{
	List   *requests;
	List   *list;

	if (!(requests = lookup_source_list (guid)))
		return NULL;

	list = list_find_custom (requests, &ip, (ListForeachFunc)find_ip);
	return list_nth_data (list, 0);
}

/*****************************************************************************/

static void insert_source_list (gt_guid_t *guid, List *src_list)
{
	if (!gt_push_requests)
		gt_push_requests = dataset_new (DATASET_HASH);

	dataset_insert (&gt_push_requests, guid, 16, src_list, 0);
}

static void add_push_source (List *pushes, gt_guid_t *guid, in_addr_t ip,
                             in_addr_t src_ip)
{
	GtPushSource *src;
	List         *old_list;

	if (!(src = gt_push_source_new (guid, ip, src_ip)))
		return;

	if ((old_list = list_find_custom (pushes, &ip, (ListForeachFunc)find_ip)))
	{
		/* push source is already there */
		gt_push_source_free (src);
		return;
	}

	pushes = list_prepend (pushes, src);
	insert_source_list (guid, pushes);
}

void gt_push_source_add (gt_guid_t *guid, in_addr_t ip, in_addr_t src_ip)
{
	List *pushes;

	pushes = lookup_source_list (guid);
	add_push_source (pushes, guid, ip, src_ip);
}

/*****************************************************************************/
/* Timing controls for push requests */

static void push_source_reset_last_sent (GtPushSource *push_src)
{
	push_src->last_sent  = gt_uptime ();  /* wrong */
	push_src->defer_time = 0.0;
}

static void push_source_set_last_sent (gt_guid_t *guid, in_addr_t ip)
{
	GtPushSource *src;

	if (!(src = push_source_lookup (guid, ip)))
		return;

	time (&src->last_sent);
}

static BOOL push_source_should_send (gt_guid_t *guid, in_addr_t ip)
{
	GtPushSource *src;
	double        deferred;
	time_t        now;

	time (&now);

	if (!(src = push_source_lookup (guid, ip)))
		return FALSE;

	deferred = difftime (now, src->last_sent);

	/* randomize the defer time a bit in order to not send pushes for all
	 * downloads at once */
	if (deferred < src->defer_time + -10.0 + 20.0 * rand() / (RAND_MAX + 1.0))
		return FALSE;

	/*
	 * Double the defer time interval (the minimum time between sent
	 * pushes for this source).
	 */
	src->defer_time *= 2;
	if (src->defer_time >= PUSH_MAX_DEFER_TIME)
		src->defer_time = PUSH_MAX_DEFER_TIME;

	if (src->defer_time == 0)
		src->defer_time = PUSH_MIN_DEFER_TIME;

	return TRUE;
}

/*****************************************************************************/

static void flush_inputs (TCPC *c)
{
	int ret;

	assert (c->fd >= 0);

	/* queued writes arent used by the HTTP system in this plugin,
	 * so this should always be true */
	ret = tcp_flush (c, TRUE);
	assert (ret == 0);

	input_remove_all (c->fd);
}

static void continue_download (GtPushSource *push_src, GtTransfer *xfer,
                               TCPC *c)
{
	Chunk *chunk;

	chunk = gt_transfer_get_chunk (xfer);

	/* remove all previous inputs */
	flush_inputs (c);

	/* HACK: remove the detach timeout */
	timer_remove_zero (&xfer->detach_timer);

	/* connect the TCPC and GtTransfer */
	gt_transfer_set_tcpc (xfer, c);

	/* update the IP and port for placing in Host: */
	peer_addr (c->fd, &xfer->ip, &xfer->port);

	/* the connection and the chunk have met up */
	gt_transfer_status (xfer, SOURCE_WAITING, "Received GIV response");

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "Continuing download for %s", xfer->request);

	input_add (c->fd, xfer, INPUT_WRITE,
	           (InputCallback)gt_http_client_start, TIMEOUT_DEF);
}

static void reset_conn (int fd, input_id id, TCPC *c)
{
	/*
	 * We should only get here if some data was sent, or if it timed out.  In
	 * which case we should close this connection, because it shouldn't be
	 * sending anything.
	 */
	if (HTTP_DEBUG)
	{
		if (fd == -1)
			GT->DBGSOCK (GT, c, "connection timed out");
		else
			GT->DBGSOCK (GT, c, "connection closed or sent invalid data");
	}

	gt_push_source_remove_conn (c);
	tcp_close (c);
}

static void store_conn (GtPushSource *src, TCPC *c)
{
	flush_inputs (c);

	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)reset_conn, PUSH_LIMBO_TIMEOUT);

	assert (!list_find (src->connections, c));
	src->connections = list_prepend (src->connections, c);

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "storing connection");
}

BOOL gt_push_source_add_conn (gt_guid_t *guid, in_addr_t ip, TCPC *c)
{
	GtTransfer    *xfer;
	GtPushSource  *push_src;

	if (!(push_src = push_source_lookup (guid, ip)))
	{
		if (HTTP_DEBUG)
		{
			GT->err (GT, "couldn't find push source %s:[%s]",
			         gt_guid_str (guid), net_ip_str (ip));
		}

		tcp_close (c);
		return FALSE;
	}

	/*
	 * Don't allow too many connections in flight from the same remote user.
	 */
	if (list_length (push_src->connections) >= PUSH_MAX_LIMBO)
	{
		if (HTTP_DEBUG)
		{
			GT->DBGSOCK (GT, c, "too many push connections from %s, closing",
			             gt_guid_str (guid));
		}

		tcp_close (c);
		return FALSE;
	}

	/*
	 * Since we now know this push source is alive, reset the push send
	 * tracking time: in case the connection is lost, we'll resend the push
	 * right away instead of waiting.
	 */
	push_source_reset_last_sent (push_src);

	/*
	 * Store the connection if there are no GtTransfer requests from
	 * giFT at the moment.
	 */
	if (!push_src->xfers)
	{
		store_conn (push_src, c);
		return FALSE;
	}

	xfer = list_nth_data (push_src->xfers, 0);
	push_src->xfers = list_remove (push_src->xfers, xfer);

	continue_download (push_src, xfer, c);
	return TRUE;
}

/* return TRUE if there's a connection residing on this push source */
static BOOL push_source_lookup_conn (gt_guid_t *guid, in_addr_t ip)
{
	GtPushSource *push_src;

	if (!(push_src = push_source_lookup (guid, ip)))
		return FALSE;

	if (push_src->connections)
	{
		if (HTTP_DEBUG)
			GT->DBGFN (GT, "found push connection for %s", net_ip_str (ip));

		return TRUE;
	}

	return FALSE;
}

static void store_xfer (GtPushSource *src, GtTransfer *xfer)
{
	assert (!list_find (src->xfers, xfer));
	src->xfers = list_prepend (src->xfers, xfer);
}

BOOL gt_push_source_add_xfer (gt_guid_t *guid, in_addr_t ip,
                              in_addr_t src_ip, GtTransfer *xfer)
{
	TCPC           *c;
	GtPushSource   *push_src;

	assert (xfer != NULL);

	/* create the source if it doesn't exist already */
	gt_push_source_add (guid, ip, src_ip);

	if (!(push_src = push_source_lookup (guid, ip)))
	{
		if (HTTP_DEBUG)
		{
			GT->err (GT, "couldn't find push source (%s:[%s]) for chunk %s",
			         gt_guid_str (guid), net_ip_str (ip), xfer->request);
		}

		return FALSE;
	}

	/*
	 * Store the GtTransfer if there are no connections to service it
	 * at the moment.
	 */
	if (!push_src->connections)
	{
		store_xfer (push_src, xfer);
		return FALSE;
	}

	c = list_nth_data (push_src->connections, 0);
	push_src->connections = list_remove (push_src->connections, c);

	continue_download (push_src, xfer, c);
	return TRUE;
}

/*****************************************************************************/

static BOOL remove_xfer (GtPushSource *src, GtTransfer *xfer)
{
	src->xfers = list_remove (src->xfers, xfer);
	return FALSE;
}

static void remove_xfer_list (ds_data_t *key, ds_data_t *value,
                              GtTransfer *xfer)
{
	List *src_list = value->data;

	list_foreach (src_list, (ListForeachFunc)remove_xfer, xfer);
}

/*
 * The chunk is being cancelled, so remove it from being tracked.
 *
 * After this, if the push recipient connects, we will have to wait
 * for another chunk timeout before transmitting.
 */
void gt_push_source_remove_xfer (GtTransfer *xfer)
{
	if (!xfer)
		return;

	dataset_foreach (gt_push_requests, DS_FOREACH(remove_xfer_list), xfer);
}

static BOOL remove_conn (GtPushSource *src, TCPC *c)
{
	src->connections = list_remove (src->connections, c);
	return FALSE;
}

static void remove_conn_list (ds_data_t *key, ds_data_t *value, TCPC *c)
{
	List *src_list = value->data;

	list_foreach (src_list, (ListForeachFunc)remove_conn, c);
}

/*
 * The connection from this push download closed
 */
void gt_push_source_remove_conn (TCPC *c)
{
	if (!c)
		return;

	dataset_foreach (gt_push_requests, DS_FOREACH(remove_conn_list), c);
}

static BOOL cleanup_xfer (GtTransfer *xfer, void *udata)
{
	gt_push_source_remove_xfer (xfer);
	return TRUE;
}

static BOOL cleanup_conn (TCPC *c, void *udata)
{
	gt_push_source_remove_conn (c);
	tcp_close (c);
	return TRUE;
}

static void remove_push_source (GtPushSource *src)
{
	List *src_list;

	src_list = lookup_source_list (src->guid);
	src_list = list_remove (src_list, src);

	insert_source_list (src->guid, src_list);
}

void gt_push_source_remove (gt_guid_t *guid, in_addr_t ip, in_addr_t src_ip)
{
	GtPushSource *src;

	if (!(src = push_source_lookup (guid, ip)))
		return;

	/* cleanup all the chunks and connections */
	src->xfers =
	    list_foreach_remove (src->xfers, (ListForeachFunc)cleanup_xfer,
	                         NULL);
	src->connections =
	    list_foreach_remove (src->connections, (ListForeachFunc)cleanup_conn,
	                         NULL);

	/* remove this source from the global list */
	remove_push_source (src);

	gt_push_source_free (src);
}

/*****************************************************************************/

static BOOL detach_timeout (void *udata)
{
	GtTransfer *xfer = udata;

	/* Added this on 2004-12-22 to track observed assertion failure in
	 * gt_transfer_get_chunk. -- mkern
	 */
	if (!xfer->chunk || xfer->chunk->udata != xfer)
	{
		GT->DBGFN (GT, "Detach timeout troubles. status = %d, "
		           "text = %s, xfer->ip = %s, "
		           "xfer = %p, xfer->chunk->udata = %p, "
		           "xfer->detach_timer = 0x%X",
		           xfer->detach_status, xfer->detach_msgtxt,
		           net_ip_str (xfer->ip), xfer,
		           xfer->chunk->udata, xfer->detach_timer);
	}

	/* Sometimes gt_transfer_status will trigger an
	 * assert (xfer->chunk->udata == xfer) failure in gt_transfer_get_chunk.
	 * But why? Is xfer already freed? Does it have another chunk and the
	 * timer was not removed?
	 */
	gt_transfer_status (xfer, xfer->detach_status, xfer->detach_msgtxt);
	gt_transfer_close (xfer, TRUE);

	return FALSE;
}

/*
 * Attach a timer that will "detach" the GtTransfer from the Chunk by
 * cancelling it, but pretend that the Source is in some other state besides
 * "cancelled" or "timed out" by changing the status text.
 *
 * This is useful to keep a semi-consistent UI in certain situations, such as
 * sending out push requests, and cancelling requests when the remote side has
 * queued our request.
 */
static void detach_transfer_in (GtTransfer *xfer, SourceStatus status,
                                char *status_txt, time_t interval)
{
	char *msg;

	msg = STRDUP (status_txt);

	gt_transfer_status (xfer, status, msg);
	xfer->detach_status = status;

	free (xfer->detach_msgtxt);
	xfer->detach_msgtxt = msg;

	xfer->detach_timer = timer_add (interval,
	                                (TimerCallback)detach_timeout, xfer);
}

/*
 * Detach an GtTransfer from its Chunk.
 */
static void detach_transfer (GtTransfer *xfer, SourceStatus status,
                             char *msgtxt)
{
	/*
	 * Cancelling from p->download_start will cause download_pause() to crash.
	 * So, the detach must happen from timer context.
	 */
	detach_transfer_in (xfer, status, msgtxt, 2 * SECONDS);
}

/*****************************************************************************/

static void send_push (GtTransfer *xfer, GtSource *gt, TCPC *server)
{
	GtPacket *packet;

	if (!(packet = gt_packet_new (GT_MSG_PUSH, PUSH_MAX_TTL, NULL)))
		return;

	gt_packet_put_ustr   (packet, gt->guid, 16);
	gt_packet_put_uint32 (packet, gt->index);
	gt_packet_put_ip     (packet, GT_NODE(server)->my_ip);
	gt_packet_put_port   (packet, GT_SELF->gt_port);

	if (gt_packet_error (packet))
	{
		gt_packet_free (packet);
		return;
	}

	gt_packet_send (server, packet);
	gt_packet_free (packet);

	/*
	 * Don't wait for the whole Chunk timeout -- that keeps the Chunk
	 * occupied for too long if there are other active sources (the Chunk
	 * also times out longer and longer each time, so this gets worse
	 * the longer the transfer is inactive).
	 *
	 * This is really an infelicity of the Chunk system.
	 */
	detach_transfer_in (xfer, SOURCE_QUEUED_REMOTE, "Sent PUSH request",
	                    PUSH_WAIT_INTERVAL);

	/* set the last time we sent a push to now */
	push_source_set_last_sent (gt->guid, gt->user_ip);
}

static BOOL send_push_to_server (in_addr_t server_ip, in_port_t server_port,
                                 GtTransfer *xfer, GtSource *gt)
{
	GtNode *server;

	if (!(server = gt_node_lookup (server_ip, server_port)))
	{
		server = gt_node_register (server_ip, server_port,
		                           GT_NODE_ULTRA);
	}

	if (!server)
	{
		GT->err (GT, "couldn't register server");
		return FALSE;
	}

	if (server->state & (GT_NODE_CONNECTED | GT_NODE_CONNECTING_2))
	{
		assert (GT_CONN(server) != NULL);

		/* Server is in a state for receiving packets -- send the push */
		send_push (xfer, gt, GT_CONN(server));
		return TRUE;
	}
	else if (server->state & GT_NODE_CONNECTING_1)
	{
		/* dont try to connect again; wait till we're connected */
		return FALSE;
	}
	else if (gt_conn_need_connections (GT_NODE_ULTRA) > 0 &&
	         !server->tried_connect &&
	         gt_connect (server) >= 0)
	{
		/*
		 * We've tried to connect to the server so we could deliver the push
		 * request eventually NOTE: this doesnt send a push until the next
		 * chunk timeout.
		 */
		return FALSE;
	}

	return FALSE;
}

static void handle_push_download (Chunk *chunk, GtTransfer *xfer, GtSource *gt)
{
	GtNode *server;

	/*
	 * If this succeeds, we already have a connection to this
	 * user and the transfer will continue by using that connection.
	 *
	 * TODO: the gt_push_source_add() should be used by some
	 *       per-source data structure
	 */
	if (gt_push_source_add_xfer (gt->guid, gt->user_ip, gt->server_ip, xfer))
		return;

	/*
	 * Dont send pushes too often. Maybe should use a global queue instead.
	 *
	 * NOTE: we can't free the xfer here because we have stored it.
	 */
	if (push_source_should_send (gt->guid, gt->user_ip) == FALSE)
	{
		/* don't occupy the Chunk forever */
		detach_transfer_in (xfer, SOURCE_QUEUED_REMOTE, "Awaiting connection",
		                    PUSH_WAIT_INTERVAL);
		return;
	}

	/*
	 * Next, try to find the server that supplied this result,
	 * and send them a push.
	 */
	if (send_push_to_server (gt->server_ip, gt->server_port, xfer, gt))
		return;

	/*
	 * Finally, try sending to a random connected server.
	 *
	 * TODO: these should be rate-limited, either globally or
	 *       per-source.
	 */
	if ((server = gt_conn_random (GT_NODE_ULTRA, GT_NODE_CONNECTED)))
	{
		send_push_to_server (server->ip, server->gt_port, xfer, gt);
		return;
	}

	detach_transfer (xfer, SOURCE_QUEUED_REMOTE, "No PUSH route");
}

static BOOL set_request (GtTransfer *xfer, Chunk *chunk, Source *source,
                         GtSource *gt_src)
{
	char  *request;

	if (!chunk || !xfer)
		return FALSE;

	request = request_str (source, gt_src->index, gt_src->filename);

	if (!gt_transfer_set_request (xfer, request))
	{
		GT->DBGFN (GT, "UI made an invalid request for '%s'", request);
		return FALSE;
	}

	/* connect the xfer and the chunk */
	gt_transfer_set_chunk (xfer, chunk);

	return TRUE;
}

static BOOL should_push (GtSource *gt)
{
	TCPC *persistent;

	/* we cannot push if there is no guid to send the push to */
	if (gt_guid_is_empty (gt->guid))
		return FALSE;

	persistent = gt_http_connection_lookup (GT_TRANSFER_DOWNLOAD,
	                                        gt->user_ip,
	                                        gt->user_port);

	/* need to close the connection to re-add it to the list, because
	 * _lookup removes it from the persistent connection list */
	gt_http_connection_close (GT_TRANSFER_DOWNLOAD, persistent, FALSE);

	/* if we already have a connection don't send a push */
	if (persistent)
		return FALSE;

	/* now check for a persistent "pushed" connection, which would be stored
	 * separately from a directly connected one */
	if (push_source_lookup_conn (gt->guid, gt->user_ip))
		return TRUE;

	/* send a push if the IP is local */
	if (gt_is_local_ip (gt->user_ip, gt->server_ip))
		return TRUE;

	/* don't send a push if we cannot receive incoming connections */
	if (gt_bind_is_firewalled())
		return FALSE;

	/* send a push if they set the firewalled bit */
	if (gt->firewalled)
		return TRUE;

	/* the last connection attempt failed, so try a push */
	if (gt->connect_failed)
		return TRUE;

	return FALSE;
}

static void handle_download (Chunk *chunk, GtTransfer *xfer, GtSource *gt)
{
	/*
	 * Send a push, or connect directly.
	 */
	if (should_push (gt))
	{
		/* (possibly) retry a connection attempt next time */
		gt->connect_failed = FALSE;

		handle_push_download (chunk, xfer, gt);
	}
	else
	{
		gt_http_client_get (chunk, xfer);
	}
}

static BOOL download_is_queued (GtSource *gt)
{
	/* back out if the request is still too early */
	if (time (NULL) < gt->retry_time)
		return TRUE;

	return FALSE;
}

int gnutella_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                             Source *source)
{
	GtTransfer    *xfer;
	GtSource      *gt;
	off_t          start;
	off_t          stop;

	gt = source->udata;
	assert (gt != NULL);

	/* giftd should send us only deactivated Chunks */
	assert (chunk->udata == NULL);

	/* free the Source URL and update it with any format changes */
	replace_url (source, gt);

	/* thank you, pretender :) */
	start = chunk->start + chunk->transmit;
	stop  = chunk->stop;

	if (!(xfer = gt_transfer_new (GT_TRANSFER_DOWNLOAD, source,
	                              gt->user_ip, gt->user_port, start, stop)))
	{
		GT->DBGFN (GT, "gt_transfer_new failed");
		return FALSE;
	}

	if (!set_request (xfer, chunk, source, gt))
	{
		gt_transfer_close (xfer, TRUE);
		return FALSE;
	}

	if (download_is_queued (gt))
	{
		detach_transfer (xfer, SOURCE_QUEUED_REMOTE, gt->status_txt);
		return TRUE;
	}

	handle_download (chunk, xfer, gt);

	return TRUE;
}

void gnutella_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                             Source *source, BOOL complete)
{
	gt_download_cancel (chunk, NULL);
}

int gnutella_upload_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source, unsigned long avail)
{
	return TRUE;
}

void gnutella_upload_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source)
{
	gt_upload_cancel (chunk, NULL);
}

int gnutella_chunk_suspend (Protocol *p, Transfer *transfer, Chunk *chunk,
                            Source *source)
{
	return gt_chunk_suspend (chunk, transfer, NULL);
}

int gnutella_chunk_resume (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source)
{
	return gt_chunk_resume (chunk, transfer, NULL);
}
