/*
 * $Id: gt_netorg.c,v 1.46 2004/03/24 06:34:52 hipnod Exp $
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

#include "gt_node.h"
#include "gt_node_list.h"
#include "gt_netorg.h"

#include "gt_connect.h"
#include "gt_accept.h"

#include "gt_packet.h"

#include "gt_node_cache.h"
#include "gt_web_cache.h"

/*****************************************************************************/

/* how often we check the network's condition */
#define MAINTAIN_INTERVAL         (10 * SECONDS)

/* how often to trim the node list */
#define CLEANUP_INTERVAL          (15 * MINUTES)

/* how often to clear indications of connecting to nodes */
#define RETRY_ALL_INTERVAL        (60 * MINUTES)

/* how many connections attempts each maintain loop for nodes previously
 * registered */
#define TRY_CONNECT_NODE_LIST     gt_config_get_int("connect/node_list=3")

/* how many connection attempts for nodes in the pong cache */
#define TRY_CONNECT_NODE_CACHE    gt_config_get_int("connect/node_cache=7")

/*****************************************************************************/

/* timer for initiating/closing connections */
static timer_id   maintain_timer;

/* timer for disconnecting connections */
static timer_id   disconnect_timer;

/* timer for cleaning up the node list */
static timer_id   cleanup_timer;

/* timer to clear 'tried' indicators to retry connecting */
static timer_id   retry_all_timer;

/*****************************************************************************/

static GtNode *node_disconnect_one (TCPC *c, GtNode *node, void *udata)
{
	GT->DBGFN (GT, "[%s]: disconnecting", net_ip_str (GT_NODE(c)->ip));
	gt_node_disconnect (c);
	return NULL;
}

static GtNode *node_ping (TCPC *c, GtNode *node, GtPacket *packet)
{
	gt_packet_send (c, packet);
	return NULL;
}

static void ping_hosts_ttl (uint8_t ttl)
{
	GtPacket  *packet;

	if (!(packet = gt_packet_new (GT_MSG_PING, ttl, NULL)))
		return;

	gt_conn_foreach (GT_CONN_FOREACH(node_ping), packet,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);

	gt_packet_free (packet);
}

static void ping_hosts (time_t now)
{
	static time_t last_ping;
	static time_t last_keep_alive;
	BOOL          need_connections;
	uint8_t       ttl;

	need_connections = gt_conn_need_connections (GT_NODE_ULTRA);

	if (now - last_ping < 30 * SECONDS && !need_connections)
		return;

	last_ping = now;

	/* ping to get more hosts if we need connections */
	if (now - last_keep_alive >= 1 * MINUTES)
	{
		/* do a keepalive */
		ttl = 1;
		last_keep_alive = now;
	}
	else
	{
		/* get more hosts */
		ttl = 7;
	}

	ping_hosts_ttl (ttl);
}

/*****************************************************************************/

static void disconnect_no_query_route (void)
{
	int nr_supernodes;

	/* only disconnect if theres other nodes to fallback on */
	nr_supernodes = gt_conn_length (GT_NODE_ULTRA, GT_NODE_CONNECTED);

	if (nr_supernodes > 0)
	{
		gt_conn_foreach (node_disconnect_one, NULL,
		                 GT_NODE_LEAF, GT_NODE_CONNECTED, 0);
	}
}

static void report_connected_leaf (int connected)
{
	static int last_connected = 0;

	if (connected != last_connected)
	{
		GT->DBGFN (GT, "connected=%d nodes=%d", connected, 
		           gt_conn_length (GT_NODE_NONE, GT_NODE_ANY));
		last_connected = connected;
	}
}

static int get_need_as_ultra (gt_node_class_t klass)
{
	switch (klass)
	{
	 case GT_NODE_ULTRA: return GT_PEER_CONNECTIONS;
	 case GT_NODE_LEAF:  return GT_LEAF_CONNECTIONS;
	 default:            return 0;
	}
}

static int get_need_as_leaf (gt_node_class_t klass)
{
	switch (klass)
	{
	 case GT_NODE_ULTRA: return GT_SHIELDED_CONNECTIONS;
	 case GT_NODE_LEAF:  return 0; /* no leaf<->leaf connections allowed */
	 default:            return 0;
	}
}

int gt_conn_need_connections (gt_node_class_t klass)
{
	int connected;
	int desired;

	connected = gt_conn_length (klass, GT_NODE_CONNECTED);

	/* don't call this with multiple classes -- the need of one
	 * class could cancel a surplus of the other */
	assert (klass == GT_NODE_ULTRA || klass == GT_NODE_LEAF);

	if (GT_SELF->klass & GT_NODE_ULTRA)
		desired = get_need_as_ultra (klass);
	else
		desired = get_need_as_leaf (klass);

	return desired - connected;
}

