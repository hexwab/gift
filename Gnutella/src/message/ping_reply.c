/*
 * $Id: ping_reply.c,v 1.7 2005/01/04 15:00:52 mkern Exp $
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
#include "message/msg_handler.h"

#include "gt_node_cache.h"
#include "gt_connect.h"
#include "gt_bind.h"

#include "gt_search.h"
#include "gt_share_state.h"
#include "gt_query_route.h"
#include "gt_stats.h"

/*****************************************************************************/

extern BOOL gt_is_pow2 (uint32_t num);   /* ping.c */

/*****************************************************************************/

/*
 * Update the port on the first pong or when the pong contains a different
 * pong.
 */
static void update_port (GtNode *node, in_port_t new_port)
{
	/* update the port */
	node->gt_port = new_port;

	/*
	 * Test if the node is connectable.  This will play with the node's
	 * ->verified and ->firewalled bits.
	 *
	 * This is only important if this node is running as an ultrapeer, because
	 * it lets us know whether we should route queries from firewalled peers
	 * to the remote node.
	 */
	if (GT_SELF->klass & GT_NODE_ULTRA)
		gt_connect_test (node, node->gt_port);
}

/*
 * Transition the node into state 'connected', and do various things.  This
 * has become a bit crufty and miscellaneous.  TODO: change this to a callback
 * registration system in gt_node.c, register the callbacks in gt_gnutella.c
 */
static BOOL complete_connection (GtNode *node)
{
	/* mark this node as now connected */
	gt_node_state_set (node, GT_NODE_CONNECTED);

	/* submit the routing table */
	if ((node->klass & GT_NODE_ULTRA) &&
	    !(GT_SELF->klass & GT_NODE_ULTRA))
	{
		query_route_table_submit (GT_CONN(node));
	}

	/* submit unfinished searches soon */
	gt_searches_submit (GT_CONN(node), 30 * SECONDS);

	/* let the bind subsystem send a ConnectBack for tracking firewalled
	 * status */
	gt_bind_completed_connection (node);

	if (!(node->share_state = gt_share_state_new ()))
		return FALSE;

	gt_share_state_update (node);

	return TRUE;
}

GT_MSG_HANDLER(gt_msg_ping_reply)
{
	in_port_t       port;
	in_addr_t       ip;
	uint32_t        files;
	uint32_t        size_kb;
	gt_node_class_t klass;

	port    = gt_packet_get_port   (packet);
	ip      = gt_packet_get_ip     (packet);
	files   = gt_packet_get_uint32 (packet);
	size_kb = gt_packet_get_uint32 (packet);

	/* this will keep the node from being disconnected by idle-check loop */
	if (node->pings_with_noreply > 0)
		node->pings_with_noreply = 0;

	/* update stats and port */
	if (gt_packet_ttl (packet) == 1 && gt_packet_hops (packet) == 0)
	{
		/* check if this is the first ping response on this connection */
		if (node->state == GT_NODE_CONNECTING_2)
		{
			if (!complete_connection (node))
			{
				gt_node_disconnect (c);
				return;
			}
		}

		if (ip == node->ip)
		{
			if (node->gt_port != port || !node->verified)
				update_port (node, port);

			/* update stats information */
			node->size_kb = size_kb;
			node->files   = files;

			/* don't add this node to the cache */
			return;
		}

		/*
		 * Morpheus nodes send pongs for other nodes with Hops=1.  If
		 * the IP doesn't equal the observed IP, then add the node to the
		 * node cache.  This may create problems with trying to connect twice
		 * to some users, though.
		 */
	}

	/* add this node to the cache */
	klass = GT_NODE_LEAF;

	/* LimeWire marks ultrapeer pongs by making files size a power of two */
	if (size_kb >= 8 && gt_is_pow2 (size_kb))
		klass = GT_NODE_ULTRA;

	/* don't register this node if its local and the peer isnt */
	if (gt_is_local_ip (ip, node->ip))
		return;

	/* keep track of stats from pongs */
	gt_stats_accumulate (ip, port, node->ip, files, size_kb);

	/* TODO: check uptime GGEP extension and add it here */
	gt_node_cache_add_ipv4 (ip, port, klass, time (NULL), 0, node->ip);
	gt_node_cache_trace ();
}
