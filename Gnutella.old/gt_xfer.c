/*
 * $Id: gt_xfer.c,v 1.54 2003/07/13 07:41:22 hipnod Exp $
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

#include "gt_xfer.h"

#include "gt_http_client.h"
#include "gt_http_server.h"

#include "html.h"

#include "gt_share.h"
#include "gt_share_file.h"

#include "gt_protocol.h"
#include "gt_packet.h"

#include "gt_node.h"
#include "gt_netorg.h"
#include "gt_connect.h"

#include "gt_xfer.h"

/******************************************************************************/

/* An alternative to carrying the push TTL around in the source url */
#define PUSH_MAX_TTL            12

/* timeout push requests after this interval */
#define INDIRECT_TIMEOUT        (4 * MINUTES)

#define PUSH_SEND_INTERVAL      (3 * EMINUTES)

/******************************************************************************/

/* this stores information about an indirect ("pushed") download */
typedef struct gt_push_download
{
	gt_guid_t  *guid;
	in_addr_t   ip;
	in_addr_t   src_ip;       /* whether this push was to a local source */
	time_t      last_sent;    /* time of last push sent to this source */
	List       *xfers;        /* xfers for this source */
	List       *connections;  /* connection for this source */
} GtPushSource;

/* Maps guid->{list of unique GtPushSources} */
static Dataset *gt_push_requests;

/******************************************************************************/

/* Gnutella URL format:
 *
 * (This will change at least once more)
 *
 * Gnutella://<user>:<port>:<client_guid>/<index>/<filename>
 */
#if 0
char *gt_share_url_new (GtShare *share, in_addr_t ip, in_port_t port,
                        gt_guid_t *client_guid)
{
	char *buf;
	char *encoded;

	buf = malloc (1024);

	if (!(encoded = gt_url_encode (share->filename)))
	{
		free (buf);
		return NULL;
	}

	snprintf (buf, 1024, "%s://%s:%hu:%s/%u/%s", gt_proto->name,
	          net_ip_str (ip), port, gt_guid_str (client_guid), share->index,
	          encoded);

	GT->DBGFN (GT, "index=%d, filename=%s, url=%s", share->index, encoded, buf);

	free (encoded);

	return buf;
}
#endif

/* hopefully temporary code to make push downloads work
 *
 * server_port is the server's gnutella port. This should probably pass
 * back both the gnutella port instead and the peer's connecting port, to
 * help in disambiguating different users behind the same firewall. */
char *gt_source_url_new (char *filename, uint32_t index,
                        in_addr_t user_ip, uint16_t user_port,
                        in_addr_t server_ip, uint16_t server_port,
                        int firewalled, gt_guid_t *client_id)
{
	char *buf;
	char *encoded;
	char *server;
	char *user;

	buf = malloc (2048);

	if (!(encoded = gt_url_encode (filename)))
	{
		free (buf);
		return NULL;
	}

	user   = STRDUP (net_ip_str (user_ip));
	server = STRDUP (net_ip_str (server_ip));

	snprintf (buf, 2048, "%s://%s:%hu@%s:%hu[%s]:%s/%u/%s", gt_proto->name,
	          user, user_port, server, server_port,
	          (firewalled ? "FW" : ""), gt_guid_str (client_id),
	          index, encoded);

	free (server);
	free (user);
	free (encoded);

	return buf;
}

char *gt_share_url_new (GtShare *share,
                        in_addr_t user_ip, uint16_t user_port,
                        in_addr_t server_ip, uint16_t server_port,
                        int firewalled, gt_guid_t *client_id)
{
	return gt_source_url_new (share->filename, share->index,
	                          user_ip, user_port, server_ip, server_port,
	                          firewalled, client_id);
}

#if 0
static int parse_url (char *url, uint32_t *r_ip, uint16_t *r_port,
                      char **r_pushid, uint32_t *r_index, char **r_fname)
{
	string_sep (&url, "://");

	/* TODO: check for more errors */

	*r_ip     = inet_addr (string_sep (&url, ":"));
	*r_port   = ATOUL     (string_sep (&url, ":"));
	*r_pushid =            string_sep (&url, "/");
	*r_index  = ATOUL     (string_sep (&url, "/"));
	*r_fname  = url;

	return TRUE;
}
#endif

