/*
 * $Id: gt_accept.c,v 1.14 2003/04/27 05:05:34 hipnod Exp $
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

#include "ft_xfer.h"
#include "ft_http_server.h"

#include "gt_xfer.h"

/*****************************************************************************/

/*
 * Handle incoming connections to this node
 * This is all nowhere near as clean and well-separated as OpenFT :-(
 *
 * should probably toss Gnutella 0.4 compatability
 */

/*****************************************************************************/

#define CONNECT_04            "CONNECT/0.4"        /* accept CRLF & LFLF */
#define CONNECT_04_REPLY      "GNUTELLA OK\n\n"

#define CONNECT_06            "CONNECT/0.6\r\n"    /* accept CRLF */

#define CONNECT_06_REPLY_OK   "GNUTELLA/0.6 200 OK\r\n" \
                              "X-Ultrapeer: True\r\n" \
                              "\r\n"

/*****************************************************************************/

#define GT_METHOD(func) void gt_##func (int fd, input_id id, Connection *c)
typedef void (*GtAcceptFunc) (int fd, input_id id, Connection *c);

GT_METHOD (server_accept);
GT_METHOD (server_get);
GT_METHOD (server_giv);

static struct _server_cmd
{
	char         *name;
	GtAcceptFunc  callback;
}
server_commands[] =
{
	{ "GNUTELLA",   gt_server_accept    },
	{ "GET",        gt_server_get       },
	{ "GIV",        gt_server_giv       },
	{ NULL,         NULL                }
};

/*****************************************************************************/

static void accept_04   (int fd, input_id id, Connection *c);
static void accept_06   (int fd, input_id id, Connection *c);

static void send_06_headers         (int fd, input_id id, Connection *c);
static void recv_06_final_handshake (int fd, input_id id, Connection *c);

/*****************************************************************************/

/* dispatch incoming connections */
static void gnutella_determine_method (int fd, input_id id, Connection *c)
{
	struct _server_cmd *command;
	int                 len;
	char                buf[RW_BUFFER];

	len = recv (c->fd, buf, sizeof (buf), MSG_PEEK);
	if (len <= 0)
	{
		TRACE (("recv: %d returned, error: %s", len, GIFT_STRERROR ()));
		tcp_close (c);
		return;
	}

	/*
	 * We successfully read some data. Mark not firewalled.
	 */
	if (!c->outgoing && GT_SELF->firewalled && 
	    !net_match_host (net_peer (fd), "LOCAL"))
	{
		TRACE (("connection from %s, setting not firewalled",
		        net_peer_ip (fd)));
		GT_SELF->firewalled = FALSE;
	}

	TRACE (("c->fd = %d, ret = %d", c->fd, len));
	for (command = server_commands; command->name != NULL; command++)
	{
		if (!strncasecmp (command->name, buf, strlen (command->name)))
		{
			input_remove (id);
			input_add (fd, c, INPUT_READ,
			           (InputCallback) command->callback, TIMEOUT_DEF);

			return;
		}
	}

	/* technically, its possible to get here without a bad request,
	 * if its only partially complete, because of MSG_PEEK. Sigh */
	TRACE (("bad command: %s", make_str (buf, len)));

	tcp_close (c);
}

void gnutella_handle_incoming (int fd, input_id id, Connection *c)
{
	Connection *new_c;

	if (!(new_c = tcp_accept (c, FALSE)))
		return;

	TRACE (("got a new connection"));

	input_add (new_c->fd, new_c, INPUT_READ,
	           (InputCallback) gnutella_determine_method, TIMEOUT_DEF);
}

/*****************************************************************************/

/* Main connection: Accept both 0.6 and 0.4 connections */
GT_METHOD (server_accept)
{
	char          buf[RW_BUFFER];
	char         *ptr, *start;
	char         *connect_str;
	char         *version_str;
	float         version;
	int           len;
	GtAcceptFunc  callback;
	GtNode       *node;
	InputState    state;

	TRACE_FUNC ();

	/* argh, cant use NBRead because there are two different
	 * possible terminators (\r\n and \n\n) */
	len = recv (c->fd, buf, sizeof (buf), MSG_PEEK);
	if (len <= 0)
	{
		TRACE (("c->fd = %d", c->fd));
		TRACE (("recv: %d returned, error: %s", len, GIFT_STRERROR ()));
		tcp_close (c);
		return;
	}

	ptr = start = make_str (buf, len);

	              string_sep (&ptr, " ");    /* "GNUTELLA " */
	connect_str = string_sep (&ptr, "/");    /* "CONNECT/"  */
	version_str = string_sep (&ptr, "\n");   /* "0.6\r\n"   */

	if (strncasecmp (connect_str, "CONNECT", strlen (connect_str)))
	{
		TRACE (("didn't find CONNECT: [%s] instead", connect_str));
		tcp_close (c);
		return;
	}

	/* Is sscanf() safe to use here? */
	sscanf (version_str, "%f", &version);
	TRACE (("version = %f", version));

	if (version < 0.6)
	{
		ptr++;                 /* possibly grab extra newline */

		callback = accept_04;
		state    = INPUT_WRITE;
	}
	else
	{
		callback = accept_06;
		state    = INPUT_READ;
	}

	TRACE (("pushing %d bytes off fd", ptr - start));

	/* push the bytes read off the fd */
	if ((len = recv (c->fd, buf, ptr - start, 0)) <= 0)
	{
		TRACE (("recv error: %s", GIFT_STRERROR ()));
		tcp_close (c);
		return;
	}

	TRACE (("recv returned %d", len));

	/* Make a node out of this connection */
	if (!(node = gt_node_instantiate (c)))
	{
		TRACE (("node_instantiate failed"));
		tcp_close (c);
		return;
	}

	gt_node_state_set (node, NODE_CONNECTING_1);
	node->incoming = TRUE;

	input_remove (id);
	input_add (fd, c, state,
	           (InputCallback)callback, TIMEOUT_DEF);
}

