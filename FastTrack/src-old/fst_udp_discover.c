/*
 * $Id: fst_udp_discover.c,v 1.7 2004/01/04 17:24:01 mkern Exp $
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

/*****************************************************************************/

#define UDP_BUFFER_LEN 1024 /* length of udp packet buffer */

/*****************************************************************************/

static void udp_discover_receive (int fd, input_id input,
                                  FSTUdpDiscover *discover);
static int udp_discover_timeout (FSTUdpDiscover *discover);
static int udp_discover_ping_nodes (FSTUdpDiscover *discover, int max_pings);
static int udp_discover_send_ping (FSTUdpDiscover *discover, FSTNode *node);

static int udpsock_bind (in_port_t port, int blocking);

/*****************************************************************************/

FSTUdpNode* fst_udp_node_create (in_addr_t ip, in_addr_t port)
{
	FSTUdpNode *udp_node = malloc (sizeof (FSTUdpNode));

	if (!udp_node)
		return NULL;

	udp_node->ip = ip;
	udp_node->port = port;
	udp_node->state = UdpNodeStateNew;
	udp_node->last_seen = 0;
	udp_node->min_enc_type = 0x00;
	udp_node->network = NULL;

	return udp_node;
}

void fst_udp_node_free (FSTUdpNode* udp_node)
{
	if (!udp_node)
		return;

	free (udp_node->network);
	free (udp_node);
}

/*****************************************************************************/

FSTUdpDiscover *fst_udp_discover_create (FSTUdpDiscoverCallback callback,
                                         FSTNodeCache *cache)
{
	FSTUdpDiscover *discover = malloc (sizeof (FSTUdpDiscover));
	in_port_t port;

	if (!discover)
		return NULL;

	discover->nodes = NULL;
	discover->pinged_nodes = 0;
	discover->cache = cache;
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

	/* create timer for timeout */
	discover->timer = timer_add (FST_UDP_DISCOVER_TIMEOUT,
	                             (TimerCallback)udp_discover_timeout,
	                             discover);

	if (!discover->timer)
	{
		fst_udp_discover_free (discover, TRUE);
		FST_ERR ("timer init failed");
		return NULL;
	}

	/* wait for responses */
	input_add (discover->fd, (void*) discover, INPUT_READ,
			   (InputCallback) udp_discover_receive, 0);


	/* sort the cache so we start with the best nodes */
	fst_nodecache_sort (discover->cache);

	if (udp_discover_ping_nodes (discover, 0) == -2)
	{
		/* callback freed us */
		return NULL;
	}

	return discover;
}


/* free udp node and optionally save it back to cache */
static int udp_discover_free_node (FSTUdpNode *udp_node,
                                   FSTNodeCache *cache)
{
	if (cache)
	{
		unsigned int load = 0;
		fst_nodecache_add (cache, NodeKlassSuper, net_ip_str (udp_node->ip),
		                   udp_node->port, load, udp_node->last_seen);
		FST_HEAVY_DBG_2 ("added node %s:%d back into cache",
		                 net_ip_str (udp_node->ip), udp_node->port);
	}
	
	fst_udp_node_free (udp_node);
	return TRUE;
}

void fst_udp_discover_free (FSTUdpDiscover *discover, int save_nodes)
{
	if (!discover)
		return;

	/* remove all inputs */
	timer_remove (discover->timer);
	input_remove_all (discover->fd);
	net_close (discover->fd);
	discover->fd = -1;

	if (save_nodes)
	{
		list_foreach_remove (discover->nodes,
		                     (ListForeachFunc)udp_discover_free_node,
	                         discover->cache);
		/* sort cache again */
		fst_nodecache_sort (discover->cache);
	}
	else
	{
		list_foreach_remove (discover->nodes,
		                     (ListForeachFunc)udp_discover_free_node,
	                         NULL);
	}

	free (discover);
}


FSTNode *fst_udp_discover_get_node (FSTUdpDiscover *discover)
{
	List *item;
	FSTNode *node;
	FSTUdpNode *udp_node;

	if (!discover)
		return NULL;

	/* find the first responsive node and return it */
	for (item=discover->nodes; item; item=item->next)
	{
		udp_node = (FSTUdpNode*)item->data;

		if (udp_node->state == UdpNodeStateUp)
			break;
	}

	if (!item)
		return NULL;

	discover->nodes = list_remove_link(discover->nodes, item);	
	
	node = fst_node_create (NodeKlassSuper, net_ip_str (udp_node->ip),
	                        udp_node->port, 0, udp_node->last_seen);

	fst_udp_node_free (udp_node);
	
	if (!node)
		return NULL;

	return node;
}

