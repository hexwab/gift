/*
 * $Id: gt_connect.c,v 1.55 2005/01/04 15:03:40 mkern Exp $
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
#include "gt_version.h"

#include "gt_connect.h"
#include "gt_accept.h"
#include "gt_packet.h"

#include "gt_node.h"
#include "gt_node_list.h"
#include "gt_utils.h"

#include "gt_search.h"
#include "gt_netorg.h"

#include "gt_node_cache.h"

#include "message/gt_message.h"   /* gnutella_start_connection */

/*****************************************************************************/

static void send_connect  (int fd, input_id id, TCPC *c);
static void recv_headers  (int fd, input_id id, TCPC *c);
static void send_response (int fd, input_id id, TCPC *c);
static BOOL send_final    (TCPC *c);

/*****************************************************************************/

static BOOL handshake_timeout (TCPC *c)
{
	GtNode *node = GT_NODE(c);

	node->handshake_timer = 0;

	if (!(node->state & GT_NODE_CONNECTED))
	{
		gt_node_disconnect (c);
		return FALSE;
	}

	return FALSE;
}

void gnutella_set_handshake_timeout (TCPC *c, time_t delay)
{
	timer_remove (GT_NODE(c)->handshake_timer);

	GT_NODE(c)->handshake_timer = timer_add (delay,
	                                         (TimerCallback)handshake_timeout, 
	                                         c);
}

int gt_connect (GtNode *node)
{
	TCPC   *c;

	if (!node)
		return -1;

#if 0
	if (GT_CONN(node) != NULL)
	{
		GT->dbg (GT, "duplicate connection?: %p", GT_CONN(node));
		return -1;
	}

	if (node->state != GT_NODE_DISCONNECTED)
	{
		GT->dbg (GT, "state = %i??", node->state);
		return -1;
	}
#endif

	/* this must be called only on disconnected nodes */
	assert (GT_CONN(node) == NULL);
	assert (node->state == GT_NODE_DISCONNECTED);

#if 0
	if (!conn_auth (c, TRUE))
		return -1;
#endif

	/* set this early: gt_netorg relies on this being set in order
	 * to check if it should access the gwebcaches */
	node->start_connect_time = time (NULL);

	/* make sure port is valid */
	if (node->gt_port == 0)
	{
		GT->DBGFN (GT, "bad port on node %s", net_ip_str (node->ip));
		return -1;
	}

	/* make outgoing connection */
	if (!(c = tcp_open (node->ip, node->gt_port, FALSE)))
		return -1;

	gt_node_connect (node, c);

	gt_node_state_set (node, GT_NODE_CONNECTING_1);
	node->incoming = FALSE;

	/* set the connection timeout */
	gnutella_set_handshake_timeout (c, TIMEOUT_1 * SECONDS);

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)send_connect, 0);

	return c->fd;
}

static void send_connect (int fd, input_id id, TCPC *c)
{
	if (net_sock_error (c->fd))
	{
		gt_node_disconnect (c);
		return;
	}

	/* Send the connect string along with our headers */
	if (!gnutella_send_connection_headers (c, "GNUTELLA CONNECT/0.6"))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	/* we connected ok, so give the peer some more time */
	gnutella_set_handshake_timeout (c, TIMEOUT_2 * SECONDS);

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)recv_headers, 0);
}

BOOL gnutella_parse_response_headers (char *reply, Dataset **r_headers)
{
	int       code;    /* 200, 404, ... */
	char     *response;
	Dataset  *headers   = NULL;

	response = string_sep (&reply, "\r\n");

	if (!response)
		return FALSE;

	/*    */     string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the headers */
	gt_http_header_parse (reply, &headers);

	if (r_headers)
		*r_headers = headers;
	else
		dataset_clear (headers);

	if (code >= 200 && code <= 299)
		return TRUE;

	return FALSE;
}

static time_t parse_uptime (Dataset *d)
{
	char   *str;
	int     days, hours, mins;
	int     n;

	if (!(str = dataset_lookupstr (d, "uptime")))
		return 0;
	
	string_lower (str);

	if ((n = sscanf (str, "%dd %dh %dm", &days, &hours, &mins)) != 3)
		return 0;

	if (HANDSHAKE_DEBUG)
	{
		GT->dbg (GT, "uptime parsed: %d days, %d hours, %d minutes",
		         days, hours, mins);
	}

	return days * EDAYS + hours * EHOURS + mins * EMINUTES;
}

