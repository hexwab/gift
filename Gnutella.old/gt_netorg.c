/*
 * $Id: gt_netorg.c,v 1.22 2003/06/07 07:13:01 hipnod Exp $
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
#include "gt_netorg.h"

#include "gt_connect.h"
#include "gt_accept.h"

#include "gt_packet.h"
#include "gt_protocol.h"

#include "gt_web_cache.h"

/*****************************************************************************/

/* how often we check the network's condition */
#define MAINTAIN_LINKS         (10 * SECONDS)

/*****************************************************************************/

/* list of all nodes -- NOTE: duplicated info in gt_node.c */
static List      *node_list;

/* last place in node_list for gt_conn_foreach */
static List      *iterator;

/* timer for initiating/closing connections */
static timer_id   maintain_timer;

/*****************************************************************************/

void gt_conn_add (GtNode *node)
{
	if (!node)
	{
		GIFT_ERROR (("adding null node to node list"));
		return;
	}

	node_list = list_append (node_list, node);

	if (!iterator)
		iterator = node_list;
}

void gt_conn_remove (GtNode *node)
{
	if (!list_find (node_list, node))
		return;

	if (list_nth_data (iterator, 0) == node)
		iterator = iterator->next;

	node_list = list_remove (node_list, node);

	if (!iterator)
		iterator = node_list;
}

static void trace_list (List *nodes)
{
	GtNode *node;

	if (!nodes)
		return;

	node = list_nth_data (nodes, 0);

	assert (node != NULL);
	assert (GT_CONN(node) != NULL);

	GT->DBGFN (GT, "%s:%hu", net_ip_str (node->ip), node->gt_port);

	if (list_next (nodes))
		trace_list (list_next (nodes));
}

/*****************************************************************************/

GtNode *gt_conn_foreach (GtConnForeachFunc func, void *udata,
                         GtNodeClass klass, GtNodeState state,
                         int iter)
{
	GtNode      *node;
	TCPC        *c;
	GtNode      *ret       = NULL;
	List        *ptr;
	List        *start;
	List        *next;
	unsigned int i, count;
	int          looped    = FALSE;
	int          iterating = FALSE;

	assert (func != NULL);

#if 0
	GT->DBGFN (GT, "length of conn list: %u", list_length (connections));
#endif

	if (iter)
		iterating = TRUE;

	if (!iterator)
		iterator = node_list;

	start = ptr = (iterating) ? iterator : node_list;

	/* having count be the static list length should keep
	 * concurrent conn_adds from making us never stop */
	count = list_length (node_list);

	/* hack for backward-compatible interface */
	if (state == (GtNodeState) -1)
		state = GT_NODE_ALL;

	for (i = 0; i < count; i++)
	{
		if (iter && !ptr && !looped)
		{
			/* data only gets appended to connection list:
			 * safe to use head of connection list (connections) */
			ptr = node_list;
			looped = TRUE;
		}

		if (!ptr)
			break;

		/* we wrapped to the beginning, but have just reached the original
		 * start so we should bail out */
		if (looped && ptr == start)
			break;

		node = ptr->data;
		c = GT_CONN(node);

		assert (node != NULL);

		if (klass && !(node->klass & klass))
		{
			ptr = list_next (ptr);
			continue;
		}

		if (state != GT_NODE_ALL && node->state != state)
		{
			ptr = list_next (ptr);
			continue;
		}

		/* grab the next item. this allows the callback to free this item */
		next = list_next (ptr);

		ret = (*func) (c, node, udata);

		ptr = next;

		if (ret)
			break;

		if (iterating && --iter == 0)
			break;
	}

	/* save the position for next time */
	if (iterating)
		iterator = ptr;

	return ret;
}

/*****************************************************************************/

static GtNode *conn_counter (TCPC *c, GtNode *node, int *length)
{
	(*length)++;
	return NULL;
}

int gt_conn_length (GtNodeClass klass, GtNodeState state)
{
	int ret = 0;

	gt_conn_foreach ((GtConnForeachFunc) conn_counter,
	                 &ret, klass, state, 0);

	return ret;
}

static GtNode *select_rand (TCPC *c, GtNode *node, void **cmp)
{
	int     *nr    = cmp[0];
	GtNode **ret   = cmp[1];
	float    range = *nr;
	float    prob;

	/* make sure we pick at least one */
	if (!*ret)
		*ret = node;

	/* set the probability of selecting this node */
	prob = range * rand() / (RAND_MAX + 1.0);

	if (prob < 1)
		*ret = node;

	(*nr)++;

	/* we dont use the return value here, because we need to try
	 * all the nodes, and returning non-null here short-circuits */
	return NULL;
}

/*
 * Pick a node at random that is also of the specified 
 * class and state.
 */
GtNode *gt_conn_random (GtNodeClass klass, GtNodeState state)
{
	void   *cmp[2];
	int     nr;   
	GtNode *ret = NULL;

	/* initial probability */
	nr = 1;

	cmp[0] = &nr;
	cmp[1] = &ret;

	gt_conn_foreach ((GtConnForeachFunc) select_rand,
	                 &cmp, klass, state, 0);

	return ret;
}

/*****************************************************************************/

