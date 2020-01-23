/*
 * $Id: gt_accept.c,v 1.64 2005/01/04 15:03:40 mkern Exp $
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

#include "gt_packet.h"
#include "gt_utils.h"

#include "gt_connect.h"

#include "gt_node.h"
#include "gt_node_cache.h"
#include "gt_netorg.h"

#include "gt_xfer_obj.h"
#include "gt_xfer.h"
#include "gt_http_server.h"

#include "gt_accept.h"
#include "gt_bind.h"
#include "gt_ban.h"
#include "gt_version.h"

#include "message/gt_message.h"    /* gnutella_start_connection */

/*****************************************************************************/

#define MAX_FDBUF_SIZE     16384

#define INCOMING_TIMEOUT   (1 * MINUTES)

/*****************************************************************************/
/* Handle incoming connections to this node */

/*
 * Wrap TCPC and associated timeout for incoming connections in order to limit
 * the number of incoming connections.
 */
struct incoming_conn
{
	TCPC      *c;
	timer_id   timer;
};

#define GT_METHOD(func) void func (int fd, input_id id, \
                                   struct incoming_conn *conn)
typedef void (*GtAcceptFunc) (int fd, input_id id, struct incoming_conn *conn);

GT_METHOD(gt_server_accept);
GT_METHOD(gt_server_get);
GT_METHOD(gt_server_giv);

static struct server_cmd
{
	char         *name;
	GtAcceptFunc  callback;
}
server_commands[] =
{
	{ "GNUTELLA",   gt_server_accept    },
	{ "GET",        gt_server_get       },
	{ "HEAD",       gt_server_get       },
	{ "GIV",        gt_server_giv       },
	{ NULL,         NULL                }
};

/*****************************************************************************/

static void send_node_headers    (int fd, input_id id, TCPC *c);
static void recv_final_handshake (int fd, input_id id, TCPC *c);
static void determine_method     (int fd, input_id id,
                                  struct incoming_conn *conn);

/*****************************************************************************/

static void incoming_conn_free (struct incoming_conn *conn)
{
	timer_remove (conn->timer);
	free (conn);
}

static void incoming_conn_close (struct incoming_conn *conn)
{
	tcp_close (conn->c);
	incoming_conn_free (conn);
}

static BOOL conn_timeout (struct incoming_conn *conn)
{
	incoming_conn_close (conn);
	return FALSE;
}

static struct incoming_conn *incoming_conn_alloc (TCPC *c)
{
	struct incoming_conn *conn;

	conn = malloc (sizeof (struct incoming_conn));
	if (!conn)
		return NULL;

	conn->c = c;
	conn->timer = timer_add (INCOMING_TIMEOUT, (TimerCallback)conn_timeout,
	                         conn);

	return conn;
}

/*****************************************************************************/

BOOL gt_fdbuf_full (FDBuf *buf)
{
	size_t len = MAX_FDBUF_SIZE;

	if (fdbuf_data (buf, &len) == NULL)
		return TRUE;

	return len >= MAX_FDBUF_SIZE;
}

/*****************************************************************************/

/*
 * Receive an incoming connection.
 */
void gnutella_handle_incoming (int fd, input_id id, TCPC *listen)
{
	TCPC *c;

	if (!(c = tcp_accept (listen, FALSE)))
		return;

	if (HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "got a new connection");

	id = INPUT_NONE;
	gt_handshake_dispatch_incoming (fd, id, c);
}

/*
 * Mark not firewalled if the other end isn't local.
 */
static void fw_status_update (TCPC *c)
{
	if (!c->outgoing && !net_match_host (c->host, "LOCAL"))
	{
		if (GT_SELF->firewalled)
			GT->DBGSOCK (GT, c, "connected, clearing firewalled status");

		gt_bind_clear_firewalled ();
	}
}

/*
 * Allocate a structure to track this connection and continue the dispatching
 * process.
 */
void gt_handshake_dispatch_incoming (int fd, input_id id, TCPC *c)
{
	struct incoming_conn *conn;
	in_addr_t             peer_ip;

	/*
	 * This will trigger if the connection was closed on the remote end
	 * or if there's a input timer setup to timeout this connection.  In the
	 * latter case, we have to avoid adding any inputs for the connection.
	 */
	if (net_sock_error (c->fd))
	{
		tcp_close (c);
		return;
	}

	peer_ip = net_peer (c->fd);

	/*
	 * While this might be nice for the banner, it's not nice to the ban-ee,
	 * and I'm sure there's some bugs in the banning code, so we should
	 * send back an error instead.
	 */
#if 0
	if (gt_ban_ipv4_is_banned (peer_ip))
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "not accepting connection [address banned]");

		tcp_close (c);
		return;
	}
