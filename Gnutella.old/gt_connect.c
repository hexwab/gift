/*
 * $Id: gt_connect.c,v 1.23 2003/07/01 16:08:22 hipnod Exp $
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

#include "gt_connect.h"
#include "gt_accept.h"

#include "gt_protocol.h"
#include "gt_packet.h"

#include "gt_node.h"
#include "gt_utils.h"

#include "gt_search.h"
#include "gt_netorg.h"

/*****************************************************************************/

static void send_connect  (int fd, input_id id, TCPC *c);
static void recv_headers  (int fd, input_id id, TCPC *c);
static void send_response (int fd, input_id id, TCPC *c);
static BOOL send_final    (TCPC *c);

/*****************************************************************************/

static int handshake_timeout (TCPC *c)
{
	GT_NODE(c)->handshake_timer = 0;

	if (!(GT_NODE(c)->state & GT_NODE_CONNECTED))
	{
		gt_node_disconnect (c);
		return FALSE;
	}

	return FALSE;
}

static void reset_connection_timer (TCPC *c, time_t delay)
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

#if 0
	if (!conn_auth (c, TRUE))
		return -1;
#endif

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

	node->start_connect_time = time (NULL);

	gt_node_state_set (node, GT_NODE_CONNECTING_1);
	node->incoming = FALSE;

	/* set the connection timeout */
	reset_connection_timer (c, 4 * SECONDS);

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
	reset_connection_timer (c, 10 * SECONDS);

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)recv_headers, 0);
}

int gnutella_parse_response_headers (char *reply, Dataset **r_headers)
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
	http_headers_parse (reply, &headers);

	if (r_headers)
		*r_headers = headers;
	else
		dataset_clear (headers);

	if (code >= 200 && code <= 299)
		return TRUE;

	return FALSE;
}

/* look in a header field for nodes, and register them */
static void extract_nodes (Dataset *d, in_addr_t src,
                           char *field, GtNodeClass klass)
{
	char *str;
	char *value;
#if 0
	int   nodes = 0;
#endif

	if (!(str = dataset_lookupstr (d, field)))
		return;

	while ((value = string_sep (&str, ",")))
	{
		in_addr_t   ip;
		in_port_t  port;

		/* dont register any more nodes if there isnt room */
		if (gt_nodes_full (NULL))
			break;

		ip   = net_ip (string_sep (&value, ":"));
		port = ATOI   (value);

		if (port == (in_port_t) -1 || ip == (in_addr_t) -1)
			continue;

		if (gt_is_local_ip (ip, src))
			continue;

		gt_node_register (ip, port, klass, 0, 0);

#if 0
		/* don't register too many nodes from a single node */
		if (++nodes > 4)
			break;
#endif
	}
}

static void recv_headers (int fd, input_id id, TCPC *c)
{
	FDBuf   *buf;
	char    *response;
	size_t   response_len = 0;
	int      n;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		GT->DBGFN (GT, "error reading headers: %s", GIFT_NETERROR ());
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);
	if (!http_headers_terminated (response, response_len))
		return;

	fdbuf_release (buf);
	GT->dbgsock (GT, c, "node handshake response:\n%s", response);

	/* parse the response code, and place them in capabilities */
	if (!gnutella_parse_response_headers (response, &GT_NODE(c)->cap))
	{
		extract_nodes (GT_NODE(c)->cap, GT_NODE(c)->ip,
		               "x-try-ultrapeers", GT_NODE_ULTRA);
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
		GT->dbgsock (GT, c, "error at stage 3 of handshake");
		gt_node_disconnect (c);
		return;
	}

	/* ok, startup this connection */
	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback)gnutella_start_connection, 0);
}

/*****************************************************************************/

/* Find out what our IP is */
static in_addr_t get_self_ip (TCPC *c)
{
	in_addr_t our_ip;
	char     *ip_str;

	if ((ip_str = dataset_lookupstr (GT_NODE(c)->cap, "remote-ip")))
	{
		/* Since we may be firewalled, we may not know what our ip is.
	 	 * So set the ip from what the other node thinks it is.
		 *
		 * Doing this allows you to setup port forwarding on a firewall
		 * and accept incoming connections. */
		our_ip = net_ip (ip_str);
	}
	else
	{
		struct sockaddr_in saddr;
		int    len = sizeof (saddr);

		if (getsockname (c->fd, (struct sockaddr *)&saddr, &len) == 0)
			our_ip = saddr.sin_addr.s_addr;
		else
			our_ip = net_ip ("127.0.0.1");
	}

	return our_ip;
}

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
		gt_conn_foreach ((GtConnForeachFunc)append_node, msg,
		                 GT_NODE_ULTRA, GT_NODE_CONNECTED, 0);
		string_append (msg, "\r\n");
	}

	if (GT_SELF->klass & GT_NODE_ULTRA && 
	    gt_conn_length (GT_NODE_LEAF, GT_NODE_CONNECTED) > 0)
	{
		string_append (msg, "Leaves: ");
		gt_conn_foreach ((GtConnForeachFunc)append_node, msg,
		                 GT_NODE_LEAF, GT_NODE_CONNECTED, 0);
		string_append (msg, "\r\n");
	}
}