static GtNode *node_reconnect (TCPC *c, GtNode *node, int *processed)
{
	if (!node->gt_port)
		return NULL;

	if (gt_connect (node) < 0 && processed)
		(*processed)++;

	return NULL;
}

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
	gt_guid_t *guid;

	if (!(guid = gt_guid_new ()))
		return;

	if (!(packet = gt_packet_new (GT_PING_REQUEST, ttl, guid)))
	{
		free (guid);
		return;
	}

	/* Get a reference to the packet so that if there are no
	 * connected nodes it gets freed. Argh. */
	gt_packet_get_ref (packet);

	gt_conn_foreach ((GtConnForeachFunc) node_ping, packet,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);

	/* discard our reference */
	gt_packet_put_ref (packet);

	free (guid);
}

static void ping_hosts (time_t now)
{
	static time_t large_ping_time;
	uint8_t       ttl;

	/* ping to get more hosts if we need connections */
	if (now - large_ping_time >= 2 * EMINUTES || 
	    gt_conn_need_connections (GT_NODE_ULTRA) > 0)
	{
		ttl = 7;
		large_ping_time = now;
	}
	else
	{
		/* just do a keepalive */
		ttl = 1;
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
		gt_conn_foreach ((GtConnForeachFunc) node_disconnect_one, NULL,
		                 GT_NODE_LEAF, GT_NODE_CONNECTED, 0);
	}
}

static void report_connected_leaf (int connected)
{
	static int last_connected = 0;

	if (connected != last_connected)
	{
		GT->DBGFN (GT, "connected=%d nodes=%d", connected, 
		           gt_conn_length (GT_NODE_NONE, -1));
		last_connected = connected;
	}
}

/*****************************************************************************/

#if 0
static TCPC *node_mark_expired (TCPC *c, GtNode *node, int *expire_cnt)
{
	if (!*expire_cnt)
		return c;

	/* Nodes are mark for deletion by being in state dead */
	node_class_set (c, GT_NODE_DEAD);
	(*expire_cnt)--;

	return NULL;
}
static int node_kill (TCPC *c, void *udata)
{
	if (GT_NODE(c)->klass == GT_NODE_DEAD)
	{
		node_free (c);

		return TRUE;
	}

	return FALSE;
}
#endif

/*****************************************************************************/

static int get_need_as_ultra (GtNodeClass klass)
{
	switch (klass)
	{
	 case GT_NODE_ULTRA: return GT_PEER_CONNECTIONS;
	 case GT_NODE_LEAF:  return GT_LEAF_CONNECTIONS;
	 default:            return 0;
	}
}

static int get_need_as_leaf (GtNodeClass klass)
{
	switch (klass)
	{
	 case GT_NODE_ULTRA: return GT_SHIELDED_CONNECTIONS;
	 case GT_NODE_LEAF:  return 0; /* no leaf<->leaf connections allowed */
	 default:            return 0;
	}
}

int gt_conn_need_connections (GtNodeClass klass)
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

int gt_conn_sort_vit (GtNode *a, GtNode *b)
{
	if (a->vitality > b->vitality)
		return -1;
	else if (a->vitality < b->vitality)
		return 1;
	else
		return 0;
}

/* NOTE: this isnt safe to call at all times */
void gt_conn_sort (CompareFunc func)
{
	node_list = list_sort (node_list, func);
}

static void maintain_class (GtNodeClass klass, time_t now)
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

	if (need > 0)
	{
		gt_conn_foreach (GT_CONN_FOREACH(node_reconnect), NULL,
		                 GT_NODE_NONE, GT_NODE_DISCONNECTED, 10);
	}
	else if (need < 0)
	{
		GT->DBGFN (GT, "too many connections (%d)[%s], disconnecting %d", 
		           connected, gt_node_class_str (klass), -need);

		gt_conn_foreach (GT_CONN_FOREACH(node_disconnect_one), NULL,
		                 GT_NODE_NONE, GT_NODE_CONNECTED, -need);
	}
}

static int maintain (void *udata)
{
	time_t        now;
	static time_t ping_time;

	now = time (NULL);

	/* disconnect nodes without query routing if we are not a supernode */
	if (!(GT_SELF->klass & GT_NODE_ULTRA))
		disconnect_no_query_route ();

	/* check the webcaches if we need more hosts */
	gt_web_cache_update ();

	/* TODO: expire old nodes here? */

#if 0
	trace_list (connections);
#endif

	/* send pings to all connected nodes */
	if (now - ping_time >= 1 * EMINUTES)
	{
		ping_hosts (now);
		ping_time = now;
	}

	maintain_class (GT_NODE_ULTRA, now);
	maintain_class (GT_NODE_LEAF, now);

	gt_node_update_cache ();

	return TRUE;
}

/*****************************************************************************/

void gt_netorg_init (void)
{
	if (maintain_timer != 0)
		return;

	/* setup the links maintain timer */
	maintain_timer = timer_add (MAINTAIN_LINKS, 
	                            (TimerCallback)maintain, NULL);

	/* call it now so we don't have to wait the first time */
	maintain (NULL);
}

void gt_netorg_cleanup (void)
{
	timer_remove_zero (&maintain_timer);
}
