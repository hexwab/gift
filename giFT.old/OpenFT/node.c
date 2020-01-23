/*
 * node.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include <stdarg.h> /* va lists */
#include <time.h>   /* time ()  */

#include "openft.h"

#include "netorg.h"

#include "queue.h"
#include "nb.h"

#include "http.h"
#include "html.h"

/*****************************************************************************/

/**/extern Connection *ft_self;

/*****************************************************************************/

/* 10 minutes heartbeat timeout */
#define MAX_HEARTBEAT 5

/*****************************************************************************/

/*
 * main node communication loop
 *
 * handle reading of packets from a node connection (at this point
 * indiscriminant as to whether it is a search or user node)
 */
void ft_node_connection (Protocol *p, Connection *c)
{
	NBRead *nb;
	unsigned short len;
	int n;

	/* grab a data buffer */
	nb = nb_active (c->fd);

	/* ok this nasty little hack here is executed so that we can maintain
	 * a single command buffer for len/command and the linear data
	 * stream to follow */
	len = nb->flag + FT_HEADER_LEN;

	n = nb_read (nb, c->fd, len, NULL);
	if (n <= 0)
	{
		nb_finish (nb);
		node_disconnect (c);
		return;
	}

	/* we are connected if we got here ... */
	node_state_set (c, NODE_CONNECTED);
	NODE (c)->vitality = time (NULL);

	/* we successfully read a portion of the current packet, but not all of
	 * it ... come back when we have more data */
	if (!nb->term)
		return;

	/* packet length (nb->len is gauranteed > 2 here) */
	len = ntohs (*((ft_uint16 *)nb->data));

	if (nb->flag || len == 0) /* we read a complete packet */
	{
		FTPacket *packet;
		int       ret;

		/* this code is totally bastardized. */
		packet = ft_packet_new (ntohs (*((ft_uint16 *)(nb->data + 2))),
		                        nb->data + FT_HEADER_LEN, len);

		ret = protocol_handle_command (p, c, packet);

		ft_packet_free (packet);

		/* we're done with this scratch buffer, get rid of it */
		nb_finish (nb);

		/* TODO - should we do this? */
		if (!ret)
		{
			TRACE_SOCK (("invalid packet, disconnecting"));
			node_disconnect (c);
			return;
		}
	}
	else if (!nb->flag) /* we read a header block with non-zero length waiting
						 * on the socket */
	{
		nb->term = 0;
		nb->flag = len;
	}
}

/*****************************************************************************/

Connection *node_new (int fd)
{
	Connection        *c;
	Node              *node;
	struct sockaddr_in saddr;
	int                len;

	/* create parent holder */
	c = connection_new (openft_proto);

	/* openft specific data */
	node = malloc (sizeof (Node));
	memset (node, 0, sizeof (Node));

	node->fd = fd;

	/* fill peer info */
	if (fd >= 0 && !node->ip)
	{
		len = sizeof (saddr);
		if (getpeername (fd, (struct sockaddr *) &saddr, &len) == 0)
			node->ip = saddr.sin_addr.s_addr;
	}

	node->incoming = FALSE;
	node->verified = FALSE;

	c->data = node;
	c->fd   = fd;

	return c;
}

static Connection *node_add (int fd)
{
	Connection *c;

	c = node_new (fd);
	conn_add (c);

	return c;
}

static Connection *locate_node (Connection *c, Node *node,
								unsigned long *ip)
{
	if (node->ip == *ip)
		return c;

	return NULL;
}

/* register a new node detected to determine if we should add it to the
 * connection list or whether or not we should connect to it */
Connection *node_register (unsigned long ip, signed short port,
                           signed short http_port, unsigned short klass,
                           int outgoing)
{
	Connection *c;

	if (!ip)
		return NULL;

	/* determine if this data is already in the connection list
	 * TODO - port must match as well */
	c = conn_foreach ((ConnForeachFunc) locate_node, &ip,
	                  NODE_NONE, -1, 0);

	if (!c)
	{
		/* TODO - this api is crap */
		c = node_add (-1);
	}

	node_conn_set (c, ip, port, http_port);

	if (NODE (c)->class == NODE_NONE)
		node_class_set (c, klass);

	/* determine if the outgoing connection should be made */
	if (outgoing)
		ft_connect (c);

	return c;
}