static int parse_url (char *url, uint32_t *r_user_ip, uint16_t *r_user_port,
                      uint32_t *r_server_ip, uint16_t *r_server_port,
                      int *firewalled, char **r_pushid,
                      uint32_t *r_index, char **r_fname)
{
	char *port_and_flags;
	char *flag;

	string_sep (&url, "://");

	/* TODO: check for more errors */

	*r_user_ip     = inet_addr (string_sep (&url, ":"));
	*r_user_port   = ATOUL     (string_sep (&url, "@"));
	*r_server_ip   = inet_addr (string_sep (&url, ":"));

	/* handle bracketed flags after port. ugh, this is so ugly */
	port_and_flags = string_sep (&url, ":");
	*r_server_port = ATOUL (string_sep (&port_and_flags, "["));

	if (!string_isempty (port_and_flags))
	{
		/* grab any flags inside the brackets */
		while ((flag = string_sep_set (&port_and_flags, ",]")))
		{
			if (!STRCMP (flag, "FW"))
				*firewalled = TRUE;
		}
	}

	*r_pushid      =            string_sep (&url, "/");
	*r_index       = ATOUL     (string_sep (&url, "/"));
	*r_fname       = url;

	return TRUE;
}

GtSource *gt_source_new (char *url)
{
	GtSource  *new_gt;
	char      *fname;
	char      *url0;
	char      *guid_ascii;

	url0 = url;
	if (!(url = STRDUP (url0)))
		return NULL;

	if (!(new_gt = malloc (sizeof (GtSource))))
	{
		free (url);
		return NULL;
	}

	memset (new_gt, 0, sizeof (GtSource));

#if 0
	if (!parse_url (url, &new_gt->ip, &new_gt->port, &push_id,
	                &new_gt->index, &fname))
#endif
	if (!parse_url (url, &new_gt->user_ip, &new_gt->user_port,
	                &new_gt->server_ip, &new_gt->server_port,
	                &new_gt->firewalled, &guid_ascii, &new_gt->index, &fname))
	{
		free (url);
		free (new_gt);
		return NULL;
	}

	new_gt->filename = STRDUP (fname);
	new_gt->guid     = gt_guid_bin (guid_ascii);

	free (url);

	return new_gt;
}

/*
 * The source URL is stored on disk and could be outdated if the format has
 * changed. This updates it with any changes.
 */
static void replace_url (Source *src, GtSource *gt)
{
	char *url;
	char *decoded;

	/* need to decode the filename, as it is encoded */
	if (!(decoded = gt_url_decode (gt->filename)))
		return;

	if (!(url = gt_source_url_new (decoded, gt->index,
	                               gt->user_ip, gt->user_port,
	                               gt->server_ip, gt->server_port,
	                               gt->firewalled, gt->guid)))
	{
		free (decoded);
		return;
	}

	free (decoded);

	/* swap urls */
	free (src->url);
	src->url = url;
}

void gt_source_free (GtSource *gt)
{
	if (!gt)
		return;

	free (gt->guid);
	free (gt->filename);
	free (gt);
}

/******************************************************************************/

static FileShare *lookup_index (GtTransfer *xfer, char *request)
{
	FileShare *file;
	char      *index;
	char      *filename;

	filename = request;
	index    = string_sep (&filename, "/");

	if (!filename || !index)
		return NULL;

	/* the filename may or may not be url encoded */
	filename = gt_url_decode (filename);
	file = gt_share_local_lookup_by_index (ATOUL (index), filename);
	free (filename);

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

	if (file)
		GT->dbg (GT, "file=%s", share_get_hpath (file));

	return file;
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
		GT->DBGFN (GT, "received unimplemented Browse Host request");
		return NULL;
	}

	if (authorized)
		*authorized = FALSE;

	if (!(path0 = path = STRDUP (s_path)))
		return NULL;

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
		file = NULL;

	/*
	 * Set xfer->content_urn [which replies with 'X-Gnutella-Content-URN']
	 * to a comma-separated list of all URNs for this file.
	 */
	xfer->content_urns = gt_share_local_get_urns (file);

	if (!file)
	{
		GT->DBGFN (GT, "bad request: %s %s", namespace, path);
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
                            uint32_t index, char *filename)
{
	/* NOTE: filename doesnt have leading '/' here, that may change */
	snprintf (request, size - 1, "/get/%u/%s", index, filename);
	return request;
}