/* look in a header field for nodes, and register them */
static void extract_nodes (Dataset *d, in_addr_t src,
                           const char *field, gt_node_class_t klass)
{
	char   *str;
	char   *value;
	time_t  now;

	now = time (NULL);

	if (!(str = dataset_lookupstr (d, field)))
		return;

	while ((value = string_sep (&str, ",")))
	{
		in_addr_t  ip;
		in_port_t  port;

		ip   = net_ip (string_sep (&value, ":"));
		port = ATOI   (value);

		if (port == (in_port_t) -1 || port == 0)
			continue;

		if (ip == INADDR_NONE || ip == 0)
			continue;

		if (gt_is_local_ip (ip, src))
			continue;

		gt_node_cache_add_ipv4 (ip, port, klass, now, 0, src);
	}

	gt_node_cache_trace ();
}

static void recv_headers (int fd, input_id id, TCPC *c)
{
	FDBuf   *buf;
	char    *response;
	size_t   response_len = 0;
	int      n;
	BOOL     ok;
	time_t   uptime;
	GtNode  *node         = GT_NODE(c);

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		GT->DBGFN (GT, "error reading headers: %s", GIFT_NETERROR ());
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
		GT->DBGSOCK (GT, c, "node handshake response:\n%s", response);

	/* parse and store the response */
	ok = gnutella_parse_response_headers (response, &node->hdr);

	/* extract nodes */
	extract_nodes (node->hdr, node->ip, "x-try-ultrapeers", GT_NODE_ULTRA);
	extract_nodes (node->hdr, node->ip, "x-try", GT_NODE_NONE);

	/* grab the uptime from the "Uptime: " header and update this node */
	if ((uptime = parse_uptime (node->hdr)) > 0)
	{
		gt_node_cache_add_ipv4 (node->ip, node->gt_port,
		                        GT_NODE_ULTRA, time (NULL), uptime, node->ip);

		/* XXX: remove the item immediately so we trigger the side effect of
		 * adding this node to the stable list */
		gt_node_cache_del_ipv4 (node->ip, node->gt_port);
	}

	if (!ok)
	{
		gt_node_disconnect (c);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)send_response, 0);
}

static void send_response (int fd, input_id id, TCPC *c)
{
	if (net_sock_error (c->fd))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	if (!gnutella_auth_connection (c))
	{
		gt_node_error (c, "[outgoing] connection not authorized");
		gt_node_disconnect (c);
		return;
	}

	if (!send_final (c))
	{
		gt_node_error (c, NULL);
		GT->DBGSOCK (GT, c, "error at stage 3 of handshake");
		gt_node_disconnect (c);
		return;
	}

	/* ok, startup this connection */
	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)gnutella_start_connection, 0);
}

/*****************************************************************************/

static GtNode *append_node (TCPC *c, GtNode *node, String *s)
{
	if (s->str[s->len - 1] != ' ')
		string_append (s, ",");

	string_appendf (s, "%s:%hu", net_ip_str (node->ip), node->gt_port);
	return NULL;
}

static void append_crawler_headers (String *msg)
{
	if (gt_conn_length (GT_NODE_ULTRA, GT_NODE_CONNECTED) > 0)
	{
		string_append (msg, "Peers: ");
		gt_conn_foreach (GT_CONN_FOREACH(append_node), msg,
		                 GT_NODE_ULTRA, GT_NODE_CONNECTED, 0);
		string_append (msg, "\r\n");
	}

	if (GT_SELF->klass & GT_NODE_ULTRA && 
	    gt_conn_length (GT_NODE_LEAF, GT_NODE_CONNECTED) > 0)
	{
		string_append (msg, "Leaves: ");
		gt_conn_foreach (GT_CONN_FOREACH(append_node), msg,
		                 GT_NODE_LEAF, GT_NODE_CONNECTED, 0);
		string_append (msg, "\r\n");
	}
}

