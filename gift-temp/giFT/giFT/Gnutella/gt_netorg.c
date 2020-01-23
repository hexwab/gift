/*
 * $Id: gt_netorg.c,v 1.12 2003/04/26 20:31:09 hipnod Exp $
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

#define NR_CONNECTIONS         (config_get_int (gt_conf, "main/connections=3"))

/*****************************************************************************/

static List *node_list;

static List *iterator;

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

	TRACE (("%s:%hu", net_ip_str (node->ip), node->gt_port));

	if (list_next (nodes))
		trace_list (list_next (nodes));
}

/*****************************************************************************/

GtNode *gt_conn_foreach (GtConnForeachFunc func, void *udata,
                         GtNodeClass klass, GtNodeState state,
                         int iter)
{
	GtNode      *node;
	Connection  *c;
	GtNode      *ret       = NULL;
	List        *ptr;
	List        *start;
	List        *next;
	unsigned int i, count;
	int          looped    = FALSE;
	int          iterating = FALSE;

	assert (func != NULL);

#if 0
	TRACE (("length of conn list: %u", list_length (connections)));
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
		state = NODE_ALL;

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

		if (state != NODE_ALL && node->state != state)
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

static GtNode *conn_counter (Connection *c, GtNode *node, int *length)
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

static GtNode *select_rand (Connection *c, GtNode *node, void **cmp)
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

static GtNode *node_reconnect (Connection *c, GtNode *node, int *processed)
{
	if (!node->gt_port)
		return NULL;

	if (gt_connect (node) < 0 && processed)
		(*processed)++;

	return NULL;
}

static GtNode *node_disconnect_one (Connection *c, GtNode *node, void *udata)
{
	TRACE (("[%s]: disconnecting", net_ip_str (NODE(c)->ip)));
	gt_node_disconnect (c);
	return NULL;
}

static GtNode *node_ping (Connection *c, GtNode *node, Gt_Packet *packet)
{
	gt_packet_send (c, packet);
	return NULL;
}

static void ping_hosts_ttl (uint8_t ttl)
{
	Gt_Packet *packet;
	gt_guid   *guid;

	if (!(guid = guid_new ()))
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
	                 NODE_NONE, NODE_CONNECTED, 0);

	/* discard our reference */
	gt_packet_put_ref (packet);

	free (guid);
}

static void ping_hosts (time_t now, int need_connections)
{
	static time_t large_ping_time;
	uint8_t       ttl;

	/* ping to get more hosts if we need connections */
	if (now - large_ping_time >= 2 * EMINUTES || need_connections)
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

static void disconnect_no_query_route ()
{
	int nr_supernodes;

	/* only disconnect if theres other nodes to fallback on */
	nr_supernodes = gt_conn_length (NODE_SEARCH, NODE_CONNECTED);

	if (nr_supernodes > 0)
	{
		gt_conn_foreach ((GtConnForeachFunc) node_disconnect_one, NULL,
		                 NODE_USER, NODE_CONNECTED, 0);
	}
}

static void report_connected (int connected)
{
	static int last_connected = 0;

	if (connected != last_connected)
	{
		TRACE (("connected=%d nodes=%d", connected,
		        gt_conn_length (NODE_NONE, -1)));
		last_connected = connected;
	}
}

/*****************************************************************************/

#if 0
static Connection *node_mark_expired (Connection *c, GtNode *node,
                                     int *expire_cnt)
{
	if (!*expire_cnt)
		return c;

	/* Nodes are mark for deletion by being in state dead */
	node_class_set (c, NODE_DEAD);
	(*expire_cnt)--;

	return NULL;
}
static int node_kill (Connection *c, void *udata)
{
	if (NODE(c)->klass == NODE_DEAD)
	{
		node_free (c);

		return TRUE;
	}

	return FALSE;
}
#endif

/*****************************************************************************/

int gt_conn_need_connections ()
{
	int connected;

	connected = gt_conn_length (NODE_SEARCH, NODE_CONNECTED);

	if (connected < NR_CONNECTIONS)
		return TRUE;

	return FALSE;
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

int gt_conn_maintain ()
{
	int           connected;
	int           total_nodes;
	int           desired          = NR_CONNECTIONS;
	int           need_connections = FALSE;
	time_t        now;
	static time_t ping_time;

	now = time (NULL);
	connected = gt_conn_length (NODE_SEARCH, NODE_CONNECTED);

	/* print the number of nodes connected if it has changed */
	report_connected (connected);

	if (desired > connected)
		need_connections = TRUE;

	/* send pings to all connected nodes */
	if (now - ping_time >= 1 * EMINUTES)
	{
		ping_hosts (now, need_connections);
		ping_time = now;
	}

	/* disconnect nodes without query routing if we are not a supernode */
	if (!(gt_self->klass & NODE_SEARCH))
		disconnect_no_query_route ();

	/* expire old nodes if the nodelist is full */
	if (gt_nodes_full (&total_nodes))
	{
#if 0
		int expire_cnt = total_nodes / 2;

		TRACE (("cannibalizing node list"));

		/*
		 * This is pretty brute force, but at least
		 * it doesnt leak (hopefully).
		 */

		/* TODO: separate user and search node marking */
		gt_conn_foreach ((GtConnForeachFunc) node_mark_expired, &expire_cnt,
		                 NODE_NONE, NODE_DISCONNECTED, 0);

		/* ok, now actually free the connections */
		connections = list_foreach_remove (connections,
		                                   (ListForeachFunc) node_kill, NULL);

		TRACE (("after cannibalized = %i nodes", list_length (connections)));
		trace_list (connections);

		/* resort by vitality */
		gt_conn_sort ((CompareFunc) conn_sort_vit);

		iterator = connections;
#endif
	}

#if 0
	trace_list (connections);
#endif

	/* try to find some more nodes to connect to */
	if (connected < desired && total_nodes < 20)
		gt_web_cache_update ();

	/*
	 * This logic needs revamping in ultrapeer mode
	 */
	if (connected < desired)
	{
		gt_conn_foreach ((GtConnForeachFunc) node_reconnect, NULL,
		                 NODE_NONE, NODE_DISCONNECTED, 10);
	}
#if 0
	else if (connected < desired)
	{
		gt_conn_foreach ((GtConnForeachFunc) node_reconnect, NULL,
		                 NODE_NONE, NODE_DISCONNECTED, 1);
	}
#endif
	else if (connected > desired)
	{
		TRACE (("nr connected > %i, disconnecting %i", desired,
		        connected - desired));

		gt_conn_foreach ((GtConnForeachFunc) node_disconnect_one, NULL,
		                 NODE_NONE, NODE_CONNECTED, connected - desired);
	}

	gt_node_update_cache ();

	return TRUE;
}
