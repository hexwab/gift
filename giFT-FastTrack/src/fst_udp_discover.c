/*
 * $Id: fst_udp_discover.c,v 1.25 2004/11/10 20:00:57 mkern Exp $
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

#include "fst_fasttrack.h"
#include "fst_udp_discover.h"

/*
 * TODO: use udp support from libgift 0.12.x
 */

/*****************************************************************************/

#define UDP_BUFFER_LEN 1024 /* length of udp packet buffer */

/*****************************************************************************/

/* udp packet types */
typedef enum
{
	UdpMsgPing  = 0x27,
	UdpMsgPong  = 0x28,
	UdpMsgPong2 = 0x29
} FSTUdpMsg;

typedef struct 
{
	FSTNode *node;				/* a copy of the node we ping */
	in_addr_t ip;				/* the resolved node->host */

	unsigned int sent_time;		/* time in seconds since the epoch this
	                             * ping was sent */

	unsigned int min_enc_type;
	char *network;
} FSTUdpNode;

/*****************************************************************************/

static void udp_discover_receive (int fd, input_id input,
                                  FSTUdpDiscover *discover);
static int udp_discover_timeout (FSTUdpDiscover *discover);
static int udpsock_bind (in_port_t port, int blocking);

/*****************************************************************************/

static FSTUdpNode* fst_udp_node_create (FSTNode *node)
{
	FSTUdpNode *udp_node;

	if (!(udp_node = malloc (sizeof (FSTUdpNode))))
		return NULL;

	if (node)
	{
		fst_node_addref (node);
		udp_node->node = node;
	}
	else
		udp_node->node = NULL;

	udp_node->ip = 0;
	udp_node->sent_time = 0;
	udp_node->min_enc_type = 0x00;
	udp_node->network = NULL;

	return udp_node;
}

static void fst_udp_node_free (FSTUdpNode* udp_node)
{
	if (!udp_node)
		return;

	fst_node_release (udp_node->node);
	free (udp_node->network);
	free (udp_node);
}

/*****************************************************************************/

FSTUdpDiscover *fst_udp_discover_create (FSTUdpDiscoverCallback callback)
{
	FSTUdpDiscover *discover;
	in_port_t port;

	if (!callback)
		return NULL;

	if (!(discover = malloc (sizeof (FSTUdpDiscover))))
		return NULL;

	discover->nodes = NULL;
	discover->pinged_nodes = 0;
	discover->udp_working = FALSE;
	discover->sent_pings = 0;
	discover->received_pongs = 0;
	discover->received_others = 0;
	discover->callback = callback;
	discover->timer = 0;

	if ((port = config_get_int (FST_PLUGIN->conf, "main/port=0")) == 0)
		port = 1214;

	/* bind socket */
	if ((discover->fd = udpsock_bind (port, FALSE)) < 0)
	{
		free (discover);
		FST_DBG ("binding UDP sock for discovery failed");
		return NULL;
	}

	/* wait for responses */
	input_add (discover->fd, (void*) discover, INPUT_READ,
			   (InputCallback) udp_discover_receive, 0);

	return discover;
}

/* free udp node list entry */
static int udp_discover_free_node (FSTUdpNode *udp_node, void *udata)
{
	fst_udp_node_free (udp_node);
	return TRUE;
}

void fst_udp_discover_free (FSTUdpDiscover *discover)
{
	if (!discover)
		return;

	/* remove all inputs */
	timer_remove (discover->timer);
	input_remove_all (discover->fd);
	net_close (discover->fd);
	discover->fd = -1;

	list_foreach_remove (discover->nodes,
		                 (ListForeachFunc)udp_discover_free_node,
	                     NULL);

	free (discover);
}


