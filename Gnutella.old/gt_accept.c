/*
 * $Id: gt_accept.c,v 1.36 2003/06/07 07:13:01 hipnod Exp $
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
#include "gt_netorg.h"

#include "ft_xfer.h"
#include "ft_http_server.h"

#include "gt_xfer.h"

#include "gt_accept.h"

/*****************************************************************************/

#define MAX_REQUEST        16384

/*****************************************************************************/
/* Handle incoming connections to this node */

#define GT_METHOD(func) void func (int fd, input_id id, TCPC *c)
typedef void (*GtAcceptFunc) (int fd, input_id id, TCPC *c);

GT_METHOD (gt_server_accept);
GT_METHOD (gt_server_get);
GT_METHOD (gt_server_giv);

static struct _server_cmd
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

static void accept_06               (int fd, input_id id, TCPC *c);
static void send_06_headers         (int fd, input_id id, TCPC *c);
static void recv_06_final_handshake (int fd, input_id id, TCPC *c);

/*****************************************************************************/

/* receive an incoming connection */
void gnutella_handle_incoming (int fd, input_id id, TCPC *c)
{
	TCPC *new_c;

	if (!(new_c = tcp_accept (c, FALSE)))
		return;

	GT->dbgsock (GT, new_c, "got a new connection");

	input_add (new_c->fd, new_c, INPUT_READ,
	           (InputCallback)gnutella_determine_method, TIMEOUT_DEF);
}

/* dispatch incoming connections */
void gnutella_determine_method (int fd, input_id id, TCPC *c)
{
	struct _server_cmd *command;
	int                 len;
	char                buf[MAX_REQUEST];
	BOOL                buf_full;

	len = tcp_peek (c, buf, sizeof (buf) - 1);
	if (len <= 0)
	{
		GT->dbgsock (GT, c, "recv: %d returned, error: %s", len, 
		             GIFT_NETERROR ());
		tcp_close (c);
		return;
	}

	buf[len] = 0;
	buf_full = (len >= sizeof (buf) - 1);

	/* make sure we have a complete request before continuing */
	if (!http_headers_terminated (buf, len))
	{
		/* if the packet is full, we won't reallocate a new buffer */
		if (buf_full)
		{
			GT->dbgsock (GT, c, "headers too large (%u), closing", len);
			tcp_close (c);
		}

		return;
	}

	/*
	 * We successfully read some data. Mark not firewalled.
	 */
	if (!c->outgoing && GT_SELF->firewalled &&
	    !net_match_host (net_peer (fd), "LOCAL"))
	{
		GT->DBGFN (GT, "connection from %s, setting not firewalled", 
		           net_peer_ip (fd));
		GT_SELF->firewalled = FALSE;
	}

	for (command = server_commands; command->name != NULL; command++)
	{
		if (!strncasecmp (command->name, buf, strlen (command->name)))
		{
			input_remove (id);
			input_add (fd, c, INPUT_READ,
			           (InputCallback)command->callback, TIMEOUT_DEF);

			return;
		}
	}

	GT->DBGFN (GT, "bad command: %s", buf);
	tcp_close (c);
}

/*****************************************************************************/

/* Main connection acceptance routine */
GT_METHOD (gt_server_accept)
{
	char          buf[MAX_REQUEST];
	char         *ptr;
	char         *connect_str;
	char         *version_str;
	int           len;
	GtNode       *node;

	GT->DBGFN (GT, "entered");

	/* argh, can't use FDBuf because we need to keep the first
	 * line with the rest of the header for the HTTP code */
	len = tcp_peek (c, buf, sizeof (buf) - 1);
	if (len <= 0)
	{
		GT->DBGFN (GT, "recv: %d returned, error: %s", len, 
		           GIFT_NETERROR ());
		tcp_close (c);
		return;
	}

	/* null terminate the buffer */
	buf[len] = 0;
	ptr = buf;

	              string_sep (&ptr, " ");    /* "GNUTELLA " */
	connect_str = string_sep (&ptr, "/");    /* "CONNECT/"  */
	version_str = string_sep (&ptr, "\n");   /* "0.6\r\n"   */

	if (STRCASECMP ("CONNECT", connect_str) != 0)
	{
		GT->dbgsock (GT, c, "didn't find CONNECT: [%s] instead", connect_str);
		tcp_close (c);
		return;
	}

	if (!version_str)
	{
		GT->dbgsock (GT, c, "didn't find version string in connect line");
		tcp_close (c);
		return;
	}

	/* Make a node out of this connection */
	if (!(node = gt_node_instantiate (c)))
	{
		GT->DBGFN (GT, "node_instantiate failed");
		tcp_close (c);
		return;
	}

	gt_node_state_set (node, GT_NODE_CONNECTING_1);
	node->incoming = TRUE;

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)accept_06, TIMEOUT_DEF);
}

GT_METHOD (gt_server_giv)
{
	FDBuf        *buf;
	int           n;
	in_addr_t     peer_ip;
	char         *response;
	size_t        response_len = 0;
	char         *client_id;
	gt_guid_t    *guid;

	GT->DBGFN (GT, "entered");

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		tcp_close (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);

	if (!http_headers_terminated (response, response_len))
		return;

	fdbuf_release (buf);

	GT->dbgsock (GT, c, "giv response=%s", response);

	string_sep (&response, " ");           /* Skip "GIV " */
	string_sep (&response, ":");           /* Skip the file index */

	client_id = string_sep (&response, "/");
	string_lower (client_id);

	if (!(guid = gt_guid_bin (client_id)))
	{
		tcp_close (c);
		return;
	}

	peer_ip = net_peer (c->fd);

	/* 
	 * This will continue the GtTransfer if one is found, and store
	 * the connection if one is not. The connection will be resumed
	 * when a chunk for this source is reissued.
	 */
	gt_push_source_add_conn (guid, peer_ip, c);
	free (guid);
}