GT_METHOD (server_giv)
{
	FDBuf        *buf;
	int           n;
	in_addr_t     peer_ip;
	char         *response;
	char         *client_id;
	GtTransfer   *xfer;
	Chunk        *chunk    = NULL;

	TRACE_FUNC ();

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n\n")) < 0)
	{
		tcp_close (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	TRACE (("giv response=%s", response));

	string_sep (&response, " ");           /* Skip "GIV " */
	string_sep (&response, ":");           /* Skip the file index */

	client_id = string_sep (&response, "/");
	string_lower (client_id);

	peer_ip = net_peer (c->fd);

	/*
	 * Implicity trust local clients by not looking up their IP addresses.
	 *
	 * This is a workaround for clients putting the public IP address in
	 * query-hits. Since they give us the public IP address, the lookup on
	 * the IP address will fail. So, the IP address is discarded instead.
	 *
	 * Ideally, there should be some proxying facility, whereby the client
	 * includes both its public and private IP addresses when replying to a
	 * query hit, allowing the peer to select the proper one to connect to,
	 * or to drop the private address when forwarding over the Internet.
	 *
	 * This workaround means anyone on the local network could hijack our
	 * push downloads.
	 */
	if (net_match_host (peer_ip, "LOCAL"))
		peer_ip = 0;

	if (!(chunk = gt_http_server_indirect_lookup (peer_ip, client_id)))
	{
		TRACE (("couldn't find chunk"));
		tcp_close (c);
		return;
	}

	GIFT_DEBUG (("found chunk for %s", client_id));

	/* don't try to connect more than once */
	gt_http_server_indirect_remove (chunk, 0, NULL);

	gt_transfer_unref (NULL, &chunk, &xfer);

	if (!xfer)
	{
		TRACE (("couldn't find xfer"));
		tcp_close (c);
		return;
	}

	/* add the connection to the xfer data structures */
	gt_transfer_ref (c, chunk, xfer);

	source_status_set (chunk->source, SOURCE_WAITING, "Received GIV response");

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback) gt_http_client_start, TIMEOUT_DEF);
}

/*****************************************************************************/

GT_METHOD (server_get)
{
	TRACE_FUNC ();

	/* dispatch to the HTTP server code */
	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) gt_get_client_request, TIMEOUT_DEF);
}

/*****************************************************************************/
/* GNUTELLA/0.4 CONNECTIONS */

static void accept_04 (int fd, input_id id, Connection *c)
{
	char *msg = "GNUTELLA OK\n\n";

	TRACE (("got an 0.4 connection <sob>"));

	if (net_send (c->fd, msg, strlen (msg)) <= 0);
	{
		TRACE (("net_send: error %s", GIFT_STRERROR ()));
		TRACE (("net_send: net error %s", platform_net_error ()));
		gt_node_disconnect (c);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback) gnutella_start_connection, TIMEOUT_DEF);
}

/*****************************************************************************/
/* GNUTELLA/0.6 CONNECTIONS */

/* Ack, duplicated with http_client.c */
void http_headers_parse (char *headers, Dataset **d)
{
	char *line, *key;

	while ((line = string_sep_set (&headers, "\r\n")))
	{
		key = string_sep (&line, ":");

		if (!key || !line)
			continue;

		/* I _think_ spaces after the field-separating colon are optional */
		string_trim (line);

		/* dont allow empty key-values, need to check this too */
		if (string_isempty (line))
			continue;

		dataset_insertstr (d, string_lower (key), line);
	}
}

int print_key (Dataset *d, DatasetNode *node, void *udata)
{
	char *key   = node->key;
	char *value = node->value;

	TRACE (("read [%s] : [%s]", key, value));

	return FALSE;
}

static void accept_06 (int fd, input_id id, Connection *c)
{
	FDBuf *buf;
	char  *response;
	int    n;

	TRACE_FUNC ();

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		TRACE (("error on recv: error: %s", GIFT_STRERROR ()));
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* store the http headers in the node capabilities */
	http_headers_parse (response, &NODE(c)->cap);

	/* debug */
	dataset_foreach (NODE(c)->cap, DATASET_FOREACH (print_key), NULL);

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback) send_06_headers, TIMEOUT_DEF);
}

static void send_06_headers (int fd, input_id id, TCPC *c)
{
	if (net_sock_error (c->fd))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	if (!gnutella_send_connection_headers (c))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) recv_06_final_handshake, TIMEOUT_DEF);
}

static void recv_06_final_handshake (int fd, input_id id, Connection *c)
{
	FDBuf *buf;
	int    n;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		TRACE (("fdbuf_delim: error %s", GIFT_STRERROR ()));
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	/* TODO: parse response here and check for 200-series */
	fdbuf_release (buf);

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback) gnutella_start_connection, TIMEOUT_DEF);
}
