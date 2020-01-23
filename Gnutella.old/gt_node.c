/*
 * $Id: gt_node.c,v 1.31 2003/06/07 07:13:01 hipnod Exp $
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

#include "gt_utils.h"

#include "gt_packet.h"
#include "gt_query_route.h"

/*****************************************************************************/

/* maps ids -> node, so we dont have to keep TCPC ptrs in
 * various data structures */
static Dataset *node_ids;

#if 0
/* identiifer used for nodes in the node cache */
static unsigned long node_counter;
#endif

/*****************************************************************************/

#if 0
static unsigned long node_uniq_id (void)
{
    node_counter++;

	while (!node_counter ||
	       dataset_lookup (node_ids, &node_counter, sizeof (node_counter)))
	{
		node_counter++;
	}

	return node_counter;
}

TCPC *node_find_by_id (unsigned long id)
{
	return dataset_lookup (node_ids, &id, sizeof (id));
}

void node_add (TCPC *c)
{
	int uniq_id;

	if (GT_NODE(c)->id != 0)
		return;

	if (!(uniq_id = node_uniq_id ()))
		return;

	if (!node_ids)
		node_ids = dataset_new (DATASET_HASH);

	dataset_insert (&node_ids, &uniq_id, sizeof (uniq_id), c, 0);

	GT_NODE(c)->id = uniq_id;
}

void node_remove (TCPC *c)
{
	int id = GT_NODE(c)->id;

	dataset_remove (node_ids, &id, sizeof (id));

	GT_NODE(c)->id = 0;
}
#endif

/*****************************************************************************/

static void node_add (GtNode *node)
{
	if (!node_ids)
		node_ids = dataset_new (DATASET_HASH);

	if (!node->ip)
		return;

	dataset_insert (&node_ids, &node->ip, sizeof (node->ip), node, 0);
}

static void node_remove (GtNode *node)
{
	if (!node)
		return;

	if (!node->ip)
		return;

	dataset_remove (node_ids, &node->ip, sizeof (node->ip));
}

/*****************************************************************************/

int gt_nodes_full (unsigned int *total_nodes)
{
	int count;

	count = dataset_length (node_ids);

	if (total_nodes)
		*total_nodes = count;

	return count >= MAX_NODES;
}

GtNode *gt_node_new ()
{
	GtNode *node;

	if (!(node = MALLOC (sizeof (GtNode))))
		return NULL;

	return node;
}

static void free_node (GtNode *node)
{
	if (!node)
		return;

	gt_node_disconnect (GT_CONN(node));
	gt_conn_remove (node);

	free (node);
}

/* NOTE: this isnt safe to call at all times */
void gt_node_free (GtNode *node)
{
	node_remove (node);
	free_node (node);
}

/* set the node to use the TCP connection */
void gt_node_connect (GtNode *node, TCPC *c)
{
	assert (GT_CONN(node) == NULL);
	assert (GT_NODE(c) == NULL);

	GT_CONN(node) = c;
	GT_NODE(c) = node;
}

/* instantiate a node from an existing connection */
GtNode *gt_node_instantiate (TCPC *c)
{
	GtNode *node;
	struct sockaddr_in saddr;
	int len;

	if (!c)
		return NULL;

	/*
	 * Ugh this code sucks for duplicates
	 */
	if (!(node = gt_node_new ()))
		return NULL;

	/* attach this node to the connection and vice-versa */
	gt_node_connect (node, c);

	/* fill in peer info (in network byte order) */
	if (!node->ip)
	{
		len = sizeof (saddr);
		if (getpeername (c->fd, (struct sockaddr *) &saddr, &len) == 0)
			node->ip = saddr.sin_addr.s_addr;
	}

	if (node->ip)
	{
		node_add (node);
		gt_conn_add (node);
	}

	return node;
}

static int free_one (ds_data_t *key, ds_data_t *value, void *udata)
{
	GtNode *node = value->data;

	/* don't call gt_node_free here while iterating through the
	 * Dataset because it will cause us to miss items when the
	 * Dataset is resized */
	free_node (node);

	return DS_CONTINUE | DS_REMOVE;
}

void gt_node_remove_all (void)
{
	dataset_foreach_ex (node_ids, DS_FOREACH_EX(free_one), NULL);
	dataset_clear (node_ids);
	node_ids = NULL;
}

/*****************************************************************************/

static int free_packet (GtPacket *packet, void *udata)
{
	gt_packet_free (packet);
	return TRUE;
}

