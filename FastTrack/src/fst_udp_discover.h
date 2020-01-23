/*
 * $Id: fst_udp_discover.h,v 1.13 2004/03/08 21:09:57 mkern Exp $
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

/* udp node */
typedef enum
{
	UdpNodeStateDown,		/* not could not be reached */
	UdpNodeStateUp,			/* node is up but doesn't want to be parent */
	UdpNodeStateFree		/* node is up and can be our parent */
} FSTUdpNodeState;


/* udp discover */
typedef struct _FSTUdpDiscover FSTUdpDiscover;

/* 
 * if udp_node is NULL we ran out of nodes to try.
 * if callback returns FALSE discover has been freed by callback.
 */
typedef void (*FSTUdpDiscoverCallback) (FSTUdpDiscover *discover,
									    FSTUdpNodeState node_state,
                                        FSTNode *node);

struct _FSTUdpDiscover
{
	int fd;						/* udp socket used for communication */

	List *nodes;				/* the list we store currently pinged nodes in */
	int pinged_nodes;			/* number of outstanding ping replies */

	int udp_working;			/* initially 0, set to 1 upon reception of
								 * first udp packet */

	/* some statistics */
	unsigned int sent_pings;      /* total number of pings we sent out */
	unsigned int received_pongs;  /* total number of received pongs */
	unsigned int received_others; /* total of other received responses */

	FSTUdpDiscoverCallback callback;
	timer_id timer;				/* timer which times out pings */
};

/*****************************************************************************/

/* init discover object */
FSTUdpDiscover *fst_udp_discover_create (FSTUdpDiscoverCallback callback);

/* free discover object */
void fst_udp_discover_free (FSTUdpDiscover *discover);

/* ping node an return result via callback. */
int fst_udp_discover_ping_node (FSTUdpDiscover *discover, FSTNode *node);

/*****************************************************************************/

#endif /* __FST_UDP_DISCOVER_H */
