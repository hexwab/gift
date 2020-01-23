/*
 * $Id: gt_connect.c,v 1.11 2003/05/04 08:17:37 hipnod Exp $
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

#define CONNECT_STR          "GNUTELLA CONNECT/0.6\r\n" \
                             "X-Query-Routing: 0.1\r\n" \
                             "User-Agent: giFT-gnutella/" GT_VERSION  "\r\n" \
                             "Bye-Packet: 0.1\r\n" \
                             "X-Ultrapeer: False\r\n" \
                             "\r\n"
#define RESPONSE_STR         "GNUTELLA/0.6 200 OK\r\n" \
                             "\r\n"

/*****************************************************************************/

static void send_connect  (int fd, input_id id, Connection *c);
static void recv_headers  (int fd, input_id id, Connection *c);
static void send_response (int fd, input_id id, Connection *c);

/*****************************************************************************/

static int handshake_timeout (Connection *c)
{
	NODE(c)->handshake_timer = 0;

	if (!(NODE(c)->state & NODE_CONNECTED))
	{
		gt_node_disconnect (c);
		return FALSE;
	}

	return FALSE;
}

static void reset_connection_timer (Connection *c, time_t delay)
{
	timer_remove (NODE(c)->handshake_timer);

	NODE(c)->handshake_timer = timer_add (delay,
	                                      (TimerCallback) handshake_timeout,
	                                      c);
}

int gt_connect (GtNode *node)
{
	TCPC   *c;

	if (GT_CONN(node) != NULL)
	{
		GIFT_DEBUG (("duplicate connection?: %p", GT_CONN(node)));
		return -1;
	}

	if (node->state != NODE_DISCONNECTED)
	{
		GIFT_DEBUG (("state = %i??", node->state));
		return -1;
	}

#if 0
	if (!conn_auth (c, TRUE))
		return -1;
#endif

	/* make sure port is valid */
	if (node->gt_port == 0)
	{
		TRACE (("bad port on node %s", net_ip_str (node->ip)));
		return -1;
	}

	/* make outgoing connection */
	if (!(c = tcp_open (node->ip, node->gt_port, FALSE)))
		return -1;

	gt_node_connect (node, c);

	node->start_connect_time = time (NULL);

	gt_node_state_set (node, NODE_CONNECTING_1);
	node->incoming = FALSE;

	/* set the connection timeout */
	reset_connection_timer (c, 4 * SECONDS);

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback) send_connect, 0);

	return c->fd;
}

static void send_connect (int fd, input_id id, Connection *c)
{
	int len;

	/* TODO: send useful http header fields (User-Agent, Remote-IP, ... */
	if ((len = net_send (c->fd, CONNECT_STR, strlen (CONNECT_STR))) <= 0)
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	assert (len == strlen (CONNECT_STR));

	/* we connected ok, so give the peer some more time */
	reset_connection_timer (c, 10 * SECONDS);

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) recv_headers, 0);
}

static int parse_response_headers (char *reply, Connection *c)
{
	char *response;
	int   code;    /* 200, 404, ... */

	response = string_sep (&reply, "\r\n");

	if (!response)
		return FALSE;

	/*    */     string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the headers */
	http_headers_parse (reply, &NODE(c)->cap);

	if (code >= 200 && code <= 299)
		return TRUE;

	return FALSE;
}

/* look in a header field for nodes, and register them */
static void extract_nodes (Dataset *d, in_addr_t src,
                           char *field, unsigned short klass)
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
		unsigned long  ip;
		unsigned short port;

		/* dont register any more nodes if there isnt room */
		if (gt_nodes_full (NULL))
			break;

		ip   = net_ip (string_sep (&value, ":"));
		port = ATOI   (value);

		if (port == (unsigned short) -1 || ip == (unsigned long) -1)
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
	FDBuf *buf;
	char  *response;
	int    n;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		TRACE (("error reading headers: %s", platform_net_error ()));
		gt_node_disconnect (c);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	TRACE (("[%s] response: %s", net_ip_str (net_peer (c->fd)), response));

	/* parse the response code */
	if (!parse_response_headers (response, c))
	{
		extract_nodes (GT_NODE(c)->cap, GT_NODE(c)->ip,
		               "x-try-ultrapeers", NODE_SEARCH);
#if 0
		extract_nodes (NODE(c)->cap, "x-try", NODE_USER);
#endif
		gt_node_disconnect (c);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback) send_response, 0);
}

static void send_response (int fd, input_id id, TCPC *c)
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

	/* ok, startup this connection */
	input_remove (id);
	input_add (fd, c, INPUT_WRITE,
	           (InputCallback) gnutella_start_connection, 0);
}

/*****************************************************************************/