#endif

	/*
	 * If there are too many HTTP connections for this IP, close the
	 * connection now before investing any more resources servicing it.
	 */
	if (gt_http_connection_length (GT_TRANSFER_UPLOAD, peer_ip) >=
	    HTTP_MAX_PERUSER_UPLOAD_CONNS)
	{
		if (HTTP_DEBUG)
			GT->DBGSOCK (GT, c, "too many connections for this user, closing");

		tcp_close (c);
		return;
	}

	/* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (GNUTELLA_LOCAL_MODE)
	{
		if (!net_match_host (peer_ip, GNUTELLA_LOCAL_ALLOW))
		{
			if (HANDSHAKE_DEBUG)
				GT->DBGSOCK (GT, c, "non-local connection, closing");

			tcp_close (c);
			return;
		}
	}

	if (!(conn = incoming_conn_alloc (c)))
	{
		tcp_close (c);
		return;
	}

	input_remove (id);
	input_add (c->fd, conn, INPUT_READ,
	           (InputCallback)determine_method, 0);
}

/*
 * Dispatch incoming connections to the proper subsystem.
 */
static void determine_method (int fd, input_id id, struct incoming_conn *conn)
{
	struct server_cmd  *command;
	FDBuf              *fdbuf;
	int                 ret;
	char               *request;
	TCPC               *c       = conn->c;

	fdbuf = tcp_readbuf (c);

	if ((ret = fdbuf_delim (fdbuf, "\n")) < 0)
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "error: %s", GIFT_NETERROR ());

		incoming_conn_close (conn);
		return;
	}

	/* some data was read: update fw status */
	fw_status_update (c);

	if (gt_fdbuf_full (fdbuf))
	{
		incoming_conn_close (conn);
		return;
	}

	if (ret > 0)
		return;

	/*
	 * NOTE: fdbuf_release() is not called here, so the first line is left on
	 * the FDBuf.
	 */
	request = fdbuf_data (fdbuf, NULL);

	for (command = server_commands; command->name != NULL; command++)
	{
		if (!strncasecmp (command->name, request, strlen (command->name)))
		{
			input_remove (id);
			input_add (fd, conn, INPUT_READ,
			           (InputCallback)command->callback, 0);

			return;
		}
	}

	if (HANDSHAKE_DEBUG)
		GT->DBGFN (GT, "bad command: %s", request);

	incoming_conn_close (conn);
}

/*****************************************************************************/

/*
 * Main connection acceptance routine.  This begins the connection's
 * journey through the rest of the handshaking code.
 */
GT_METHOD(gt_server_accept)
{
	char         *request;
	char         *version_str;
	size_t        request_len = 0;
	int           n;
	GtNode       *node;
	TCPC         *c           = conn->c;
	FDBuf        *buf;

	if (HANDSHAKE_DEBUG)
		GT->DBGFN (GT, "entered");

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "error on recv: %s", GIFT_NETERROR ());

		incoming_conn_close (conn);
		return;
	}

	if (gt_fdbuf_full (buf))
	{
		incoming_conn_close (conn);
		return;
	}

	if (n > 0)
		return;

	request = fdbuf_data (buf, &request_len);

	if (!gt_http_header_terminated (request, request_len))
		return;

	fdbuf_release (buf);

	if (HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "accepted headers:\n%s", request);

	version_str = strchr (request, '/');
	if (version_str)
		version_str++;

	if (strncasecmp ("GNUTELLA CONNECT/", request,
	                  sizeof ("GNUTELLA CONNECT/") - 1) != 0)
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "bad handshake header");

		incoming_conn_close (conn);
		return;
	}

	/* deny 0.4 connections */
	if (!version_str || strncasecmp (version_str, "0.4", 3) == 0)
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "closing 0.4 connection");

		incoming_conn_close (conn);
		return;
	}

	/* make a node out of this connection */
	if (!(node = gt_node_instantiate (c)))
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGFN (GT, "node_instantiate failed");

		incoming_conn_close (conn);
		return;
	}

	/*
	 * Update the start_connect_time so the node will stick around for a while
	 * even when the node list gets trimmed.
	 */
	node->start_connect_time = time (NULL);

	gt_node_state_set (node, GT_NODE_CONNECTING_1);
	node->incoming = TRUE;

	/*
	 * Store the http header.
	 *
	 * NOTE: We don't check the return code here. This function expects a
	 *       properly formatted HTTP response, but we are handing it a
	 *       Gnutella connection request instead, so it will return failure.
	 */
	gnutella_parse_response_headers (request, &node->hdr);

	/*
	 * Use the node handshake timeout timer now, and get rid of the
	 * generic incoming connection timeout timer.
	 */
	gnutella_set_handshake_timeout (c, TIMEOUT_2 * SECONDS);
	incoming_conn_free (conn);

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)send_node_headers, TIMEOUT_DEF);
}