static void disconnect_hosts (gt_node_class_t klass, int excess)
{
	int connected;

	connected = gt_conn_length (klass, GT_NODE_CONNECTED);

	GT->DBGFN (GT, "too many connections (%d)[%s], disconnecting %d", 
	           connected, gt_node_class_str (klass), excess);

	while (excess-- > 0)
	{
		GtNode *node = gt_conn_random (klass, GT_NODE_CONNECTED);

		/* TODO: send BYE message here */

		assert (GT_CONN(node) != NULL);
		gt_node_disconnect (GT_CONN(node));
	}
}

static BOOL disconnect_excess_timer (void *udata)
{
	int leaf_excess;
	int ultra_excess;

	leaf_excess  = gt_conn_need_connections (GT_NODE_LEAF);
	ultra_excess = gt_conn_need_connections (GT_NODE_ULTRA);

	if (leaf_excess < 0)
		disconnect_hosts (GT_NODE_LEAF, -leaf_excess);

	if (ultra_excess < 0)
		disconnect_hosts (GT_NODE_ULTRA, -ultra_excess);

	disconnect_timer = 0;
	return FALSE;
}

static GtNode *collect_each_node (TCPC *c, GtNode *node, List **nodes)
{
	if (node->tried_connect)
		return NULL;

	if (!node->gt_port)
		return NULL;

	/* mark having tried to to connect to this node already */
	node->tried_connect = TRUE;

	*nodes = list_append (*nodes, node);

	/* stop iterating if we have enough nodes */
	if (list_length (*nodes) >= TRY_CONNECT_NODE_LIST)
		return node;
	
	return NULL;
}

static GtNode *clear_try_bit (TCPC *c, GtNode *node, void *udata)
{
	node->tried_connect = FALSE;
	return NULL;
}

static BOOL prune_registered (struct cached_node *cached, void *udata)
{
	if (gt_node_lookup (cached->addr.ip, cached->addr.port))
	{
		GT->DBGFN (GT, "pruning %s (already registered)", 
		           net_ip_str (cached->addr.ip), cached->addr.port);
		free (cached);
		return TRUE;
	}

	return FALSE;
}

static BOOL register_cached (struct cached_node *cached, void *udata)
{
	GtNode *node;

	node = gt_node_lookup (cached->addr.ip, cached->addr.port);

	if (node)
	{
		/* 
		 * Argh, gt_node_lookup only matches by IP 
		 * This should be assert (0)
		 */
		assert (node->gt_port != cached->addr.port);

		free (cached);
		return TRUE;
	}

	node = gt_node_register (cached->addr.ip, cached->addr.port,
	                         cached->klass);

	/* we've got to free the node, Jim */
	free (cached);

	/* this happens if the address is invalid or a mem failure */
	if (!node)
		return TRUE;

	gt_connect (node);
	node->tried_connect = TRUE;

	return TRUE;
}

static BOOL connect_each (GtNode *node, void *udata)
{
	if (gt_connect (node) < 0)
	{
		GT->err (GT, "Failed to connect to node %s:%hu: %s",
		         net_ip_str (node->ip), node->gt_port, GIFT_NETERROR());
		return TRUE;
	}

	return TRUE;
}

/*****************************************************************************/

/* returns number of nodes we will try to connect to */
static size_t try_some_nodes (time_t now)
{
	List    *nodes  = NULL;
	List    *cached = NULL;
	size_t   total  = 0;
	size_t   nr;
	size_t   len;
	size_t   count;

	/* the total amount of nodes we should try */
	nr = TRY_CONNECT_NODE_LIST + TRY_CONNECT_NODE_CACHE;

	/*
	 * Iterate the node (pong) cache and node list until we
	 * have seen 'nr' nodes or there are no more hosts to try.
	 */

	while (total < nr)
	{
		gt_conn_foreach (GT_CONN_FOREACH(collect_each_node), &nodes,
		                 GT_NODE_NONE, GT_NODE_DISCONNECTED, 0);

		/* grab at most nr - total nodes (still need to fix the preceeding
		 * call to gt_conn_foreach() to respect 'total') */
		count = MIN (nr - total, TRY_CONNECT_NODE_CACHE);
		assert (count >= 0);

		cached = gt_node_cache_get_remove (count);

		/* registered nodes can still slip into our node cache, argh */
		cached = list_foreach_remove (cached, 
		                              (ListForeachFunc)prune_registered,
		                              NULL);

		len = list_length (nodes) + list_length (cached);

		total += len;

		if (len == 0)
			break;
		
		nodes = list_foreach_remove (nodes, (ListForeachFunc)connect_each, 
		                             NULL);
		assert (nodes == NULL);

		cached = list_foreach_remove (cached, (ListForeachFunc)register_cached,
		                              NULL);
		assert (cached == NULL);
	}

	return total;
}

