/*
 * $Id: ft_handshake.c,v 1.8 2003/06/10 16:34:11 jasta Exp $
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

#include "ft_openft.h"

#include "ft_netorg.h"
#include "ft_conn.h"

#include "ft_protocol.h"
#include "ft_handshake.h"

/*****************************************************************************/

#define MIN_VER_USERS 10
#define MAX_VER_USERS 300

/* used to hastle people who dont upgrade */
static Dataset *ver_upgrade = NULL;

/*****************************************************************************/

FT_HANDLER (ft_version_request)
{
	ft_packet_sendva (c, FT_VERSION_RESPONSE, 0, "hhhh",
	                  OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO, OPENFT_REV);
}

static char *generate_msg (Dataset *upgset)
{
	char         *msg;
	unsigned long nodes;

	nodes = dataset_length (upgset);

	msg = stringf ("%lu %s reported a more recent OpenFT "
	               "revision than you are currently using.  You are "
	               "STRONGLY advised to update your node as soon as "
	               "possible.  See http://www.giftproject.org/ for more "
	               "details.",
	               nodes, (nodes == 1) ? "node has" : "nodes have");

	return msg;
}

static void version_outdated (TCPC *c)
{
	char   *ver_str;
	uint8_t major = 0;
	uint8_t minor = 0;
	uint8_t micro = 0;
	uint8_t rev   = 0;

	/* avoid memory abuse */
	if (dataset_length (ver_upgrade) >= MAX_VER_USERS)
		return;

	ft_version_parse (FT_NODE(c)->version, &major, &minor, &micro, &rev);

	/* construct a string mainly so that we don't need to carry around
	 * three values */
	ver_str = stringf ("%hu.%hu.%hu-%hu", major, minor, micro, rev);

	if (!ver_upgrade)
		ver_upgrade = dataset_new (DATASET_HASH);

	dataset_insert (&ver_upgrade,
	                &FT_NODE(c)->ip, sizeof (FT_NODE(c)->ip),
	                ver_str, STRLEN_0 (ver_str));

	/* bother the user only when we have a collection of new version
	 * responses to prevent a simple annoying flood */
	if (dataset_length (ver_upgrade) >= MIN_VER_USERS)
		FT->message (FT, generate_msg (ver_upgrade));
}

FT_HANDLER (ft_version_response)
{
	uint8_t major;
	uint8_t minor;
	uint8_t micro;
	uint8_t rev;

	/* grumble grumble grumble...i'd like to be able to change this to
	 * ..._get_uint8, but it would break backwards compat for a pretty silly
	 * reason */
	major = (uint8_t)ft_packet_get_uint16 (packet, TRUE);
	minor = (uint8_t)ft_packet_get_uint16 (packet, TRUE);
	micro = (uint8_t)ft_packet_get_uint16 (packet, TRUE);
	rev   = (uint8_t)ft_packet_get_uint16 (packet, TRUE);

	/* apply some magic */
	FT_NODE(c)->version = ft_version (major, minor, micro, rev);

	if (FT_VERSION_GT(FT_NODE(c)->version, FT_VERSION_LOCAL))
	{
		/* bug the user to upgrade */
		version_outdated (c);
	}
	else if (FT_VERSION_LT(FT_NODE(c)->version, FT_VERSION_LOCAL))
	{
		ft_node_err (FT_NODE(c), FT_ERROR_VERMISMATCH,
		             stringf ("%08x", FT_NODE(c)->version));

		/* force a response so that this node understands why we just
		 * disconnected them */
		ft_version_request (c, packet);
		ft_session_stop (c);
	}
	else /* if (FT_VERSION_EQ(FT_NODE(c)->version, FT_VERSION_LOCAL)) */
	{
		/* FINISH HANDSHAKE */
		ft_session_stage (c, 1);
	}
}

/*****************************************************************************/

FT_HANDLER (ft_nodeinfo_request)
{
	FTNode     *inforeq = FT_SELF;
	in_addr_t   ip;

	if ((ip = ft_packet_get_ip (packet)) > 0)
	{
		if (!(inforeq = ft_netorg_lookup (ip)))
			return;
	}

	ft_packet_sendva (c, FT_NODEINFO_RESPONSE, 0, "Ihhhs",
	                  inforeq->ip, ft_node_class (inforeq, FALSE),
	                  inforeq->port, inforeq->http_port, inforeq->alias);
}