GT_METHOD(gt_server_giv)
{
	FDBuf        *buf;
	int           n;
	in_addr_t     peer_ip;
	char         *response;
	size_t        response_len = 0;
	char         *client_id;
	gt_guid_t    *guid;
	TCPC         *c            = conn->c;

	if (HTTP_DEBUG || HANDSHAKE_DEBUG)
		GT->DBGFN (GT, "entered");

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		incoming_conn_close (conn);
		return;
	}

	if (gt_fdbuf_full (buf))
	{
		incoming_conn_close (conn);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);

	if (!gt_http_header_terminated (response, response_len))
		return;

	fdbuf_release (buf);

	if (HTTP_DEBUG || HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "giv response=%s", response);

	string_sep (&response, " ");           /* Skip "GIV " */
	string_sep (&response, ":");           /* Skip the file index */

	client_id = string_sep (&response, "/");
	string_lower (client_id);

	if (!(guid = gt_guid_bin (client_id)))
	{
		incoming_conn_close (conn);
		return;
	}

	peer_ip = net_peer (c->fd);

	/*
	 * This will continue the GtTransfer if one is found, and store
	 * the connection if one is not. The connection will be resumed
	 * when a chunk for this source is reissued.
	 */
	gt_push_source_add_conn (guid, peer_ip, c);

	incoming_conn_free (conn);
	free (guid);
}

/*****************************************************************************/

GT_METHOD(gt_server_get)
{
	if (HTTP_DEBUG)
		GT->DBGFN (GT, "entered");

	/* dispatch to the HTTP server code */
	gt_http_server_dispatch (fd, id, conn->c);

	/* get rid of the tracking information for the connection in this
	 * subsystem */
	incoming_conn_free (conn);
}

/*****************************************************************************/
/* GNUTELLA/0.6 CONNECTIONS */

BOOL gt_http_header_terminated (char *data, size_t len)
{
	int cnt;

	assert (len > 0);
	len--;

	for (cnt = 0; len > 0 && cnt < 2; cnt++)
	{
		if (data[len--] != '\n')
			break;

		/* treat CRLF as LF */
		if (data[len] == '\r')
			len--;
	}

	return (cnt == 2);
}

/* TODO: header continuation and joining of multiple occurrences */
void gt_http_header_parse (char *headers, Dataset **d)
{
	char *line, *key;

	while ((line = string_sep_set (&headers, "\r\n")))
	{
		key = string_sep (&line, ":");

		if (!key || !line)
			continue;

		string_trim (key);
		string_trim (line);

		/* dont allow empty key-values, need to check this too */
		if (string_isempty (line))
			continue;

		dataset_insertstr (d, string_lower (key), line);
	}
}

static void send_nodes (struct cached_node *node, String *s)
{
	if (s->str[s->len - 1] != ':')
		string_append (s, ",");
	else
		string_append (s, " ");

	string_appendf (s, "%s:%hu", net_ip_str (node->addr.ip), node->addr.port);
}

static void deny_connection (TCPC *c, int code, char *msg)
{
	String    *s;
	List      *nodes;
	in_addr_t  ip;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return;

	string_appendf (s, "GNUTELLA/0.6 %d %s\r\n", code, msg);
	string_appendf (s, "User-Agent: %s\r\n", gt_version());

	ip = net_peer (c->fd);
	if (!gt_is_local_ip (ip, 0))
		string_appendf (s, "Remote-IP: %s\r\n", net_ip_str (ip));

	/* append some different nodes to try */
	nodes = gt_node_cache_get (10);

	if (nodes)
	{
		string_append (s, "X-Try-Ultrapeers:");

		list_foreach (nodes, (ListForeachFunc)send_nodes, s);
		list_free (nodes);

		string_append (s, "\r\n");
	}

	/* append message terminator */
	string_append (s, "\r\n");

	/* we will be closing the connection after this, so we don't
	 * care if the send fails or not. */
	tcp_send (c, s->str, s->len);

	if (HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "connection denied response:\n%s", s->str);

	string_free (s);
}

