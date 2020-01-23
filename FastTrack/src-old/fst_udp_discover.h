/*
 * $Id: fst_udp_discover.h,v 1.4 2004/01/02 21:50:27 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_UDP_DISCOVER_H
#define __FST_UDP_DISCOVER_H

#include "fst_node.h"
#include "fst_packet.h"
#include "fst_crypt.h"

/*****************************************************************************/

/*
 * After creation a FSTUdpDiscover object will continually retrieve nodes from
 * the passed nodecache and ping them. Nodes which respond are kept and a
 * callback is raised. Dead nodes are removed.
 *
 * TODO: use udp support from libgift 0.12.x
 */

/*****************************************************************************/

/* udp packet types */
typedef enum
{
	UdpMsgPing  = 0x27,
	UdpMsgPong  = 0x28,
	UdpMsgPong2 = 0x29

} FSTUdpMsg;


/* udp node */

typedef enum
{
	UdpNodeStateNew,
	UdpNodeStatePinged,
	UdpNodeStateUp

} FSTUdpNodeState;

typedef struct 
{
	in_addr_t ip;
	in_port_t port;

	FSTUdpNodeState state;

	unsigned int last_seen;		/* time in seconds since the epoch nodes was
	                             * last seen
	                             */

	unsigned int min_enc_type;
	char *network;
} FSTUdpNode;


/* udp discover */

typedef struct _FSTUdpDiscover FSTUdpDiscover;

/* 
 * if udp_node is NULL we ran out of nodes to try.
 * if callback returns FALSE discover has been freed by callback.
 */
typedef int (*FSTUdpDiscoverCallback) (FSTUdpDiscover *discover,
                                       FSTNode *node);

struct _FSTUdpDiscover
{
	int fd;						/* udp socket used for communication */

	List *nodes;				/* the list we store currently pinged nodes in */
	int pinged_nodes;			/* number of outstanding ping replies */
	FSTNodeCache *cache;		/* the cache we get our nodes to try from */

	FSTUdpDiscoverCallback callback;
	timer_id timer;				/* timer which times out pings */
};

/*****************************************************************************/

/* allocate and init udp node */
FSTUdpNode* fst_udp_node_create (in_addr_t ip, in_addr_t port);

/* free udp_node */
void fst_udp_node_free (FSTUdpNode* udp_node);

/*****************************************************************************/

/* init discover object and start search */
FSTUdpDiscover *fst_udp_discover_create (FSTUdpDiscoverCallback callback,
                                         FSTNodeCache *cache);

/*
 * free discover and if save_nodes is TRUE writes back all discovered nodes to
 * discover->cache;
 */
void fst_udp_discover_free (FSTUdpDiscover *discover, int save_nodes);

/* 
 * get discovered node and remove it from list.
 * caller frees returned node
 */
FSTNode *fst_udp_discover_get_node (FSTUdpDiscover *discover);

/*****************************************************************************/

#endif /* __FST_UDP_DISCOVER_H */