static void pending_packets (in_addr_t ip, in_port_t port,
                             in_port_t http_port, unsigned short klass)
{
	FTNode *node;

	/* we never registered this node, so this data must have been sent to us
	 * in error */
	if (!(node = ft_netorg_lookup (ip)))
		return;

	/* again, we didn't request packets be scheduled here, so we're not going
	 * to proceed any further */
	if (!node->squeue)
		return;

	/* TODO: we currently cannot redirect to users which cannot be directly
	 * connected as there is no proper forwarding system setup in OpenFT */
	if (port == 0)
		return;

	/* set connection parameters */
	ft_node_set_port      (node, port);
	ft_node_set_http_port (node, http_port);
	ft_node_set_class     (node, klass);

	/* actually make the outgoing TCPC, this should be safe if the connection
	 * is already being attempted */
	ft_session_connect (node);
}

FT_HANDLER (ft_nodeinfo_response)
{
	FTNode        *node;
	in_addr_t      ip;
	unsigned short klass;
	in_port_t      port;
	in_port_t      http_port;
	char          *alias;

	ip        = ft_packet_get_ip     (packet);
	klass     = ft_packet_get_uint16 (packet, TRUE);
	port      = ft_packet_get_uint16 (packet, TRUE);
	http_port = ft_packet_get_uint16 (packet, TRUE);
	alias     = ft_packet_get_str    (packet);

	if (ip)
		node = ft_netorg_lookup (ip);
	else
		node = FT_NODE(c);

	/* strip context-specific status */
	if (klass & FT_NODE_CHILD)
		klass &= ~FT_NODE_CHILD;

	if (klass & FT_NODE_PARENT)
		klass &= ~FT_NODE_PARENT;

	/* add the class data back on if our local node registration says that
	 * they are either a CHILD or PARENT node */
	if (node)
	{
		if (klass & FT_NODE_USER && node->klass & FT_NODE_CHILD)
			klass |= FT_NODE_CHILD;

		if (klass & FT_NODE_SEARCH && node->klass & FT_NODE_PARENT)
			klass |= FT_NODE_PARENT;
	}

	/*
	 * If an ip address was supplied on this command, it means the user is
	 * reporting data on a connected node (and not itself).  This is used for
	 * scheduling packets for delivery to nodes we are not currently
	 * connected to.  See ::ft_packet_sendto for more details.
	 */
	if (ip)
	{
		pending_packets (ip, port, http_port, klass);
		return;
	}

	/* set attributes */
	ft_node_set_class     (FT_NODE(c), (FTNodeClass)klass);
	ft_node_set_port      (FT_NODE(c), port);
	ft_node_set_http_port (FT_NODE(c), http_port);
	ft_node_set_alias     (FT_NODE(c), alias);

	/* we need to verify that this node actually accepts connections on the
	 * configured ports (damn liars!) */
	if (!FT_SESSION(c)->incoming)
	{
		FT_SESSION(c)->verified = TRUE;
		ft_session_stage (c, 2);
	}
	else
	{
		/* they may have just changed ports, not set...so if this node had been
		 * previously verified, we need to do it again */
		FT_SESSION(c)->verified = FALSE;
		ft_accept_test (c);
	}
}

/*****************************************************************************/

static int nodelist_add (FTNode *node, Array **args)
{
	TCPC     *c;
	FTPacket *listpkt;

	list (args, &c, &listpkt, NULL);

	assert (c != NULL);
	assert (listpkt != NULL);

	/* avoid giving the user his ip address back */
	if (node == FT_NODE(c))
		return FALSE;

	/* this is used as a sentinel on the receiving end, so make sure its not
	 * possible to send prematurely */
	assert (node->ip != 0);

	/* add this nodes information to the list */
	ft_packet_put_ip     (listpkt, node->ip);
	ft_packet_put_uint16 (listpkt, node->port, TRUE);
	ft_packet_put_uint16 (listpkt, (uint16_t)node->klass, TRUE);

	return TRUE;
}

static int nodelist_default (Array **args)
{
	int n;

	n = ft_netorg_foreach (FT_NODE_SEARCH | FT_NODE_INDEX, FT_NODE_CONNECTED, 15,
	                       FT_NETORG_FOREACH(nodelist_add), args);

	/* we weren't able to come up with enough search/index nodes, try to give
	 * them at least something */
	if (n < 10)
	{
		n += ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 20,
		                        FT_NETORG_FOREACH(nodelist_add), args);
	}

	return n;
}