/*****************************************************************************/

void node_free (Connection *c)
{
	free (c->data);
	connection_free (c);
}

void node_remove (Connection *c)
{
	conn_remove (c);

	node_disconnect (c);
	node_free (c);
}

/*****************************************************************************/

static void verify_clear (Connection **verify)
{
	if (*verify)
	{
		net_close ((*verify)->fd);
		input_remove (*verify);
		connection_destroy (*verify);
		*verify = NULL;
	}
}


void node_disconnect (Connection *c)
{
	int l_parent;

	/* flush queue data */
	queue_flush (c);

	/* remove from select and close */
	input_remove (c);
	net_close (c->fd);

#if 0
	GIFT_DEBUG (("removed connection %i from %s:%hu", c->fd,
				 net_ip_str (NODE (c)->ip), NODE (c)->port));
#endif

	/* ensure that this host has no shares to self */
	openft_share_remove_by_host (NODE (c)->ip);

	node_state_set (c, NODE_DISCONNECTED);
	NODE (c)->fd = c->fd = -1;

	/* make sure the call-back connections are dead */
	verify_clear (&(NODE (c)->port_verify));
	verify_clear (&(NODE (c)->http_port_verify));

	/* flush the sent/recv list */
	NODE (c)->sent_list = FALSE;
	NODE (c)->recv_list = FALSE;

	/* reset heartbeat */
	NODE (c)->heartbeat = 0;

	if (NODE (c)->class & NODE_PARENT)
		l_parent = TRUE;

	/* remove all temporary conditions */
	node_class_remove (c, NODE_CHILD);
	node_class_remove (c, NODE_PARENT);

	/* locate a new parent :) */
	if (l_parent && validate_share_submit ())
		openft_share_local_submit (NULL);
}

/*****************************************************************************/

unsigned short node_highest_class (Node *node)
{
	unsigned short high_class;

	if (node->class & NODE_INDEX)
		high_class = NODE_INDEX;
	else if (node->class & NODE_SEARCH)
		high_class = NODE_SEARCH;
	else if (node->class & NODE_USER)
		high_class = NODE_USER;
	else
		high_class = NODE_NONE;

	return high_class;
}

/*****************************************************************************/

static Connection *node_maintain_link (Connection *c, Node *node, FILE *f)
{
	fprintf (f, "%lu %s:%hu %hu\n", node->vitality, net_ip_str (node->ip),
			 node->port, node->http_port);

	return NULL;
}

void node_update_cache ()
{
	FILE *f;

	f = fopen (gift_conf_path ("OpenFT/nodes"), "w");

	if (!f)
		perror ("fopen");
	else
	{
		conn_foreach ((ConnForeachFunc) node_maintain_link, f,
					  NODE_NONE, -1, 0);

		fclose (f);
	}

}

/*****************************************************************************/

static Connection *node_stats (Connection *c, Node *node, void *udata)
{
	ft_packet_send (c, FT_STATS_REQUEST, NULL);

	return NULL;
}

static Connection *handle_heartbeat (Connection *c, Node *node, void *udata)
{
	/* they've reached their maximum */
	if (NODE (c)->heartbeat >= MAX_HEARTBEAT)
	{
		TRACE_SOCK (("heartbeat timeout"));
		node_disconnect (c);
		return NULL;
	}

	NODE (c)->heartbeat++;
	ft_packet_send (c, FT_PING_REQUEST, NULL);

	return NULL;
}

int node_maintain_links (void *udata)
{
	/* send out the OpenFT heartbeat...each time a heartbeat is sent out w/o
	 * a response it will increase a counter that eventually will lead to this
	 * nodes disconnection */
	conn_foreach ((ConnForeachFunc) handle_heartbeat, NULL,
				  NODE_USER, NODE_CONNECTED, 0);

	/* gather stats information */
	conn_foreach ((ConnForeachFunc) node_stats, NULL,
	              NODE_SEARCH, NODE_CONNECTED, 0);

	/* maintain network organization */
	conn_maintain ();

	node_update_cache ();

	return TRUE;
}