static void stop_queue (TCPC *c)
{
	input_remove (GT_NODE(c)->queue_id);
	GT_NODE(c)->queue_id = 0;
}

static void transmit_packet (int fd, input_id id, TCPC *c)
{
	List      *ptr;
	int        len;
	GtPacket  *packet;

	ptr = GT_NODE(c)->packet_queue;
	packet = ptr->data;

	/* remove the head */
	GT_NODE(c)->packet_queue = list_remove_link (GT_NODE(c)->packet_queue, ptr);

	len = net_send (fd, packet->data, packet->len);

	if (len == packet->len)
		gt_packet_log (packet, c, TRUE);

	/* stop sending if there's no more packets */
	if (!GT_NODE(c)->packet_queue)
		stop_queue (c);

	gt_packet_free (packet);
}

static void flush_queue (TCPC *c)
{
	GT_NODE(c)->packet_queue = list_foreach_remove (GT_NODE(c)->packet_queue,
	                                             (ListForeachFunc)free_packet,
	                                             NULL);

	stop_queue (c);
}

static void start_queue (TCPC *c)
{
	if (GT_NODE(c)->queue_id != 0)
		return;

	GT_NODE(c)->queue_id = input_add (c->fd, c, INPUT_WRITE,
	                               (InputCallback)transmit_packet, FALSE);
}

void gt_node_queue (TCPC *c, GtPacket *packet)
{
	/*
	 * Don't queue the packet if the node isn't in a state
	 * to send it.
	 */
	if (!(GT_NODE(c)->state & (GT_NODE_CONNECTED | GT_NODE_CONNECTING_2)))
	{
		gt_packet_free (packet);
		return;
	}

	if (!c || c->fd < 0)
	{
		gt_packet_free (packet);
		return;
	}

	assert (GT_NODE(c)->c == c);
	start_queue (c);

	GT_NODE(c)->packet_queue = list_append (GT_NODE(c)->packet_queue, packet);
}

/*****************************************************************************/

GtNode *gt_node_lookup (in_addr_t ip, in_port_t port)
{
	return dataset_lookup (node_ids, &ip, sizeof (ip));
}

GtNode *gt_node_register (in_addr_t ip, in_port_t port,
                          GtNodeClass klass, uint32_t size_kb,
                          uint32_t files)
{
	GtNode *node;

	if (GNUTELLA_LOCAL_MODE)
	{
		if (!net_match_host (ip, "LOCAL"))
			return NULL;
	}

	if (!ip)
		return NULL;

	/* TODO: there is probably a problem here if a node is already
	 *       connected and we're informed about it falsely some other way.  */
	if ((node = dataset_lookup (node_ids, &ip, sizeof (ip))))
	{
		if (klass != GT_NODE_NONE)
			gt_node_class_set (node, klass);

		node->size_kb = size_kb;
		node->files   = files;

		return node;
	}

	if (!(node = gt_node_new ()))
		return NULL;

	node->ip      = ip;
	node->gt_port = port;

	node_add (node);
	gt_conn_add (node);

	if (klass != GT_NODE_NONE)
		gt_node_class_set (node, klass);

	node->size_kb = size_kb;
	node->files   = files;

	return node;
}

void gt_node_error (TCPC *c, char *fmt, ...)
{
	static char buf[4096];
	va_list     args;

	assert (GT_CONN(GT_NODE(c)) == c);

	if (!fmt)
	{
		GT->dbgsock (GT, c, "[%hu] error: %s", GT_NODE(c)->gt_port, GIFT_NETERROR ());
		return;
	}

	va_start (args, fmt);
	vsnprintf (buf, sizeof (buf) - 1, fmt, args);
	va_end (args);

	GT->dbgsock (GT, c, "error: %s", buf);
}