int fst_udp_discover_ping_node (FSTUdpDiscover *discover, FSTNode *node)
{
	FSTUdpNode *udp_node;
	FSTPacket *packet;
	struct sockaddr_in addr;

	if (!discover || !node)
		return FALSE;

	/* create udp_node */
	if (!(udp_node = fst_udp_node_create (node)))
		return FALSE;

	if ((udp_node->ip = net_ip (node->host)) == INADDR_NONE)
	{
		struct hostent *he;

		/* TODO: make this non-blocking */
		if (! (he = gethostbyname (node->host)))
		{
			fst_udp_node_free (udp_node);
			FST_WARN_1 ("fst_udp_discover_ping_node: gethostbyname failed for host %s",
			            node->host);
			return FALSE;
		}

		/* hmm */
		udp_node->ip = *((in_addr_t*)he->h_addr_list[0]);
	}

	FST_HEAVY_DBG_3 ("sending udp ping to %s(%s):%d", udp_node->node->host,
	                 net_ip_str(udp_node->ip), udp_node->node->port);

	/* create packet */
	if (!(packet = fst_packet_create ()))
	{
		fst_udp_node_free (udp_node);
		return FALSE;
	}

	fst_packet_put_uint8 (packet, UdpMsgPing);
	fst_packet_put_uint32 (packet, htonl (0x29)); /* minimum enc_type */
	fst_packet_put_uint8 (packet, 0x80);          /* unknown */
	fst_packet_put_ustr (packet, FST_NETWORK_NAME, strlen (FST_NETWORK_NAME) + 1);

	/* send packet */
	memset (&addr, 0, sizeof (addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = udp_node->ip;
	addr.sin_port        = htons (udp_node->node->port);

	if(sendto (discover->fd, packet->data, packet->used, 0, (struct sockaddr*)&addr, sizeof (addr)) != packet->used)
	{
		FST_DBG_2 ("sendto failed for %s:%d",
		           net_ip_str(udp_node->ip), udp_node->node->port);
		fst_udp_node_free (udp_node);
		fst_packet_free (packet);

		/* TODO: signal network down to caller */
		return FALSE;
	}

	fst_packet_free (packet);

	/* create timer for ping timeout if necessary */
	if (!discover->timer)
	{
		discover->timer = timer_add (FST_UDP_DISCOVER_TIMEOUT,
		                             (TimerCallback)udp_discover_timeout,
		                             discover);

		if (!discover->timer)
		{
			fst_udp_node_free (udp_node);
			FST_ERR ("timer init failed");
			return FALSE;
		}
	}

	udp_node->sent_time = time (NULL);

	/* add node to list */
	discover->nodes = list_append (discover->nodes, udp_node);
	discover->pinged_nodes++;
	discover->sent_pings++;

	FST_HEAVY_DBG_3 ("sent udp ping to %s:%d, pinged nodes: %d",
	                 net_ip_str (udp_node->ip), udp_node->node->port,
	                 discover->pinged_nodes);

	return TRUE;
}

/*****************************************************************************/

static void udp_discover_receive (int fd, input_id input,
                                  FSTUdpDiscover *discover)
{
	FSTPacket *packet;
	FSTUdpNode *udp_node = NULL;
	List *udp_node_link = NULL;
	unsigned char buf[UDP_BUFFER_LEN];
	struct sockaddr_in addr;
	int addr_len = sizeof(addr);
	int len, type;
	
	if (net_sock_error (fd))
	{
		/* this should never happen for an udp socket */
		FST_ERR ("udp_discover_receive called with invalid fd");
		return;
	}

	/* TODO: check for ICMP host unreachable and friends */
	if ((len = recvfrom (fd, buf, UDP_BUFFER_LEN, 0, (struct sockaddr*)&addr, &addr_len)) < 0)
	{
#ifdef WIN32
		FST_ERR_1 ("udp_discover_receive: recvfrom failed: %d", WSAGetLastError ());
#else
		FST_ERR ("udp_discover_receive: recvfrom failed");
#endif
		return;
	}

	/* find this udp node in list */
	for (udp_node_link=discover->nodes; udp_node_link; udp_node_link=udp_node_link->next)
	{
		if (((FSTUdpNode*)udp_node_link->data)->ip == addr.sin_addr.s_addr)
		{
			udp_node = udp_node_link->data;
			break;
		}
	}
	
	if (!udp_node)
	{
		FST_DBG_2 ("recevied udp reply from node %s:%d which is not in list",
		           net_ip_str (addr.sin_addr.s_addr), ntohs (addr.sin_port));
		return;
	}

	/* remove udp_node from list */
	discover->nodes = list_remove_link(discover->nodes, udp_node_link);

	/* we got _some_ udp packet so udp is not firewalled */
	discover->udp_working = TRUE;

	/* create packet */
	if (!(packet = fst_packet_create ()))
		return;

	fst_packet_put_ustr (packet, buf, len);
	fst_packet_rewind (packet);

	/* parse packet */
	type = fst_packet_get_uint8 (packet);

	switch (type)
	{
	case UdpMsgPong:
	{
		fst_uint8 unknown1;
		fst_uint8 time_inc;
		fst_uint8 outdegree; /* unconfirmed, confidence < 10% */
		fst_uint8 children;  /* unconfirmed, confidence < 30% */
		fst_uint8 unknown2;
		
		/* update udp_node with the new data */
		udp_node->min_enc_type = ntohl (fst_packet_get_uint32 (packet));

		unknown1  = fst_packet_get_uint8 (packet); /* unknown (0x00) */
		time_inc  = fst_packet_get_uint8 (packet); /* 30 minute increment */
		outdegree = fst_packet_get_uint8 (packet); /* supernode outdegree? */
		children  = fst_packet_get_uint8 (packet); /* supernode children? */

		/* load */
		udp_node->node->load = fst_packet_get_uint8 (packet);

		unknown2 =	fst_packet_get_uint8 (packet); /* unknown (signed?) */

#if 0
		FST_HEAVY_DBG_5 ("udp data: unk1: 0x%02x, time_inc: 0x%02x, outdegree: %d, children: %d, unk2: %d",
		                 unknown1, time_inc, outdegree, children, unknown2);

#endif

		if((len = fst_packet_strlen (packet, 0x00)) < 0)
		{
			FST_DBG_2 ("received corrupted udp reply from %s:%d",
						net_ip_str (udp_node->ip), udp_node->node->port);
			fst_udp_node_free (udp_node);
			fst_packet_free (packet);
			/* let the ping time out */
			return;
		}

		udp_node->network = fst_packet_get_ustr (packet, len+1);
		fst_packet_free (packet);
	
		/* this node is up and has free child slots */
		udp_node->node->last_seen = time (NULL);
		discover->pinged_nodes--;
		discover->received_pongs++;

		FST_HEAVY_DBG_4 ("received udp reply 0x%02x (pong) from %s:%d, load: %d%%",
		                 type, net_ip_str (udp_node->ip), udp_node->node->port,
		                 udp_node->node->load);

		/* raise callback */
		discover->callback (discover, UdpNodeStateFree, udp_node->node);
	
		fst_udp_node_free (udp_node);
		break;
	}

	case UdpMsgPong2:
		
		/* update udp_node with the new data */
		udp_node->min_enc_type = ntohl (fst_packet_get_uint32 (packet));
		fst_packet_free (packet);
	
		/* this node is up and has free child slots */
		udp_node->node->last_seen = time (NULL);
		discover->pinged_nodes--;
		discover->received_pongs++;

		FST_HEAVY_DBG_4 ("received udp reply 0x%02x (pong2) from %s:%d, pinged nodes: %d",
		                 type, net_ip_str (udp_node->ip), udp_node->node->port,
		                 discover->pinged_nodes);

		/* raise callback */
		discover->callback (discover, UdpNodeStateFree, udp_node->node);
	
		fst_udp_node_free (udp_node);
		break;

	default: /* unknown packet */

		fst_packet_free (packet);

		/* this node is up but has no child slot for us */
		udp_node->node->last_seen = time (NULL);
		discover->pinged_nodes--;
		discover->received_others++;

		FST_HEAVY_DBG_4 ("received udp reply 0x%02x from %s:%d, pinged nodes: %d",
		                 type, net_ip_str (udp_node->ip), udp_node->node->port,
					     discover->pinged_nodes);

		/* raise callback */
		discover->callback (discover, UdpNodeStateUp, udp_node->node);

		fst_udp_node_free (udp_node);
		break;
	}

	assert (discover->pinged_nodes >= 0);

	/* remove timer if no more outstanding pings */
	if (discover->pinged_nodes == 0)
	{
		/* remove timer */
		timer_remove (discover->timer);
		discover->timer = 0;
		return;
	}

	/* wait for next packet */
	return;
}

static int udp_discover_timeout (FSTUdpDiscover *discover)
{
	List *item, *next;
	unsigned int now = time (NULL);

	FST_HEAVY_DBG ("timer raised");

	/* check all pending udp nodes for timeout */
	for (item=discover->nodes; item; item=next)
	{
		FSTUdpNode *udp_node = (FSTUdpNode*)item->data;

		next = item->next;
		
		if (udp_node->sent_time + (FST_UDP_DISCOVER_TIMEOUT/SECONDS) > now)
			continue;

		/* the node timed out, remove it */
		discover->nodes = list_remove_link(discover->nodes, item);	
		discover->pinged_nodes--;

		FST_HEAVY_DBG_3 ("removed timed out udp node %s:%d, pinged nodes: %d",
		                 net_ip_str (udp_node->ip), udp_node->node->port,
						 discover->pinged_nodes);

		/* raise callback */
		discover->callback (discover, UdpNodeStateDown, udp_node->node);

		fst_udp_node_free (udp_node);
	}

	assert (discover->pinged_nodes >= 0);

	/* remove timer if no more outstanding pings */
	if (discover->pinged_nodes == 0)
	{
		/* remove timer */
		discover->timer = 0;
		return FALSE;
	}
	
	/* raise us again after FST_UDP_DISCOVER_TIMEOUT */
	return TRUE;
}

/*****************************************************************************/

static int udpsock_bind (in_port_t port, int blocking)
{
	int fd;
	struct sockaddr_in server;
	int opt;

	assert (port > 0);

	if ((fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		return fd;

	memset (&server, 0, sizeof (server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port        = htons (port);

	/* was opt = sizeof (server)...is this change correct? */
	opt = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
	net_set_blocking (fd, blocking);

	if (bind (fd, (struct sockaddr*)&server, sizeof (server)) < 0)
	{
		net_close (fd);
		return -1;
	}

	listen (fd, 5);

	return fd;
}

/*****************************************************************************/