BOOL gnutella_send_connection_headers (TCPC *c, const char *header)
{
	String *msg;

	if (!(msg = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	string_appendf (msg, "%s\r\n", header);

	string_append  (msg, "X-Query-Routing: 0.1\r\n");
	string_appendf (msg, "X-Ultrapeer: %s\r\n", 
	                (GT_SELF->klass & GT_NODE_ULTRA) ? "True" : "False");

	/* append the client and version we are using */
	string_appendf (msg, "User-Agent: %s\r\n", gt_version ());

	/* Add a header describing the remote IP of the peer */
	string_appendf (msg, "Remote-IP: %s\r\n", net_peer_ip (c->fd));

	/* let remote end know it's ok to send vendor messages */
	string_appendf (msg, "Vendor-Message: 0.1\r\n");

	/* support transmission of pings/pongs with GGEP appended */
	string_append  (msg, "GGEP: 0.5\r\n");

	/* If this is the limewire crawler, append "Peers: " and "Leaves: "
	 * headers and close the connection */
	if (!c->outgoing && dataset_lookupstr (GT_NODE(c)->hdr, "crawler"))
		append_crawler_headers (msg);

	/* append willingness to receive compressed data */
	string_append (msg, "Accept-Encoding: deflate\r\n");
		
	/* check whether the remote node sent us Accept-Encoding: deflate
	 * already */
	gnutella_mark_compression (GT_NODE(c));

	/* compress data if we must */
	if (GT_NODE(c)->tx_deflated)
		string_append (msg, "Content-Encoding: deflate\r\n");

	/* Add message terminator */
	string_append (msg, "\r\n");

	if (HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "sending node headers:\n%s", msg->str);

	if (tcp_send (c, msg->str, msg->len) <= 0)
	{
		string_free (msg);
		return FALSE;
	}

	string_free (msg);
	return TRUE;
}

static BOOL send_final (TCPC *c)
{
	String *s;
	int     ret;
	int     len;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	/* append header acceptance line */
	string_append (s, "GNUTELLA/0.6 200 OK\r\n");

	/* mark the connection as complete */
	gnutella_mark_compression (GT_NODE(c));

	if (GT_NODE(c)->tx_deflated)
		string_append (s, "Content-Encoding: deflate\r\n");

	/* append msg terminator */
	string_append (s, "\r\n");

	if (HANDSHAKE_DEBUG)
		GT->DBGSOCK (GT, c, "sending final handshake:\n%s", s->str);

	len = s->len;
	ret = tcp_send (c, s->str, s->len);

	string_free (s);

	if (ret != len)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/
/* CONNECTABILITY TESTING */

static void connect_test_result (GtNode *node, TCPC *c, BOOL success)
{
	GT->DBGFN (GT, "connect test to %s %s", net_ip_str (node->ip),
	           (success ? "succeeded" : "failed"));

	node->firewalled = (success ? FALSE : TRUE);
	node->verified   = TRUE;

	if (c)
	{
		tcp_close (c);
		node->gt_port_verify = NULL;
	}
}

static void test_connectable (int fd, input_id id, TCPC *c)
{
	GtNode *node;

	node = c->udata;

	if (net_sock_error (c->fd))
	{
		connect_test_result (node, c, FALSE);
		return;
	}

	/*
	 * Send two newlines, because some firewalls will let connections pass 
	 * through, but no data.
	 */
	tcp_send (c, "\n\n", 2);
	connect_test_result (node, c, TRUE);
}

/*
 * Test if the port of a peer we are connected to is connectable.  This lets a
 * node know if it's firewalled.  We could use this info to mangle the 'push'
 * flag on query hits from this node if it is a leaf.
 *
 * Mangling query hits would break any future checksum or signing algorithm on
 * query hits though, so that isn't done.
 */
void gt_connect_test (GtNode *node, in_port_t port)
{
	TCPC *new_c;

	if (!port)
	{
		node->firewalled = TRUE;
		return;
	}

	/* this needs some kind of local mode switch */
#if 0
	if (net_match_host (GT_NODE(c)->ip, "LOCAL"))
	{
		GT_NODE(c)->firewalled = TRUE;
		return;
	}
#endif

	if (!node->incoming)
		return;

	GT->DBGFN (GT, "starting connect test on %s:%hu", 
	           net_ip_str (node->ip), port);

	if (!(new_c = tcp_open (node->ip, port, FALSE)))
	{
		GT->DBGFN (GT, "failed to open test connection to %s:%hu",
		           net_ip_str (node->ip), node->gt_port);
		return;
	}

	if (node->gt_port_verify)
		tcp_close (node->gt_port_verify);

	/* keep track of this connection */
	node->gt_port_verify = new_c;
	new_c->udata = node;

	input_add (new_c->fd, new_c, INPUT_WRITE,
	           (InputCallback)test_connectable, TIMEOUT_DEF);
}