static void maintain_class (gt_node_class_t klass, time_t now)
{
	int   connected;
	int   need;

	connected = gt_conn_length (klass, GT_NODE_CONNECTED);
	need      = gt_conn_need_connections (klass);

	/* 
	 * print the number of nodes connected if it has changed 
	 * XXX: print leaves from ultrapeers and leaves too.
	 *      damn static variables to hell 
	 */
	if (klass == GT_NODE_ULTRA)
		report_connected_leaf (connected);

	/* 0 == perfection */
	if (need == 0)
		return;

	/* disconnect some nodes */
	if (need < 0)
	{
		if (disconnect_timer)
			return;

		/* 
		 * Disconnect the node soon, because it could happen that 
		 * someone will disconnect from us first, causing cascading
		 * disconnects.
		 */
		GT->DBGFN (GT, "starting disconnect timer...");
		disconnect_timer = timer_add (4 * SECONDS, 
		                              (TimerCallback)disconnect_excess_timer, 
		                              NULL);
		return;
	}

	/*
	 * If try_some_nodes() returns 0, then there are no nodes in the node
	 * cache nor any on the node list that we haven't tried yet. In that case,
	 * we need to contact the gwebcaches and hope a fresh infusion of nodes
	 * will help. While we wait, we retry all the nodes we already tried by
	 * clearing node->tried_connect for each node, which otherwise prevents
	 * from recontacting the nodes.
	 *
	 * We will "block" on the gwebcaches if the bandwidth is completely
	 * saturated and we can't get a reply from anyone, or if there are no
	 * ultrapeers with connection slots available. The gwebcache subsystem
	 * imposes its own limits on how often it will contact gwebcaches, so if
	 * we do end up in this situation, hopefully we will simply spend most of
	 * the time unconnected rather than hammering the gwebcaches.
	 */
	if (try_some_nodes (now) == 0)
	{
		size_t len;

		len = gt_conn_length (GT_NODE_NONE, GT_NODE_ANY);
		GT->dbg (GT, "try_some_nodes() returned 0. node list len=%u", len);

		if (connected == 0 || len < 20)
		{
			/* try to get more hosts */
			GT->dbg (GT, "No hosts to try. Looking in gwebcaches...");
			gt_web_cache_update ();
		}

		GT->dbg (GT, "Retrying to connect to nodes...");

		/* while we are waiting for the gwebcaches, try each node again */
		gt_conn_foreach (GT_CONN_FOREACH(clear_try_bit), NULL,
		                 GT_NODE_NONE, GT_NODE_ANY, 0);

		return;
	}
}

/*
 * This is the main network maintainence function. All connections to the
 * network are initiated from here.
 */
static BOOL maintain (void *udata)
{
	time_t now;

	now = time (NULL);

	/* disconnect nodes without query routing if we are not a supernode */
	if (!(GT_SELF->klass & GT_NODE_ULTRA))
		disconnect_no_query_route ();

	/* TODO: expire old nodes here? */

#if 0
	trace_list (connections);
#endif

	/*
	 * Send pings to all connected nodes. We used to do this only every
	 * minute, but because some nodes have short timeouts if they receive
	 * nothing from you, we now do it every MAINTAIN_INTERVAL.
	 */
	ping_hosts (now);

	maintain_class (GT_NODE_ULTRA, now);
	maintain_class (GT_NODE_LEAF, now);

	return TRUE;
}

static BOOL cleanup (void *udata)
{
	/* trim excess nodes */
	gt_conn_trim ();

	/* save to disk important nodes from the node list */
	gt_node_list_save ();

	/* save to disk important nodes from the node cache */
	gt_node_cache_save ();

	return TRUE;
}

static BOOL retry_all (void *udata)
{
	/*
	 * Clear the 'tried' bit for all nodes, so if we start looking for nodes
	 * we try reconnecting to the ones we know about instead of contacting the
	 * gwebcaches.
	 *
	 * NOTE: should all the nodes be possibly retried (GT_NODE_ANY) or
	 * only those that are disconnected (GT_NODE_DISCONNECTED)?
	 */
	gt_conn_foreach (GT_CONN_FOREACH(clear_try_bit), NULL,
	                 GT_NODE_NONE, GT_NODE_ANY, 0);

	return TRUE;
}
/*****************************************************************************/

void gt_netorg_init (void)
{
	if (maintain_timer != 0)
		return;

	/* load the node cache */
	gt_node_cache_init ();

	/* setup the links maintain timer */
	maintain_timer = timer_add (MAINTAIN_INTERVAL,
	                            (TimerCallback)maintain, NULL);

	cleanup_timer = timer_add (CLEANUP_INTERVAL,
	                           (TimerCallback)cleanup, NULL);

	retry_all_timer = timer_add (RETRY_ALL_INTERVAL,
	                             (TimerCallback)retry_all, NULL);

	/* call it now so we don't have to wait the first time */
	maintain (NULL);
}

void gt_netorg_cleanup (void)
{
	/* save the node cache */
	gt_node_cache_cleanup ();

	timer_remove_zero (&disconnect_timer);

	timer_remove_zero (&maintain_timer);
	timer_remove_zero (&cleanup_timer);
	timer_remove_zero (&retry_all_timer);
}