/*
 * Setup a request string. Try request-by-hash (/uri-res/N2R?urn:sha1:..),
 * but if there are any problems, fallback to a "/get/<index>/<filename>"
 * request.
 */
static char *request_str (Chunk *chunk, uint32_t index, char *filename)
{
	static char request[RW_BUFFER];
	char       *hash = chunk->transfer->hash;

	/*
	 * Sigh, the uri-res method doesn't seem to work with many clients,
	 * so we do it probablistically. Ideally, we would attach some
	 * info to the download as to whether the uri-res failed or not,
	 * either through the source url or some other way, and if it failed
	 * fallback to an index request.
	 */
	if (hash && ((3.0 * rand() / (RAND_MAX + 1.0) > 1) ||
	             string_isempty (filename)))
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

	src->guid     = gt_guid_dup (guid);
	src->ip       = ip;
	src->src_ip   = src_ip;

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

time_t gt_push_source_last_sent_time_get (gt_guid_t *guid, in_addr_t ip)
{
	GtPushSource *src;

	if (!(src = push_source_lookup (guid, ip)))
		return 0;

	return src->last_sent;
}

void gt_push_source_last_sent_time_set (gt_guid_t *guid, in_addr_t ip, time_t t)
{
	GtPushSource *src;

	if (!(src = push_source_lookup (guid, ip)))
		return;

	src->last_sent = t;
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

static void continue_download (GtTransfer *xfer, TCPC *c)
{
	Chunk      *chunk = NULL;

	gt_transfer_unref (NULL, &chunk, &xfer);
	assert (chunk != NULL);

	/* remove all previous inputs */
	flush_inputs (c);

	/* connect the Chunk, TCPC, and GtTransfer */
	gt_transfer_ref (c, chunk, xfer);

	/* the connection and the chunk have met up */
	gt_transfer_status (xfer, SOURCE_WAITING, "Received GIV response");

	GT->dbg (GT, "Continuing download for %s [%s]", xfer->request,
	         net_peer_ip (c->fd));

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)gt_http_client_start, TIMEOUT_DEF);
}

static void reset_conn (int fd, input_id id, TCPC *c)
{
	/* We should only get here if some data was sent, or if it timed out.
	 * In which case we should close this connection, because it shouldn't
	 * be sending anything */
	GT->DBGFN (GT, "connection to [%s] timed out or sent invalid data",
	           net_peer_ip (c->fd));

	gt_push_source_remove_conn (c);
	tcp_close (c);
}

static void store_conn (GtPushSource *src, TCPC *c)
{
	flush_inputs (c);

	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)reset_conn, 4 * MINUTES);

	assert (!list_find (src->connections, c));
	src->connections = list_prepend (src->connections, c);

	GT->dbg (GT, "storing connection for [%s:%hu]",
	         net_ip_str (c->fd), c->port);
}

int gt_push_source_add_conn (gt_guid_t *guid, in_addr_t ip, TCPC *c)
{
	GtTransfer    *xfer;
	GtPushSource  *push_src;

	if (!(push_src = push_source_lookup (guid, ip)))
	{
		GT->err (GT, "couldn't find push source %s:[%s]", gt_guid_str (guid),
		         net_ip_str (ip));
		tcp_close (c);
		return FALSE;
	}

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

	continue_download (xfer, c);
	return TRUE;
}

static void store_xfer (GtPushSource *src, GtTransfer *xfer)
{
	assert (!list_find (src->xfers, xfer));
	src->xfers = list_prepend (src->xfers, xfer);
}

