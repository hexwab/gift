/*
 * $Id: gt_node.h,v 1.36 2005/01/04 15:00:51 mkern Exp $
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

#ifndef GIFT_GT_NODE_H_
#define GIFT_GT_NODE_H_

/*****************************************************************************/

#define MAX_NODES               gt_config_get_int("main/max_nodes=2000")

/*****************************************************************************/

struct gt_query_router;
struct gt_packet;
struct gt_vendor_msg;
struct gt_rx_stack;
struct gt_tx_stack;

typedef enum gt_node_state
{
	GT_NODE_DISCONNECTED  = 0x00, /* functionless node */
	GT_NODE_CONNECTING_1  = 0x01, /* pending */
	GT_NODE_CONNECTING_2  = 0x02, /* waiting for first ping response */
	GT_NODE_CONNECTED     = 0x08, /* first packet is seen */
	GT_NODE_ANY           = 0xFF, /* any state */
} gt_node_state_t;

typedef enum gt_node_class
{
	GT_NODE_NONE   = 0x00,
	GT_NODE_LEAF   = 0x01,        /* plain 0.6 or 0.4 nodes */
	GT_NODE_ULTRA  = 0x02,        /* ultrapeers */
	GT_NODE_DEAD   = 0x04,        /* node is marked for deletion (unused) */
} gt_node_class_t;

typedef struct gt_node
{
	in_addr_t       ip;

	/* the gnutella port of the other side */
	in_port_t       gt_port;

	/* the port the other side came from, could be the same as gt_port */
	in_port_t       peer_port;

	/* IP address used in communication with this node */
	in_addr_t       my_ip;

	/* HTTP headers the other node sent on 0.6 connection in stage-2 of
	 * the 3-way handshake */
	Dataset        *hdr;

	/* Contains all the vendor messages supported by this node */
	Dataset        *vmsgs_supported;

	unsigned int    incoming      : 1;    /* incoming connection */
	unsigned int    verified      : 1;    /* port has been verified */
	unsigned int    firewalled    : 1;    /* firewalled connection */
	unsigned int    tried_connect : 1;    /* used internally by gt_netorg.c */
	unsigned int    rx_inflated   : 1;    /* incoming traffic compressed */
	unsigned int    tx_deflated   : 1;    /* outgoing traffic compressed */
	unsigned int    vmsgs_sent    : 1;    /* sent our initial batch of vmsgs */

	/* current state of the given connection */
	gt_node_state_t state;

	/* node classification that this connection is communicating with */
	gt_node_class_t klass;

	/* TCPC a node uses. could be null */
	TCPC           *c;

	/* consecutive number of pings the host has not replied to */
	unsigned int    pings_with_noreply;

	/* push proxy address, which may be different from peer address if remote
	 * end is multi-homed */
	in_addr_t       push_proxy_ip;
	in_port_t       push_proxy_port;

	/* Data source for packets being read in */
	struct gt_rx_stack      *rx_stack;
	/* Data source for packets being sent out */
	struct gt_tx_stack      *tx_stack;

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

	/* status of shares submitted to this node */
	struct gt_share_state    *share_state;

	/* router for query packets */
	struct gt_query_router   *query_router;

	/* version of the query router table submitted to this node currently */
	int                       query_router_counter;
} GtNode;

/*****************************************************************************/

#define GT_NODE(c)         ((GtNode *)c->udata)
#define GT_CONN(node)      ((TCPC *) node->c)

/*****************************************************************************/

GtNode       *gt_node_new         (void);
GtNode       *gt_node_instantiate (TCPC *c);
void          gt_node_free        (GtNode *node);
BOOL          gt_node_freeable    (GtNode *node);

char         *gt_node_str         (GtNode *node);
void          gt_node_connect     (GtNode *node, TCPC *c);
void          gt_node_disconnect  (TCPC *c);
void          gt_node_error       (TCPC *c, const char *fmt, ...);

void          gt_node_remove_all  (void);

/*****************************************************************************/

void          gt_node_state_set   (GtNode *node, gt_node_state_t state);
void          gt_node_class_set   (GtNode *node, gt_node_class_t klass);

char         *gt_node_class_str   (gt_node_class_t klass);
char         *gt_node_state_str   (gt_node_state_t state);

/*****************************************************************************/

GtNode       *gt_node_lookup      (in_addr_t ip, in_port_t port);
GtNode       *gt_node_register    (in_addr_t ip, in_port_t port,
                                   gt_node_class_t klass);

/*****************************************************************************/

BOOL          gt_node_send_if_supported (GtNode *node, struct gt_packet *pkt);
BOOL          gt_node_send              (GtNode *node, struct gt_packet *pkt);

/*****************************************************************************/

#endif /* GIFT_GT_NODE_H_ */