/*****************************************************************************/

GT_METHOD (gt_server_get)
{
	GT->DBGFN (GT, "entered");

	/* dispatch to the HTTP server code */
	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)gt_get_client_request, TIMEOUT_DEF);
}

/*****************************************************************************/
/* GNUTELLA/0.6 CONNECTIONS */

int http_headers_terminated (char *data, size_t len)
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

/* Ack, duplicated with http_client.c */
void http_headers_parse (char *headers, Dataset **d)
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

static void accept_06 (int fd, input_id id, TCPC *c)
{
	FDBuf *buf;
	char  *response;
	size_t response_len = 0;
	int    n;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		GT->dbgsock (GT, c, "error on recv: %s", GIFT_NETERROR ());
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);
	
	if (!http_headers_terminated (response, response_len))
		return;

	fdbuf_release (buf);
	GT->dbgsock (GT, c, "accepted headers:\n%s", response);

	/* 
	 * Store the http headers in the node capabilities.
	 *
	 * NOTE: We don't check the return code here. This function expects a
	 *       properly formatted HTTP response, but we are handing it a 
	 *       gnutella connection request instead, so it will return failure.
	 */
	gnutella_parse_response_headers (response, &GT_NODE(c)->cap);

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)send_06_headers, TIMEOUT_DEF);
}

static GtNode *send_ultrapeers (TCPC *c, GtNode *node, String *s)
{
	if (s->str[s->len - 1] != ':')
		string_append (s, ",");
	else
		string_append (s, " ");

	string_appendf (s, "%s:%hu", net_ip_str (node->ip), node->gt_port);
	return NULL;
}

static void deny_connection (TCPC *c, int code, char *msg)
{
	String    *s;
	int        len;
	in_addr_t  ip;

	/* 
	 * It would be better to send nodes we learned about from fresh pongs, 
	 * instead. 
	 */
	
	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return;

	string_appendf (s, "GNUTELLA/0.6 %d %s\r\n", code, msg);
	string_appendf (s, "User-Agent: %s\r\n", gt_version());

	ip = net_peer (c->fd);
	if (!gt_is_local_ip (ip, 0))
		string_appendf (s, "Remote-IP: %s\r\n", net_ip_str (ip));

	len = gt_conn_length (GT_NODE_ULTRA, GT_NODE_CONNECTED);

	if (len > 0)
	{
		string_append (s, "X-Try-Ultrapeers:");
		gt_conn_foreach ((GtConnForeachFunc)send_ultrapeers, s,
		                 GT_NODE_ULTRA, GT_NODE_CONNECTED, 0);
		string_append (s, "\r\n");
	}

	/* append message terminator */
	string_append (s, "\r\n");

	/* we will be closing the connection after this, so we don't
	 * care if the send fails or not. */
	tcp_send (c, s->str, s->len);

	GT->dbgsock (GT, c, "connection denied response:\n%s", s->str);
	string_free (s);
}

static void setup_node_class (GtNode *node)
{
	char      *ultrapeer;
	char      *qrp;

	ultrapeer = dataset_lookupstr (node->cap, "x-ultrapeer");
	qrp       = dataset_lookupstr (node->cap, "x-query-routing");

	if (ultrapeer && !strcasecmp (ultrapeer, "true") && qrp != NULL)
		gt_node_class_set (node, GT_NODE_ULTRA);
	else
		gt_node_class_set (node, GT_NODE_LEAF);
}

int gnutella_auth_connection (TCPC *c)
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
	if (dataset_lookupstr (GT_NODE(c)->cap, "crawler"))
		return TRUE;

	/*
	 * If we are a leaf node, and so is this node, deny the connection,
	 * but send 'X-Try-Ultrapeer:' headers with the ultrapeers we
	 * are connected to in order to try to bootstrap the remote client.
	 */
	if (!(GT_SELF->klass & GT_NODE_ULTRA) && node->klass & GT_NODE_LEAF)
	{
		deny_connection (c, 503, "I am a shielded leaf node");
		return FALSE;
	}

	if (gt_conn_need_connections (node->klass) <= 0)
	{
		deny_connection (c, 503, "Too many connections");
		return FALSE;
	}
	
	return TRUE;
}

static void send_06_headers (int fd, input_id id, TCPC *c)
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

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)recv_06_final_handshake, TIMEOUT_DEF);
}

static void recv_06_final_handshake (int fd, input_id id, TCPC *c)
{
	FDBuf   *buf;
	int      n;
	char    *response;
	size_t   response_len = 0;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		GT->dbgsock (GT, c, "fdbuf_delim: error %s", GIFT_NETERROR ());
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);

	if (!http_headers_terminated (response, response_len))
		return;

	fdbuf_release (buf);
	GT->dbgsock (GT, c, "stage3 response:\n%s", response);

	if (!gnutella_parse_response_headers (response, NULL))
	{
		gt_node_error (c, "node denied us in stage3 of handshake");
		gt_node_disconnect (c);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)gnutella_start_connection, TIMEOUT_DEF);
}