int gt_push_source_add_xfer (gt_guid_t *guid, in_addr_t ip,
                             in_addr_t src_ip, GtTransfer *xfer)
{
	TCPC           *c;
	GtPushSource   *push_src;

	assert (xfer != NULL);

	/* create the source if it doesn't exist already */
	gt_push_source_add (guid, ip, src_ip);

	if (!(push_src = push_source_lookup (guid, ip)))
	{
		GT->err (GT, "couldn't find push source (%s:[%s]) for chunk %s",
		         gt_guid_str (guid), net_ip_str (ip), xfer->request);
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

	continue_download (xfer, c);
	return TRUE;
}

/*****************************************************************************/

static int remove_xfer (GtPushSource *src, GtTransfer *xfer)
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

static int remove_conn (GtPushSource *src, TCPC *c)
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

static int cleanup_xfer (GtTransfer *xfer, void *udata)
{
	gt_push_source_remove_xfer (xfer);
	return TRUE;
}

static int cleanup_conn (TCPC *c, void *udata)
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

static void send_push (GtTransfer *xfer, GtSource *gt, TCPC *server)
{
	GtPacket *packet;

	if (!(packet = gt_packet_new (GT_PUSH_REQUEST, PUSH_MAX_TTL, NULL)))
		return;

	gt_packet_put_ustr   (packet, gt->guid, 16);
	gt_packet_put_uint32 (packet, gt->index);
	gt_packet_put_ip     (packet, GT_NODE(server)->my_ip);
	gt_packet_put_port   (packet, gt_self->gt_port);

	if (gt_packet_error (packet))
	{
		gt_packet_free (packet);
		return;
	}

	gt_packet_send (server, packet);

	/*
	 * Set the last time we sent a push to now.
	 */
	gt_push_source_last_sent_time_set (gt->guid, gt->user_ip, time (NULL));
	gt_transfer_status (xfer, SOURCE_WAITING, "Sent PUSH request");
}

static int send_push_to_server (in_addr_t server_ip, in_port_t server_port,
                                GtTransfer *xfer, GtSource *gt)
{
	GtNode *server;

	if (!(server = gt_node_lookup (server_ip, server_port)))
	{
		server = gt_node_register (server_ip, server_port,
		                           GT_NODE_ULTRA, 0, 0);
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
	         gt_connect (server) >= 0)
	{
		/* We tried to connect to the server so we could deliver the
		 * push request eventually
		 * NOTE: this doesnt send a push until the next chunk timeout */
		return FALSE;
	}

	return FALSE;
}

static void handle_push_download (Chunk *chunk, GtTransfer *xfer, GtSource *gt)
{
	GtNode *server;
	time_t  last_time;
	time_t  interval;

	/* connect the Chunk and the GtTransfer */
	gt_transfer_ref (NULL, chunk, xfer);

	/*
	 * If this succeeds, we already have a connection to this
	 * user and the transfer will continue by using that connection.
	 *
	 * TODO: the gt_push_source_add() should be used by some
	 *       per-source data structure
	 */
	if (gt_push_source_add_xfer (gt->guid, gt->user_ip, gt->server_ip, xfer))
		return;

	last_time = gt_push_source_last_sent_time_get (gt->guid, gt->user_ip);

	/*
	 * The interval we use for sending push requests.
	 *
	 * This is offset slightly randomly in order to try to not send the
	 * requests for all downloads at once.
	 */
	interval = PUSH_SEND_INTERVAL + (4.0 * rand () / (RAND_MAX + 1.0));

	/*
	 * Dont send pushes too often. Maybe should use a global queue instead.
	 */
	if (last_time > 0 && time (NULL) - last_time < interval)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Awaiting connection");
		gt_transfer_close (xfer, TRUE);
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

	gt_transfer_status (xfer, SOURCE_CANCELLED, "No PUSH route");
	gt_transfer_close (xfer, TRUE);
}

static int set_request (GtTransfer *xfer, Chunk *chunk, GtSource *gt_src)
{
	char  *request;

	if (!chunk || !xfer)
		return FALSE;

	request = request_str (chunk, gt_src->index, gt_src->filename);

	if (!gt_transfer_set_request (xfer, request))
	{
		GT->DBGFN (GT, "UI made an invalid request for '%s'", request);
		return FALSE;
	}

	return TRUE;
}

static int is_firewalled_source (GtSource *gt)
{
	if (gt_is_local_ip (gt->user_ip, gt->server_ip))
		return TRUE;

	if (gt->firewalled)
		return TRUE;

	return FALSE;
}

static int self_is_firewalled (void)
{
	if (!gt_self->firewalled)
		return FALSE;

	/* Pretend we are not firewalled at the beginning in order
	 * to possibly get more connections, to prove we are not firewalled. */
	if (gt_uptime () < 10 * EMINUTES)
		return FALSE;

	/* we are firewalled */
	return TRUE;
}

static void handle_download (Chunk *chunk, GtTransfer *xfer, GtSource *gt)
{
	TCPC *persistent;

	persistent = gt_http_connection_lookup (gt_http, gt->user_ip,
	                                        gt->user_port,
	                                        GT_TRANSFER_DOWNLOAD);

	/* need to close the connection to re-add it to the list, because
	 * _lookup removes it from the persistent connection list */
	gt_http_connection_close (gt_http, persistent, FALSE, GT_TRANSFER_DOWNLOAD);

	/*
	 * Probabalistically do a push request, but only if we are not
	 * firewalled. It would be much better to do this based on which method
	 * the last request was, but there's currently no way to store this
	 * other than mangling the source url which is ugly.
	 */
	if (!gt_guid_is_empty (gt->guid) &&
	    !persistent &&
	    !self_is_firewalled() && (is_firewalled_source (gt) ||
	                              10.0 * rand() / (RAND_MAX+1.0) < 1.0))
	{
		handle_push_download (chunk, xfer, gt);
	}
	else
	{
		gt_http_client_get (gt_http, chunk, xfer);
	}
}

int gnutella_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                             Source *source)
{
	GtTransfer    *xfer;
	GtSource      *gt;
	off_t          start;
	off_t          stop;

	if (!(gt = gt_source_new (source->url)))
		return FALSE;

	replace_url (source, gt);

#if 0
	GT->dbg (GT, "ip=%s port=%hu guid=%s index=%u filename=%s",
	         net_ip_str (gt->user_ip), gt->user_port,
	         gt_guid_str (gt->guid), gt->index, gt->filename);
#endif

	/* thank you, pretender :) */
	start = chunk->start + chunk->transmit;
	stop  = chunk->stop;

	if (!(xfer = gt_transfer_new (gt_http, GT_TRANSFER_DOWNLOAD, gt->user_ip,
	                              gt->user_port, start, stop)))
	{
		GT->DBGFN (GT, "gt_transfer_new failed");
		gt_source_free (gt);
		return FALSE;
	}

	if (!set_request (xfer, chunk, gt))
	{
		gt_transfer_close (xfer, TRUE);
		gt_source_free (gt);
		return FALSE;
	}

	handle_download (chunk, xfer, gt);
	gt_source_free (gt);

	return TRUE;
}

