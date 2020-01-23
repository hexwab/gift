/*
 * $Id: gt_message.c,v 1.6 2004/01/07 07:24:43 hipnod Exp $
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
#include "msg_handler.h"

#include "gt_netorg.h"
#include "gt_connect.h"

#include "gt_utils.h"

#include "io/rx_stack.h"               /* gt_rx_stack_new */
#include "io/tx_stack.h"               /* gt_tx_stack_new */

#include "gt_message.h"

/*****************************************************************************/

extern void gt_vmsg_send_supported (GtNode *node);   /* vendor.c */

/*****************************************************************************/

extern GT_MSG_HANDLER(gt_msg_ping);
extern GT_MSG_HANDLER(gt_msg_ping_reply);
extern GT_MSG_HANDLER(gt_msg_bye);
extern GT_MSG_HANDLER(gt_msg_push);
extern GT_MSG_HANDLER(gt_msg_query_route);
extern GT_MSG_HANDLER(gt_msg_query);
extern GT_MSG_HANDLER(gt_msg_query_reply);
extern GT_MSG_HANDLER(gt_msg_vendor);

static struct msg_handler
{
	uint8_t command;
	GtMessageHandler func;
}
msg_handler_table[] =
{
	/* table listed not in numerical order, but by frequency of messages */
	{ GT_MSG_QUERY,        gt_msg_query       },
	{ GT_MSG_QUERY_REPLY,  gt_msg_query_reply },
	{ GT_MSG_PING_REPLY,   gt_msg_ping_reply  },
	{ GT_MSG_PING,         gt_msg_ping        },
	{ GT_MSG_PUSH,         gt_msg_push        },
	{ GT_MSG_QUERY_ROUTE,  gt_msg_query_route },
	{ GT_MSG_VENDOR,       gt_msg_vendor      },
	{ GT_MSG_VENDOR_STD,   gt_msg_vendor      }, /* same as non-standard */
	{ GT_MSG_BYE,          gt_msg_bye         },
	{ 0x00,                NULL               }
};

/*****************************************************************************/

static BOOL handle_message (TCPC *c, GtPacket *packet)
{
	struct msg_handler *handler;
	uint8_t command;

	if (!packet)
		return FALSE;

	command = gt_packet_command (packet);

	/* locate the handler */
	for (handler = msg_handler_table; handler->func; handler++)
	{
		if (command == handler->command)
		{
			handler->func (GT_NODE(c), c, packet);
			return TRUE;
		}
	}

	GIFT_ERROR (("[%s] found no handler for cmd %hx, payload %hx",
				 net_ip_str (GT_NODE(c)->ip), command,
				 gt_packet_payload_len (packet)));

	return FALSE;
}

static void cleanup_node_rx (GtNode *node)
{
	TCPC    *c = GT_CONN(node);

	assert (GT_NODE(c) == node);
	gt_node_disconnect (c);
}

/* TODO: make this the same type as cleanup_node_rx */
static void cleanup_node_tx (GtTxStack *stack, GtNode *node)
{
	TCPC *c = GT_CONN(node);

	assert (GT_NODE(c) == node);
	gt_node_disconnect (c);
}

static void recv_packet (GtNode *node, GtPacket *packet)
{
	assert (packet != NULL);

	gt_packet_log (packet, GT_CONN(node), FALSE);
	(void)handle_message (node->c, packet);
}

/* Find out what our IP is */
static in_addr_t get_self_ip (TCPC *c)
{
	in_addr_t our_ip;
	char     *ip_str;

	if ((ip_str = dataset_lookupstr (GT_NODE(c)->hdr, "remote-ip")))
	{
		/*
		 * Since we may be firewalled, we may not know what our ip is.  So set
		 * the ip from what the other node thinks it is.
		 *
		 * Doing this allows you to setup port forwarding on a firewall
		 * and accept incoming connections.
		 */
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

/*
 * Begin a node connection with the peer on the specified TCPC.
 *
 * We arrive here from either an incoming or outgoing connection.
 * This is the entrance point to the main packet-reading loop.
 *
 * After setting up the connection, we send the node a ping.
 * If it doesn't respond after a timeout, we will destroy the
 * connection.
 */
void gnutella_start_connection (int fd, input_id id, TCPC *c)
{
	GtPacket    *ping;
	GtNode      *node;

	node = GT_NODE(c);
	assert (GT_CONN(node) == c);

	/* remove the old input handler first -- need to before sending data */
	input_remove (id);

	if (net_sock_error (c->fd))
	{
		if (HANDSHAKE_DEBUG)
			gt_node_error (c, NULL);

		gt_node_disconnect (c);
		return;
	}

	/* if this is the crawler, disconnect */
	if (dataset_lookupstr (GT_NODE(c)->hdr, "crawler"))
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "closing crawler connection");

		gt_node_disconnect (c);
		return;
	}

	if (!(node->rx_stack = gt_rx_stack_new (node, c, node->rx_inflated)))
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "error allocating rx_stack");

		gt_node_disconnect (c);
		return;
	}

	if (!(node->tx_stack = gt_tx_stack_new (c, node->tx_deflated)))
	{
		if (HANDSHAKE_DEBUG)
			GT->DBGSOCK (GT, c, "error allocating tx stack");

		gt_node_disconnect (c);
		return;
	}

	/* determine the other node's opinion of our IP address */
	node->my_ip = get_self_ip (c);

	/* determine the other ends port */
	peer_addr (c->fd, NULL, &node->peer_port);

	if (HANDSHAKE_DEBUG)
	{
		GT->DBGSOCK (GT, c, "self IP=[%s]", net_ip_str (node->my_ip));
		GT->DBGSOCK (GT, c, "peer port=%hu", node->peer_port);
	}

	if (!(ping = gt_packet_new (GT_MSG_PING, 1, NULL)))
	{
		gt_node_disconnect (c);
		return;
	}

	/* set the state as intermediately connecting and mark the node connected
	 * only when it replies to our ping */
	gt_node_state_set (node, GT_NODE_CONNECTING_2);

	/* give the connection some more time */
	gnutella_set_handshake_timeout (c, TIMEOUT_3 * SECONDS);

	/*
	 * Setup our packet handlers, for both receiving and sending packets.
	 */
	gt_rx_stack_set_handler (node->rx_stack,
	                         (GtRxStackHandler)recv_packet,
	                         (GtRxStackCleanup)cleanup_node_rx,
	                         node);

	gt_tx_stack_set_handler (node->tx_stack,
	                         (GtTxStackCleanup)cleanup_node_tx,
	                         node);

	/* send first ping */
	gt_packet_send (c, ping);
	gt_packet_free (ping);

	/* send MessagesSupported Vendor message, if this node supports it */
	gt_vmsg_send_supported (node);
}
