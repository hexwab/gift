/*
 * $Id: gt_node.c,v 1.59 2005/01/04 15:00:51 mkern Exp $
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

#include "gt_utils.h"

#include "gt_packet.h"
#include "gt_query_route.h"
#include "gt_share_state.h"

#include "gt_node_cache.h"

#include "io/rx_stack.h"            /* gt_rx_stack_free */
#include "io/tx_stack.h"            /* gt_tx_stack_free, gt_tx_stack_queue */

#include "transfer/push_proxy.h"

/*****************************************************************************/

/* maps ids -> node, so we dont have to keep TCPC ptrs in
 * various data structures */
static Dataset *node_ids;

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

	node->c = c;
	c->udata = node;
}

/* put the node into some data structures to keep track of it */
static void track_node (GtNode *node, TCPC *c)
{
	if (node->ip)
		assert (node->ip == c->host);

	/* fill in peer info (in network byte order) */
	node->ip = c->host;
	assert (node->ip != 0);

	gt_conn_add (node);
	node_add (node);
}

/* instantiate a node from an existing connection */
GtNode *gt_node_instantiate (TCPC *c)
{
	GtNode *node;
	BOOL    existed = FALSE;

	if (!c || !c->host)
		return NULL;

	/* TODO: We should really lookup the port in Listen-IP header, right? */
	node = gt_node_lookup (c->host, 0);

	if (node)
	{
		existed = TRUE;

		/* abort if already connected/connecting */
		if (node->state != GT_NODE_DISCONNECTED)
			return NULL;
	}
	else
	{
		if (!(node = gt_node_new ()))
			return NULL;
	}

	assert (node->c == NULL);

	/* attach this node to the connection and vice-versa */
	gt_node_connect (node, c);

	if (!existed)
		track_node (node, c);

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

BOOL gt_node_freeable (GtNode *node)
{
	time_t now;

	if (node->state != GT_NODE_DISCONNECTED)
		return FALSE;

	now = time (NULL);

	/* keep hosts with whom we've had a connection for a good while */
	if (node->vitality > 0 && now - node->vitality <= 30 * EDAYS)
		return FALSE;

	if (now - node->start_connect_time <= 30 * EMINUTES)
		return FALSE;

	/* yeah, sure, free the node if you want */
	return TRUE;
}

/*****************************************************************************/

/*
 * Check if this node supports the vendor message packet inside 'pkt',
 * and then send the vendor message if so.
 *
 * The 'version' field of the VMSG is mangled to be the minimum supported
 * by both this node and the remote end.
 */
BOOL gt_node_send_if_supported (GtNode *node, GtPacket *pkt)
{
	gt_vendor_msg_t vmsg;
	unsigned char  *vendor;
	uint16_t        id;
	uint16_t        ver;
	uint16_t       *send_ver;

	gt_packet_seek (pkt, GNUTELLA_HDR_LEN);
	vendor = gt_packet_get_ustr   (pkt, 4);
	id     = gt_packet_get_uint16 (pkt);
	ver    = gt_packet_get_uint16 (pkt);

	if (gt_packet_error (pkt))
		return FALSE;

	memset (&vmsg, 0, sizeof(vmsg));
	memcpy (&vmsg.vendor_id, vendor, 4);
	vmsg.id = id;

	send_ver = dataset_lookup (node->vmsgs_supported, &vmsg, sizeof(vmsg));
	if (!send_ver)
		return FALSE;

	/* XXX: we've no good facility for writing in the middle of the packet */
	memcpy (&pkt->data[GNUTELLA_HDR_LEN + VMSG_HDR_LEN - 2], send_ver, 2);

	if (gt_packet_send (GT_CONN(node), pkt) < 0)
		return FALSE;

	return TRUE;
}

BOOL gt_node_send (GtNode *node, GtPacket *packet)
{
	/* don't queue the packet if the node isn't in a state to send it */
	if (!(node->state & (GT_NODE_CONNECTED | GT_NODE_CONNECTING_2)))
		return FALSE;

	/* enable this at some point in the future */
#if 0
	assert (GT_CONN(node) != NULL);
#endif
	if (!GT_CONN(node) || GT_CONN(node)->fd < 0)
		return FALSE;

	return gt_tx_stack_queue (node->tx_stack, packet->data, packet->len);
}

/*****************************************************************************/

GtNode *gt_node_lookup (in_addr_t ip, in_port_t port)
{
	return dataset_lookup (node_ids, &ip, sizeof (ip));
}

GtNode *gt_node_register (in_addr_t ip, in_port_t port,
                          gt_node_class_t klass)
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

	/* remove this node from the node cache in order to keep the cache
	 * conherent with the node list */
	gt_node_cache_del_ipv4 (ip, port);

	return node;
}