/*****************************************************************************/

static void udp_discover_receive (int fd, input_id input,
                                  FSTUdpDiscover *discover)
{
	FSTPacket *packet;
	FSTUdpNode *udp_node = NULL;
	List *udp_node_link = NULL;
	FSTNode *node;
	unsigned char buf[UDP_BUFFER_LEN];
	struct sockaddr_in addr;
	int addr_len = sizeof(addr);
	int len, type;
	
	if (net_sock_error (fd))
	{
		/* this should never happen */
		FST_ERR ("udp_discover_receive called with invalid fd");
		/* TODO: do something more clever than aborting */
		assert(net_sock_error (fd) == FALSE);
		return;
	}

	if ((len = recvfrom (fd, buf, UDP_BUFFER_LEN, 0, (struct sockaddr*)&addr, &addr_len)) < 0)
	{
#ifdef WIN32
		if (WSAGetLastError () == WSAEMSGSIZE)
		{
			FST_DBG_1 ("udp_discover_receive: packet larger than %d bytes",
			           UDP_BUFFER_LEN);
			len = UDP_BUFFER_LEN;			
		}
		else
		{
			FST_ERR_1 ("udp_discover_receive: recvfrom failed: %d", WSAGetLastError ());
			return;
		}
#else
		FST_ERR ("udp_discover_receive: recvfrom failed");
		return;
#endif
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
		
		/* update udp_node with the new data */
		udp_node->min_enc_type = ntohl (fst_packet_get_uint32 (packet));
		fst_packet_get_uint8 (packet); /* unknown (0x00) */
		fst_packet_get_uint8 (packet); /* unknown */
		fst_packet_get_uint32 (packet); /* unknown */

		if((len = fst_packet_strlen (packet, 0x00)) < 0)
		{
			FST_DBG_2 ("received corrupted udp reply from %s:%d",
						net_ip_str (udp_node->ip), udp_node->port);
			/* remove udp_node from list */
			discover->nodes = list_remove_link(discover->nodes, udp_node_link);
			fst_udp_node_free (udp_node);
			fst_packet_free (packet);
			return;
		}

		udp_node->network = fst_packet_get_ustr (packet, len+1);
		fst_packet_free (packet);
	
		udp_node->last_seen = time (NULL);
		udp_node->state = UdpNodeStateUp;
		discover->pinged_nodes--;

		/* notify plugin using FSTNode */
		node = fst_node_create (NodeKlassSuper, net_ip_str (udp_node->ip),
		                        udp_node->port, 0, udp_node->last_seen);

		if (!node)
		{
			discover->nodes = list_remove_link(discover->nodes, udp_node_link);
			fst_udp_node_free (udp_node);
			return;
		}

		FST_DBG_4 ("received udp reply 0x%02x (pong) from %s:%d, pinged nodes: %d",
		           type, net_ip_str (udp_node->ip), udp_node->port,
		           discover->pinged_nodes);

		/* callback may free us */
		discover->callback (discover, node);
	
		/* don't send another ping right now since this was a pong */
		fst_node_free (node);
		
		return;

	default: /* unknown packet */

		fst_packet_free (packet);

		FST_DBG_4 ("received udp reply 0x%02x from %s:%d, pinged nodes: %d",
					type, net_ip_str (udp_node->ip), udp_node->port,
					discover->pinged_nodes);
		
		/* remove udp_node from list */
		discover->nodes = list_remove_link(discover->nodes, udp_node_link);
		
		/* add this node back to cache with high load */
		fst_nodecache_add (discover->cache, NodeKlassSuper,
		                   net_ip_str (udp_node->ip), udp_node->port,
		                   100, udp_node->last_seen);

		fst_udp_node_free (udp_node);

		/* callback may free us */
		udp_discover_ping_nodes (discover, 1);
	}

	/* wait for next packet */
	return;
}