static void setup_node_class (GtNode *node)
{
	char      *ultrapeer;
	char      *qrp;

	ultrapeer = dataset_lookupstr (node->hdr, "x-ultrapeer");
	qrp       = dataset_lookupstr (node->hdr, "x-query-routing");

	if (ultrapeer && !strcasecmp (ultrapeer, "true") && qrp != NULL)
		gt_node_class_set (node, GT_NODE_ULTRA);
	else
		gt_node_class_set (node, GT_NODE_LEAF);
}

BOOL gnutella_auth_connection (TCPC *c)
{
	GtNode *node;

	node = GT_NODE(c);
	assert (GT_NODE(c) == node && GT_CONN(node) == c);

	/* set the class of this node based on the headers sent */
	setup_node_class (node);

	/*
	 * If the remote node is only crawling us, accept the connection
	 * no matter what.
	 */
	if (dataset_lookupstr (node->hdr, "crawler"))
		return TRUE;

	/*
	 * If we are a leaf node, and so is this node, deny the connection,
	 * but send 'X-Try-Ultrapeer:' headers with the ultrapeers we
	 * are connected to in order to try to bootstrap the remote client.
	 */
	if (!(GT_SELF->klass & GT_NODE_ULTRA) && (node->klass & GT_NODE_LEAF))
	{
		deny_connection (c, 503, "I am a shielded leaf node");
		return FALSE;
	}

	if (gt_conn_need_connections (node->klass) <= 0)
	{
		deny_connection (c, 503, "Too many connections");
		return FALSE;
	}

	if (gt_ban_ipv4_is_banned (node->ip))
	{
		deny_connection (c, 403, "Unauthorized");
		return FALSE;
	}

	return TRUE;
}

static void send_node_headers (int fd, input_id id, TCPC *c)
{
	if (net_sock_error (c->fd))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	if (!gnutella_auth_connection (c))
	{
		gt_node_error (c, "[incoming] connection not authorized");
		gt_node_disconnect (c);
		return;
	}

	/* send OK response to the peer and send headers for this node also */
	if (!gnutella_send_connection_headers (c, "GNUTELLA/0.6 200 OK"))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	/* reset the timeout for this connection */
	gnutella_set_handshake_timeout (c, TIMEOUT_3 * SECONDS);

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)recv_final_handshake, 0);
}

/* TODO: this should be abstracted */
static char *field_has_value (Dataset *d, const char *key, const char *val)
{
	char *value;
	char *str;

	if (!(value = dataset_lookupstr (d, key)))
		return NULL;

	if ((str = strstr (value, val)))
		return str;

	return NULL;
}

static BOOL need_inflate (GtNode *node)
{
	if (!(field_has_value (node->hdr, "content-encoding", "deflate")))
		return FALSE;

	return TRUE;
}

static BOOL need_deflate (GtNode *node)
{
	if (!(field_has_value (node->hdr, "accept-encoding", "deflate")))
		return FALSE;

	return TRUE;
}

void gnutella_mark_compression (GtNode *node)
{
	if (need_inflate (node))
		node->rx_inflated = TRUE;

	if (need_deflate (node))
		node->tx_deflated = TRUE;
}

static void add_key (ds_data_t *key, ds_data_t *value, Dataset **d)
{
	char *hdr = key->data;
	char *val = value->data;

	dataset_insertstr (d, hdr, val);
}

static void recv_final_handshake (int fd, input_id id, TCPC *c)
{
	FDBuf   *buf;
	int      n;
	char    *response;
	size_t   response_len = 0;
	Dataset *additional   = NULL;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "fdbuf_delim: error %s", GIFT_NETERROR ());

		gt_node_disconnect (c);
		return;
	}

	if (gt_fdbuf_full (buf))
	{
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);

	if (!gt_http_header_terminated (response, response_len))
		return;

	fdbuf_release (buf);

	if (HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "stage3 response:\n%s", response);

	if (!gnutella_parse_response_headers (response, &additional))
	{
		if (HANDSHAKE_DEBUG)
			gt_node_error (c, "node denied us in stage3 of handshake");

		gt_node_disconnect (c);
		dataset_clear (additional);
		return;
	}

	/*
	 * Append or replace the fields the node sent into the header for this
	 * node.
	 *
	 * TODO: should probably change the interface to parse_response_headers so
	 * we can just pass in the header list of the node.
	 */
	dataset_foreach (additional, DS_FOREACH(add_key), &GT_NODE(c)->hdr);
	dataset_clear (additional);

	/* mark the compression flags on this GtNode */
	gnutella_mark_compression (GT_NODE(c));

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)gnutella_start_connection, TIMEOUT_DEF);
}