/*****************************************************************************/

static void node_class_change (Connection *c, unsigned short change)
{
	char *state_str = "???";

	/* ignore local class changes...expect that they are handled on an
	 * individual level */
	if (c == ft_self)
		return;

	if (!change || NODE (c)->state == NODE_DISCONNECTED)
		return;

	switch (NODE (c)->state)
	{
	 case NODE_CONNECTED:
		state_str = "FINAL";
		break;
	 case NODE_CONNECTING:
		state_str = "LIMBO";
		break;
	 default:
		break;
	}

	TRACE (("%s:%hu (%s) -> NODE_%s",
			net_ip_str (NODE (c)->ip), NODE (c)->port,
			state_str, node_class_str (NODE (c))));

	/* request the stats from this node */
	if (change & NODE_SEARCH)
		ft_packet_send (c, FT_STATS_REQUEST, NULL);

	/* if the node has just changed to a search node */
	if (change & NODE_SEARCH && validate_share_submit ())
		openft_share_local_submit (c);

	/* children should increment total users */
	if (change & NODE_CHILD)
		NODE (ft_self)->users++;

	html_update_nodepage ();
}

void node_class_set (Connection *c, unsigned short class)
{
	unsigned short orig;

	orig = NODE (c)->class;
	NODE (c)->class = class;
	node_class_change (c, (class & ~orig));
}

void node_class_add (Connection *c, unsigned short class)
{
	unsigned short orig;

	orig = NODE (c)->class;
	NODE (c)->class |= class;
	node_class_change (c, (class & ~orig));
}

void node_class_remove (Connection *c, unsigned short klass)
{
	/* klass doesnt match, don't do anything */
	if (!(NODE (c)->class & klass))
		return;

	if (klass & NODE_CHILD)
		NODE (ft_self)->users--;

	NODE (c)->class &= ~klass;

	/* VOLATILE!  Will cause an accidental reconnection to a dead node!
	 * !! VERY DANGEROUS !! */
#if 0
	/* if a parent has just been removed, look for a new one */
	if (class & NODE_PARENT && validate_share_submit ())
	{
		openft_share_local_submit (NULL);
	}
#endif
}

/*****************************************************************************/

void node_state_set (Connection *c, unsigned short state)
{
	char *state_str;
	int   change = 0;

	if (NODE (c)->state != state)
		change = 1;

	NODE (c)->state = state;

	/* dont print useless info */
	if (!change || !NODE (c)->class || NODE (c)->vitality + 300 < time (NULL))
		return;

	switch (state)
	{
	 case NODE_CONNECTED:
		state_str = "CONNECTED";
		break;
	 case NODE_CONNECTING:
		state_str = "CONNECTING";
		break;
	 default:
		state_str = "DISCONNECTED";
		break;
	}

	/* to be honest NODE_USER connections are just used to gather more
	 * nodes in your connection list, hardly worth notifying the user :) */
	if (NODE (c)->class != NODE_USER)
	{
		TRACE (("%s:%hu (NODE_%s) -> NODE_%s",
		        net_ip_str (NODE (c)->ip), NODE (c)->port,
		        node_class_str (NODE (c)), state_str));
	}

	/* if we just got connected, emulate a class change */
	if (state == NODE_CONNECTED)
		node_class_change (c, NODE (c)->class);

	html_update_nodepage ();
}

/*****************************************************************************/

void node_conn_set (Connection *c, unsigned long ip, signed long port,
                    signed long http_port)
{
	if (ip)
		NODE (c)->ip = ip;

	if (port >= 0)
	{
		NODE (c)->firewalled = (port == 0);
		NODE (c)->port = port;
	}

	if (http_port > 0)
		NODE (c)->http_port = http_port;

	html_update_nodepage ();
}