static void nodelist_request (TCPC *c, FTNodeClass klass, int nreq)
{
	Array    *args = NULL;
	FTPacket *listpkt;
	int       nodes;

#if 0
	if (n > 2048)
	{
		FT->DBGSOCK (FT, c, "clamped n (%i <=> %i)", n, 2048);
		n = 2048;
	}
#endif

	if (!(listpkt = ft_packet_new (FT_NODELIST_RESPONSE, 0)))
		return;

	/* add the two arguments we will need in nodelist_add */
	push (&args, c);
	push (&args, listpkt);

	/* if the klass requested was nothing special, switch over to a method
	 * which attempts to guess what will be most useful for the remote node */
	if (klass == 0)
		nodes = nodelist_default (&args);
	else
	{
		/* otherwise we should respond only to the connected classes that
		 * they requested (this is used to graph the network through third
		 * party tools) */
		nodes = ft_netorg_foreach (klass, FT_NODE_CONNECTED, nreq,
		                           FT_NETORG_FOREACH(nodelist_add), &args);
	}

	/* we're done w/ this :) */
	unset (&args);

	/* deliver the packet with all nodes compacted in one */
	ft_packet_send (c, listpkt);

	FT->DBGSOCK (FT, c, "sent %i nodes", nodes);
}

FT_HANDLER (ft_nodelist_request)
{
	uint16_t  klass;
	uint16_t  nreq;

	klass = ft_packet_get_uint16 (packet, TRUE);
	nreq  = ft_packet_get_uint16 (packet, TRUE);

	/* pass the work along to the real function */
	nodelist_request (c, (FTNodeClass)klass, nreq);
}

FT_HANDLER (ft_nodelist_response)
{
	FTNode   *node;
	in_addr_t ip;
	in_port_t port;
	uint16_t  klass;
	int       conns;
	int       n = 0;
	int       newconns = 0;

	/* the packet structure given back to us contains a list of nodes for
	 * us to use, so we will simply loop until we've reached the sentinel,
	 * which is defined as ip=0 */
	for (;;)
	{
		ip    = ft_packet_get_ip     (packet);
		port  = ft_packet_get_uint16 (packet, TRUE);
		klass = ft_packet_get_uint16 (packet, TRUE);

		/* sentinel reached, abort */
		if (ip == 0)
			break;

		n++;

		if (!(node = ft_node_register (ip)))
			continue;

		/* we already have this information straight from the user, no need
		 * to run through this */
		if (node->session)
			continue;

		ft_node_set_port  (node, port);
		ft_node_set_class (node, klass);

		conns  = ft_netorg_length (FT_NODE_USER, FT_NODE_CONNECTING);
		conns += ft_netorg_length (FT_NODE_USER, FT_NODE_CONNECTED);

		if (conns > FT_MAX_CONNECTIONS)
			continue;

		if (ft_conn_need_peers () || ft_conn_need_parents ())
		{
			if (ft_session_connect (node) >= 0)
				newconns++;
		}
	}

	FT->DBGSOCK (FT, c, "rcvd %i nodes (%i new conns)", n, newconns);
}

/*****************************************************************************/

static void add_nodecap (ds_data_t *key, ds_data_t *value, FTPacket *packet)
{
	ft_packet_put_uint16 (packet, 1, TRUE);
	ft_packet_put_str    (packet, key->data);
}

FT_HANDLER (ft_nodecap_request)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_NODECAP_RESPONSE, 0)))
		return;

	/* append all our local capabilities to the packet */
	dataset_foreach (FT_SELF->session->cap, DS_FOREACH(add_nodecap), pkt);

	ft_packet_put_uint16 (pkt, 0, TRUE);
	ft_packet_send (c, pkt);
}

FT_HANDLER (ft_nodecap_response)
{
	uint16_t  key_id;
	char     *key;

	if (!(FT_SESSION(c)->cap))
		FT_SESSION(c)->cap = dataset_new (DATASET_LIST);

	while ((key_id = ft_packet_get_uint16 (packet, TRUE)))
	{
		if (!(key = ft_packet_get_str (packet)))
			continue;

		dataset_insertstr (&FT_SESSION(c)->cap, key, key);
	}
}

/*****************************************************************************/

FT_HANDLER (ft_ping_request)
{
	FT_SESSION(c)->heartbeat += 2;
	ft_packet_sendva (c, FT_PING_RESPONSE, 0, NULL);
}

FT_HANDLER (ft_ping_response)
{
	FT_SESSION(c)->heartbeat += 2;
}

/*****************************************************************************/

/*
 * Session negotiation is incomplete.  These functions very lazily trust the
 * assumptions laid out by the handshaking stage.  Fix later.
 */
FT_HANDLER (ft_session_request)
{
	if (FT_SESSION(c)->stage != 3)
		return;

	ft_session_stage (c, 3);
}

FT_HANDLER (ft_session_response)
{
	uint16_t reply;

	if (FT_SESSION(c)->stage != 3)
		return;

	reply = ft_packet_get_uint16 (packet, TRUE);

	if (reply)
		ft_session_stage (c, 3);
}