/* Find out what our IP is */
static unsigned long get_self_ip (Connection *c)
{
	unsigned long our_ip;
	char         *ip_str;

	if ((ip_str = dataset_lookupstr (NODE(c)->cap, "remote-ip")))
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

		if (getsockname (c->fd, (struct sockaddr *) &saddr, &len) == 0)
			our_ip = saddr.sin_addr.s_addr;
		else
			our_ip = net_ip ("127.0.0.1");
	}

	return our_ip;
}

int gnutella_send_connection_headers (TCPC *c)
{
	String *msg;

	if (!(msg = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	string_append (msg, "GNUTELLA/0.6 200 OK\r\n");

	string_append  (msg, "X-Query-Routing: 0.1\r\n");
	string_appendf (msg, "X-Ultrapeer: %s\r\n", 
	                (gt_self->klass & NODE_SEARCH) ? "True" : "False");

	/* append the client and version we are using */
	string_appendf (msg, "User-Agent: %s\r\n", gt_version ());

	/* Add a header describing the remote IP of the peer */
	string_appendf (msg, "Remote-IP: %s\r\n", net_peer_ip (c->fd));

	/* Add message terminator */
	string_append (msg, "\r\n");

	if (tcp_send (c, msg->str, msg->len) <= 0)
	{
		string_free (msg);
		return FALSE;
	}

	string_free (msg);
	return TRUE;
}

void gnutella_start_connection (int fd, input_id id, Connection *c)
{
	Gt_Packet *ping;
	char      *ultrapeer;
	char      *qrp;

	/* remove the old input handler first -- need to before sending data */
	input_remove (id);

	if (net_sock_error (c->fd))
	{
		gt_node_error (c, NULL);
		gt_node_disconnect (c);
		return;
	}

	if (!(ping = gt_packet_new (GT_PING_REQUEST, 1, NULL)))
	{
		gt_node_disconnect (c);
		return;
	}

	NODE(c)->my_ip = get_self_ip (c);
	TRACE (("self ip = %s", net_ip_str (NODE(c)->my_ip)));

	/* determine the other ends port */
	peer_addr (c->fd, NULL, &NODE(c)->peer_port);
	TRACE (("peer port = %hu", NODE(c)->peer_port));

	ultrapeer = dataset_lookupstr (NODE(c)->cap, "x-ultrapeer");
	qrp       = dataset_lookupstr (NODE(c)->cap, "x-query-routing");

	if (ultrapeer && !strcasecmp (ultrapeer, "true") && qrp != NULL)
		gt_node_class_set (NODE(c), NODE_SEARCH);
	else
		gt_node_class_set (NODE(c), NODE_USER);

	/* save bandwidth by disconnecting from non-ultrapeer nodes
	 * if we're not an ultrapeer and we have other ultrapeer connections */
	if (!(gt_self->klass & NODE_SEARCH) &&
	    !(NODE(c)->klass & NODE_SEARCH) &&
	    gt_conn_length (NODE_SEARCH, NODE_CONNECTED) > 0)
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
	gt_node_state_set (NODE(c), NODE_CONNECTING_2);

	/* send first ping */
	gt_packet_send (c, ping);

	/* give the connection some more time */
	reset_connection_timer (c, 10 * SECONDS);

	input_add (fd, c, INPUT_READ,
	           (InputCallback) gnutella_connection, 0);
}

/*****************************************************************************/
/* PORT VERIFICATION */

static void connect_test_result (GtNode *node, TCPC *verify, int success)
{
	if (!success)
	{
		/* this node isnt connectable, so set it as firewalled */
		TRACE (("setting %s as firewalled", net_ip_str (node->ip)));
		node->firewalled = TRUE;
	}
	else
	{
		/* ok, we can connect to this node */
		TRACE (("%s isnt firewalled", net_ip_str (node->ip)));
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
void gt_connect_test (GtNode *node, unsigned short port)
{
	TCPC *new_c;

	if (!port)
	{
		node->firewalled = TRUE;
		return;
	}

	/* this needs some kind of local mode switch */
#if 0
	if (net_match_host (NODE(c)->ip, "LOCAL"))
	{
		NODE(c)->firewalled = TRUE;
		return;
	}
#endif

	if (!node->incoming)
		return;

	TRACE (("starting connect test on %s:%hu", net_ip_str (node->ip), port));

	if (!(new_c = tcp_open (node->ip, port, FALSE)))
	{
		TRACE (("failed open test connection"));
		return;
	}

	if (node->gt_port_verify)
		tcp_close (node->gt_port_verify);

	node->gt_port_verify = new_c;

	node->firewalled = FALSE;
	node->verified   = FALSE;

	new_c->udata = node;

	input_add (new_c->fd, new_c, INPUT_WRITE,
	           (InputCallback) gt_handle_verification, TIMEOUT_DEF);
}
