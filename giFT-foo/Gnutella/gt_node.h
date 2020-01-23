/*
 * $Id: gt_node.h,v 1.22 2003/06/07 07:13:01 hipnod Exp $
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

#ifndef __GT_NODE_H__
#define __GT_NODE_H__

/*****************************************************************************/

#define MAX_NODES               gt_config_get_int("main/max_nodes=1000")

/*****************************************************************************/

struct gt_query_router;
struct gt_packet;

typedef enum gt_node_state
{
	GT_NODE_DISCONNECTED  = 0x00, /* functionless node */
	GT_NODE_CONNECTING_1  = 0x01, /* pending */
	GT_NODE_CONNECTING_2  = 0x02, /* waiting for first ping response */
	GT_NODE_CONNECTED     = 0x08, /* first packet is seen */
	GT_NODE_IDLE          = 0x10, /* after handshake */
} GtNodeState;

typedef enum gt_node_class
{
	GT_NODE_NONE   = 0x00,
	GT_NODE_LEAF   = 0x01,        /* plain 0.6 or 0.4 nodes */
	GT_NODE_ULTRA  = 0x02,        /* ultrapeers */
	GT_NODE_INDEX  = 0x04,        /* pseudo-indexing nodes ::unused */
	GT_NODE_DEAD   = 0x08,        /* node is marked for deletion */
	GT_NODE_ALL    = 0xFF,        /* special node state indicating all states */
} GtNodeClass;

typedef struct gt_node
{
	in_addr_t       ip;

	/* the gnutella port of the other side */
	in_port_t       gt_port;

	/* the port the other side came from, could be the same as gt_port */
	in_port_t       peer_port;

	/* IP address used in communication with this node */
	in_addr_t       my_ip;

	/* capabilities of this node, set from HTTP headers on 0.6 connection */
	Dataset        *cap;

	unsigned int    incoming   : 1;    /* incoming connection */
	unsigned int    verified   : 1;    /* port has been verified */
	unsigned int    firewalled : 1;    /* firewalled connection */

	/* current state of the given connection (TODO - IDLE cannot be properly
	 * implemented in this way) */
	GtNodeState     state;

	/* node classification that this connection is communicating with
	 * (bitwise OR) */
	GtNodeClass     klass;

	/* TCPC a node uses. could be null */
	TCPC           *c;

	/* TCPC used for port verification */
	TCPC          *gt_port_verify;

	/* identifier for this node in the GUID cache */
	unsigned long  id;

	/* stats information */
	unsigned long  size_kb;
	unsigned long  files;

	/* timers for node things */
	timer_id       handshake_timer;
	timer_id       search_timer;
	timer_id       query_route_timer;

	/* around the time of the last connect to this node */
	time_t         vitality;

	/* number of disconnections from this node */
	unsigned int   disconnect_cnt;

	/* guid of the last ping from this node */
	gt_guid_t     *ping_guid;

	/* time of the last ping from this node */
	time_t         last_ping_time;

	/* start of the last connect to this node */
	time_t         start_connect_time;

	/* time the last connection made to this node lasted */
	time_t         last_connect_duration;

	/* total amount of time we have been connected to this node */
	time_t         total_connect_duration;

	/* router for query packets */
	struct gt_query_router   *query_router;

	/* version of the query router table submitted to this node currently */
	int                       query_router_counter;

	/* packet queue and input id for packet sending */
	input_id    queue_id;
	List       *packet_queue;
} GtNode;

/*****************************************************************************/

#define GT_NODE(c)         ((GtNode *)c->udata)
#define GT_CONN(node)      ((TCPC *) node->c)

/*****************************************************************************/

GtNode       *gt_node_new         (void);
GtNode       *gt_node_instantiate (TCPC *c);
void          gt_node_free        (GtNode *node);

void          gt_node_connect     (GtNode *node, TCPC *c);
void          gt_node_disconnect  (TCPC *c);
void          gt_node_error       (TCPC *c, char *fmt, ...);

void          gt_node_remove_all  (void);

/*****************************************************************************/

void     gt_node_state_set   (GtNode *node, GtNodeState state);
void     gt_node_class_set   (GtNode *node, GtNodeClass klass);

char    *gt_node_class_str   (GtNodeClass klass);
char    *gt_node_state_str   (GtNodeState state);

/*****************************************************************************/

GtNode       *gt_node_lookup      (in_addr_t ip, in_port_t port);
GtNode       *gt_node_register    (in_addr_t ip, in_port_t port,
                                   GtNodeClass klass, uint32_t size_kb,
                                   uint32_t files);

/*****************************************************************************/

void          gt_node_queue       (TCPC *c, struct gt_packet *packet);

/*****************************************************************************/


/*
 * Returns TRUE if max number of nodes have been allocated, FALSE otherwise
 *
 * Total number of nodes allocated returned in total_nodes
 */
int       gt_nodes_full          (unsigned int *total_nodes);

/*
 * Update the cache of nodes in ~/.giFT/Gnutella/nodes
 */
void      gt_node_update_cache   (void);

/*****************************************************************************/

#endif /* __GT_NODE_H__ */