void gt_node_error (TCPC *c, const char *fmt, ...)
{
	static char buf[4096];
	va_list     args;

	assert (GT_CONN(GT_NODE(c)) == c);

	if (!fmt)
	{
		GT->DBGSOCK (GT, c, "[%hu] error: %s", GT_NODE(c)->gt_port,
		             GIFT_NETERROR ());
		return;
	}

	va_start (args, fmt);
	vsnprintf (buf, sizeof (buf) - 1, fmt, args);
	va_end (args);

	GT->DBGSOCK (GT, c, "error: %s", buf);
}

void gt_node_disconnect (TCPC *c)
{
	GtNode *node;

	if (!c)
		return;

	node = GT_NODE(c);
	assert (node->c == c);

	/* remove node timers */
	timer_remove_zero (&node->handshake_timer);
	timer_remove_zero (&node->search_timer);
	timer_remove_zero (&node->query_route_timer);

	/* destroy existing received buffers for this connection */
	gt_rx_stack_free (node->rx_stack);
	node->rx_stack = NULL;

	/* destroy send buffers */
	gt_tx_stack_free (node->tx_stack);
	node->tx_stack = NULL;

	/* remove the node from push proxy status */
	gt_push_proxy_del (node);

	/* reset connection status flags */
	node->verified      = FALSE;
	node->firewalled    = FALSE;
	node->incoming      = FALSE;
	node->rx_inflated   = FALSE;
	node->tx_deflated   = FALSE;
	node->vmsgs_sent    = FALSE;

	/* close the connection for this node, if any */
	tcp_close_null (&node->c);

	node->pings_with_noreply = 0;

	/* clear verification connection */
	tcp_close_null (&node->gt_port_verify);

	free (node->ping_guid);
	node->ping_guid = NULL;

	dataset_clear (node->hdr);
	node->hdr = NULL;

	dataset_clear (node->vmsgs_supported);
	node->vmsgs_supported = NULL;

	gt_share_state_free (node->share_state);
	node->share_state = NULL;

	gt_query_router_free (node->query_router);
	node->query_router         = NULL;
	node->query_router_counter = 0;

	/* update the last time if this node was connected */
	node->last_connect_duration = time (NULL) - node->start_connect_time;
	node->total_connect_duration += node->last_connect_duration;

	gt_node_state_set (node, GT_NODE_DISCONNECTED);
}

/*****************************************************************************/

char *gt_node_class_str (gt_node_class_t klass)
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

char *gt_node_state_str (gt_node_state_t state)
{
	switch (state)
	{
	 case GT_NODE_DISCONNECTED:    return "Disconnected";
	 case GT_NODE_CONNECTING_1:    return "Connecting (handshaking)";
	 case GT_NODE_CONNECTING_2:    return "Connecting (awaiting ping response)";
	 case GT_NODE_CONNECTED:       return "Connected";
	 default:                      return "<Unknown state>";
	}
}

char *gt_node_str (GtNode *node)
{
	static char buf[128];

	snprintf (buf, sizeof (buf) - 1, "%s:%hu", net_ip_str (node->ip),
	          node->gt_port);

	return buf;
}

void gt_node_state_set (GtNode *node, gt_node_state_t state)
{
	gt_node_state_t old_state = node->state;

	if (old_state != state)
	{
		node->state = state;
		gt_conn_set_state (node, old_state, state);
	}
}

void gt_node_class_set (GtNode *node, gt_node_class_t klass)
{
	gt_node_class_t old_class = node->klass;

	if (old_class != klass)
	{
		/* quiet, please */
#if 0
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
#endif

		node->klass = klass;
		gt_conn_set_class (node, old_class, klass);
	}
}