static int udp_discover_timeout (FSTUdpDiscover *discover)
{
	List *item;
	unsigned int now = time (NULL);

	FST_HEAVY_DBG ("timer raised");

	/* check all pending udp nodes for timeout */
	for (item=discover->nodes; item; item=item->next)
	{
		FSTUdpNode *udp_node = (FSTUdpNode*)item->data;

		if (udp_node->state != UdpNodeStatePinged)
			continue;

		if (udp_node->last_seen + (FST_UDP_DISCOVER_TIMEOUT/SECONDS) > now)
			continue;

		/* the node timed out, remove it */
		discover->nodes = list_remove_link(discover->nodes, item);	
		discover->pinged_nodes--;

		FST_HEAVY_DBG_3 ("removed timed out udp node %s:%d, pinged nodes: %d",
		                 net_ip_str (udp_node->ip), udp_node->port,
						 discover->pinged_nodes);

		fst_udp_node_free (udp_node);

		/* restart iteration at beginning of list */
		item = discover->nodes;
	}
	
	if (udp_discover_ping_nodes (discover, 0) == -2)
	{
		/* callback freed us */
		return FALSE;
	}
	
	/* raise us again after FST_UDP_DISCOVER_TIMEOUT */
	return TRUE;
}

/*****************************************************************************/

/*
 * sends enough pings so FST_UDP_DISCOVER_MAX_PINGS is reached
 * returns number of currently pinged nodes, -1 if network is down and -2 if
 * callback freed us.
 */
static int udp_discover_ping_nodes (FSTUdpDiscover *discover, int max_pings)
{
	if (max_pings == 0)
		max_pings = FST_UDP_DISCOVER_MAX_PINGS;
	else
		max_pings = discover->pinged_nodes + max_pings;	

	while (discover->pinged_nodes < max_pings)
	{
		FSTNode *node = fst_nodecache_get_front (discover->cache);
		
		if(!node)
			break;

		if (udp_discover_send_ping (discover, node) == -1)
		{
			/* network is down, let the timer try again later */
			FST_WARN_1 ("no route to the internet, trying again in %d seconds",
			            FST_UDP_DISCOVER_TIMEOUT / SECONDS);
			fst_node_free (node);
			return -1;
		}

		/* remove this node from cache */
		fst_nodecache_remove (discover->cache, node->host);
		fst_node_free (node);
	}

	if (discover->pinged_nodes == 0)
	{
		/* notify plugin via callback */
		if (!discover->callback (discover, NULL))
		{
			/* callback freed us */
			return -2;
		}
	}

	return discover->pinged_nodes;
}


/*
 * pings node via udp
 * returns 0 on success, -1 if network is down and 1 if something else fails
 */
static int udp_discover_send_ping (FSTUdpDiscover *discover, FSTNode *node)
{
	struct hostent *he;
	FSTUdpNode *udp_node;
	in_addr_t ip;
	FSTPacket *packet;
	struct sockaddr_in addr;

	if (!discover || !node)
		return 1;

	/* TODO: make this non-blocking */
	if (! (he = gethostbyname (node->host)))
	{
		FST_WARN_1 ("udp_discover_send_ping: gethostbyname failed for host %s",
		            node->host);
		return 1;
	}

	ip = *((in_addr_t*)he->h_addr_list[0]);

	FST_HEAVY_DBG_3 ("sending udp ping to %s(%s):%d", node->host,
	                 net_ip_str(ip), node->port);

	/* create udp_node */
	if (!(udp_node = fst_udp_node_create (ip, node->port)))
		return 1;

	/* create packet */
	if (!(packet = fst_packet_create ()))
	{
		fst_udp_node_free (udp_node);
		return 1;
	}

	fst_packet_put_uint8 (packet, UdpMsgPing);
	fst_packet_put_uint32 (packet, htonl (0x29)); /* minimum enc_type */
	fst_packet_put_uint8 (packet, 0x80); /* unknown */
	fst_packet_put_ustr (packet, FST_NETWORK_NAME, strlen (FST_NETWORK_NAME) + 1);

	/* send packet */

	memset (&addr, 0, sizeof (addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = udp_node->ip;
	addr.sin_port        = htons (udp_node->port);

	if(sendto (discover->fd, packet->data, packet->used, 0, (struct sockaddr*)&addr, sizeof (addr)) != packet->used)
	{
		FST_DBG_2 ("sendto failed for %s:%d",
		           net_ip_str (udp_node->ip), udp_node->port);
		fst_udp_node_free (udp_node);
		fst_packet_free (packet);
		return -1;
	}

	fst_packet_free (packet);
	
	udp_node->state = UdpNodeStatePinged;
	udp_node->last_seen = time (NULL);

	/* add node to list */
	discover->nodes = list_append (discover->nodes, udp_node);
	discover->pinged_nodes++;

	FST_DBG_3 ("sent udp ping to %s:%d, pinged nodes: %d",
	           net_ip_str (udp_node->ip), udp_node->port,
	           discover->pinged_nodes);

	return 0;
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