void gnutella_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                             Source *source, int complete)
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

/*****************************************************************************/

int gnutella_source_cmp (Protocol *p, Source *a, Source *b)
{
	GtSource *gt_a = NULL;
	GtSource *gt_b = NULL;
	int       ret  = 0;

	if (!(gt_a = gt_source_new (a->url)) ||
	    !(gt_b = gt_source_new (b->url)))
	{
		gt_source_free (gt_a);
		gt_source_free (gt_b);
		return -1;
	}

	if (gt_a->user_ip > gt_b->user_ip)
		ret =  1;
	else if (gt_a->user_ip < gt_b->user_ip)
		ret = -1;

	if (gt_a->user_port > gt_b->user_port)
		ret =  1;
	else if (gt_a->user_port < gt_b->user_port)
		ret = -1;

	/* if both IPs are private match by the guid */
	if (gt_is_local_ip (gt_a->user_ip, gt_a->server_ip) &&
	    gt_is_local_ip (gt_b->user_ip, gt_b->server_ip))
	{
		ret = gt_guid_cmp (gt_a->guid, gt_b->guid);
	}

	if (ret == 0)
	{
		/* if the hashes match consider them equal */
		if (a->hash && b->hash)
			ret = strcmp (a->hash, b->hash);
		else
			ret = strcmp (gt_a->filename, gt_b->filename);
#if 0
			/* should we do this? */
			ret |= INTCMP (gt_a->index,    gt_b->index);
#endif
	}

	gt_source_free (gt_a);
	gt_source_free (gt_b);

	return ret;
}

int gnutella_source_add (Protocol *p, Transfer *transfer, Source *source)
{
	/* TODO */
	return TRUE;
}

void gnutella_source_remove (Protocol *p, Transfer *transfer, Source *source)
{
	/* TODO */
}