int gnutella_send_connection_headers (TCPC *c, char *header)
{
	String *msg;

	if (!(msg = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	string_appendf (msg, "%s\r\n", header);

	string_append  (msg, "X-Query-Routing: 0.1\r\n");
	string_appendf (msg, "X-Ultrapeer: %s\r\n", 
	                (gt_self->klass & GT_NODE_ULTRA) ? "True" : "False");

	/* append the client and version we are using */
	string_appendf (msg, "User-Agent: %s\r\n", gt_version ());

	/* Add a header describing the remote IP of the peer */
	string_appendf (msg, "Remote-IP: %s\r\n", net_peer_ip (c->fd));

	/* If this is the limewire crawler, append "Peers: " and "Leaves: "
	 * headers and close the connection */
	if (!c->outgoing && dataset_lookupstr (GT_NODE(c)->cap, "crawler"))
		append_crawler_headers (msg);

	/* Add message terminator */
	string_append (msg, "\r\n");
	GT->dbgsock (GT, c, "sending node headers:\n%s", msg->str);

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

	/* append msg terminator */
	string_append (s, "\r\n");
	GT->DBGSOCK (GT, c, "sending final handshake:\n%s", s->str);

	len = s->len;
	ret = tcp_send (c, s->str, s->len);

	string_free (s);

	if (ret != len)
		return FALSE;

	return TRUE;
}

void gnutella_start_connection (int fd, input_id id, TCPC *c)
{
	GtPacket *ping;

	/* remove the old input handler first -- need to before sending data */
	input_remove (id);

	if (net_sock_error (c->fd))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	/* If this is the crawler, disconnect */
	if (dataset_lookupstr (GT_NODE(c)->cap, "crawler"))
	{
		GT->dbg (GT, "closing crawler connection");
		gt_node_disconnect (c);
		return;
	}

	if (!(ping = gt_packet_new (GT_PING_REQUEST, 1, NULL)))
	{
		gt_node_disconnect (c);
		return;
	}

	GT_NODE(c)->my_ip = get_self_ip (c);
	GT->DBGFN (GT, "self IP=[%s]", net_ip_str (GT_NODE(c)->my_ip));

	/* determine the other ends port */
	peer_addr (c->fd, NULL, &GT_NODE(c)->peer_port);
	GT->DBGFN (GT, "peer port=%hu", GT_NODE(c)->peer_port);

	/* save bandwidth by disconnecting from non-ultrapeer nodes
	 * if we're not an ultrapeer and we have other ultrapeer connections */
	if (!(GT_SELF->klass & GT_NODE_ULTRA) &&
	    !(GT_NODE(c)->klass & GT_NODE_ULTRA) &&
	    gt_conn_length (GT_NODE_ULTRA, GT_NODE_CONNECTED) > 0)
	{
		gt_packet_free (ping);
		gt_node_error (c, "we want ultrapeers");
		gt_node_disconnect (c);
		return;
	}

	/*
	 * Set the state as intermediately connecting. The node gets marked
	 * connected if it replies to our ping.
	 */
	gt_node_state_set (GT_NODE(c), GT_NODE_CONNECTING_2);

	/* send first ping */
	gt_packet_send (c, ping);

	/* give the connection some more time */
	reset_connection_timer (c, 10 * SECONDS);

	input_add (fd, c, INPUT_READ,
	           (InputCallback)gnutella_connection, 0);
}

/*****************************************************************************/
/* PORT VERIFICATION */

static void connect_test_result (GtNode *node, TCPC *verify, int success)
{
	if (!success)
	{
		/* this node isnt connectable, so set it as firewalled */
		GT->DBGFN (GT, "setting %s as firewalled", net_ip_str (node->ip));
		node->firewalled = TRUE;
	}
	else
	{
		/* ok, we can connect to this node */
		GT->DBGFN (GT, "%s isnt firewalled", net_ip_str (node->ip));
		node->firewalled = FALSE;
	}

	if (verify)
	{
		tcp_close (verify);
		node->gt_port_verify = NULL;
	}

	node->verified = TRUE;
}

static void gt_handle_verification (int fd, input_id id, TCPC *verify)
{
	GtNode *node;

	node = verify->udata;

	if (net_sock_error (verify->fd))
	{
		connect_test_result (node, verify, FALSE);
		return;
	}

	/*
	 * Send two newlines, because some firewalls will let connections pass 
	 * through, but no data.
	 * 
	 * This works just like the TCP ConnectBack vendor message.
	 */
	tcp_send (verify, "\n\n", 2);
	connect_test_result (node, verify, TRUE);
}

/* Verify the port of a peer we are connected to is connectable
 *
 * Nodes should send a ping response with their port in it
 * when pinged with a TTL of one, so utilize this info
 * to verify their firewalled status.  We could use this
 * info to mangle the 'push' flag on query hits from this
 * node if it is a leaf.
 *
 * NOTE: Mangling query hits would break any future
 * checksum or signing algorithm on query hits that
 * go to an ultranode */
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

	node->gt_port_verify = new_c;

	node->firewalled = FALSE;
	node->verified   = FALSE;

	new_c->udata = node;

	input_add (new_c->fd, new_c, INPUT_WRITE,
	           (InputCallback)gt_handle_verification, TIMEOUT_DEF);
}