void gt_node_disconnect (TCPC *c)
{
	struct timeval tv;
	GtNode        *node;

	if (!c)
		return;

	node = GT_NODE(c);
	assert (node->c == c);

	/* remove node timers */
	timer_remove_zero (&node->handshake_timer);
	timer_remove_zero (&node->search_timer);
	timer_remove_zero (&node->query_route_timer);

	/* reset tracking of any QRP table sent to this node */
	node->query_router_counter = 0;

	/* flush auxillary data for this node's connection */
	flush_queue (c);

	/* close the connection for this node, if any */
	tcp_close_null (&GT_CONN(node));

	/*pong_cache_waiter_remove (c);*/

	/* clear verification connection */
	tcp_close_null (&node->gt_port_verify);

	free (node->ping_guid);
	node->ping_guid = NULL;

	dataset_clear (node->cap);
	node->cap = NULL;

	gt_query_router_free (node->query_router);
	node->query_router         = NULL;
	node->query_router_counter = 0;

	node->firewalled = FALSE;
	node->verified   = FALSE;
	node->incoming   = FALSE;

	platform_gettimeofday (&tv, NULL);

	/*
	 * Update the last time if this node was connected
	 */
	node->last_connect_duration = tv.tv_sec - node->start_connect_time;
	node->total_connect_duration += node->last_connect_duration;

	/*
	 * If we werent ever connected to this node for any significant amount
	 * of time, mark it as GT_NODE_DEAD, which will clean it off the
	 * connection list eventually.
	 */
	if (node->total_connect_duration < 10 /* SECONDS */ &&
	    node->vitality == 0 &&
	    node->klass != GT_NODE_INDEX &&
	    !node->incoming)
	{
		/* Hmm, this could cause us to access the webcaches excessively */
#if 0
		gt_node_class_set (node, GT_NODE_DEAD);
#endif
	}

	gt_node_state_set (node, GT_NODE_DISCONNECTED);
}

struct _sync_args
{
	struct timeval tv;
	FILE  *f;
};

static GtNode *sync_node (TCPC *c, GtNode *node, struct _sync_args *sync)
{
	/* node->vitality is updated lazily, to avoid a syscall for every
	 * packet.  Maybe this isnt worth it */
	if (node->state & GT_NODE_CONNECTED)
		node->vitality = sync->tv.tv_sec;

	/* only cache the node if we have connected to it before successfully */
	if (node->vitality > 0 && node->gt_port > 0)
	{
		fprintf (sync->f, "%lu %s:%hu\n", node->vitality,
		         net_ip_str (node->ip), node->gt_port);
	}

	return NULL;
}

void gt_node_update_cache (void)
{
	struct _sync_args sync;
	char  *tmp_path;

	platform_gettimeofday (&sync.tv, NULL);

	tmp_path = STRDUP (gift_conf_path ("Gnutella/nodes.tmp"));

	if (!(sync.f = fopen (gift_conf_path ("Gnutella/nodes.tmp"), "w")))
	{
		GT->DBGFN (GT, "error opening tmp file: %s", GIFT_STRERROR ());
		free (tmp_path);
		return;
	}

	gt_conn_foreach ((ConnForeachFunc) sync_node, &sync,
	                 GT_NODE_NONE, -1, 0);

	fclose (sync.f);
	file_mv (tmp_path, gift_conf_path ("Gnutella/nodes"));

	free (tmp_path);
}

/*****************************************************************************/

char *gt_node_class_str (GtNodeClass klass)
{
	switch (klass)
	{
	 case GT_NODE_NONE:   return "NONE";
	 case GT_NODE_LEAF:   return "LEAF";
	 case GT_NODE_ULTRA:  return "ULTRAPEER";
	 case GT_NODE_DEAD:   return "DEAD (freeing node)";
	 default:             return "<Unknown class>";
	}
}

char *gt_node_state_str (GtNodeState state)
{
	switch (state)
	{
	 case GT_NODE_DISCONNECTED:    return "Disconnected";
	 case GT_NODE_CONNECTING_1:    return "Connecting (half-open)";
	 case GT_NODE_CONNECTING_2:    return "Connecting (awaiting ping response)";
	 case GT_NODE_CONNECTED:       return "Connected";
	 default:                      return "<Unknown state>";
	}
}

static char *gt_node_str (GtNode *node)
{
	static char buf[128];

	snprintf (buf, sizeof (buf) - 1, "%s:%hu", net_ip_str (node->ip),
	          node->gt_port);

	return buf;
}

void gt_node_state_set (GtNode *node, GtNodeState state)
{
	GtNodeState old_state = node->state;

	if (old_state != state)
	{
		GT->dbg (GT, "%-24s %s", gt_node_str (node),
		         gt_node_state_str (state));

		node->state = state;
	}
}

void gt_node_class_set (GtNode *node, GtNodeClass klass)
{
	GtNodeClass old_class = node->klass;

	if (old_class != klass)
	{
		if (old_class == GT_NODE_NONE)
		{
			GT->dbg (GT, "%-24s %s", gt_node_str (node),
			         gt_node_class_str (klass));
		}
		else
		{
			GT->dbg (GT, "%-24s %s (%s)", gt_node_str (node),
			         gt_node_class_str (klass), gt_node_class_str (old_class));
		}

		node->klass = klass;
	}
}
