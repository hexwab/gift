/*
 * $Id: ft_handshake.c,v 1.26 2004/05/09 00:14:53 jasta Exp $
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
	                &FT_NODE_INFO(c)->host, sizeof (FT_NODE_INFO(c)->host),
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

	if (ft_packet_overrun (packet))
	{
		FT->DBGSOCK (FT, c, "very broken version header");
		return;
	}

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
	in_addr_t      host;
	ft_class_t     klass;
	ft_nodeinfo_t *ninfo;

	/*
	 * By dfeault we are supposed to deliver information about our own local
	 * node, but if an IP address other than 0 is passed, we will respond with
	 * that nodes information if available.
	 */
	if ((host = ft_packet_get_ip (packet)) == 0)
		ninfo = &openft->ninfo;
	else
	{
		FTNode *inode;

		if (!(inode = ft_netorg_lookup (host)))
		{
#if 0
			FT->DBGFN (FT, "unable to locate %s: what to do?",
			           net_ip_str (host));
#endif
			return;
		}

		ninfo = &inode->ninfo;
	}

	/* hack off any modifiers */
	klass = ninfo->klass & FT_NODE_CLASSPRI_MASK;

	ft_packet_sendva (c, FT_NODEINFO_RESPONSE, 0, "Ihhhs",
	                  host, klass,
	                  ninfo->port_openft, ninfo->port_http, ninfo->alias);
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
	ft_session_connect (node, FT_PURPOSE_UNDEFINED | FT_PURPOSE_DELIVERY);
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

	if (ft_packet_overrun (packet))
		return;

	if (ip)
		node = ft_netorg_lookup (ip);
	else
		node = FT_NODE(c);

	/* strip context-specific status */
	klass &= FT_NODE_CLASSPRI_MASK;

	/* add the class data back on if our local node registration says that
	 * they are either a CHILD or PARENT node */
	if (node)
	{
		if (klass & FT_NODE_USER && node->ninfo.klass & FT_NODE_CHILD)
			klass |= FT_NODE_CHILD;

		if (klass & FT_NODE_SEARCH && node->ninfo.klass & FT_NODE_PARENT)
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

	/*
	 * FIXME: The below hack corrupts the ft_netorg_length cache.  Why is this
	 *        hack necessary?  Can it be emulated by calling ft_node_set_class
	 *        two times?
	 */
#if 0
	/* hack the class to emulate a search and/or index class gain */
	node->ninfo.klass &= ~(FT_NODE_SEARCH | FT_NODE_INDEX);
#else
	if (node->ninfo.klass & (FT_NODE_SEARCH | FT_NODE_INDEX))
	{
		handle_class_gain (FT_NODE(c),
		                   node->ninfo.klass & ~(FT_NODE_SEARCH | FT_NODE_INDEX),
		                   node->ninfo.klass);
	}
#endif
	ft_node_set_class (FT_NODE(c), (ft_class_t)klass);

	/* set other attributes */
	ft_node_set_port      (FT_NODE(c), port);
	ft_node_set_http_port (FT_NODE(c), http_port);
	ft_node_set_alias     (FT_NODE(c), alias);

	/* yes there is a race condition here, no i dont care...this code is
	 * temporary */
	if (klass & (FT_NODE_SEARCH | FT_NODE_INDEX) || ft_conn_children_left() > 0)
		FT_SESSION(c)->child_eligibility = TRUE;
	else
		FT_SESSION(c)->child_eligibility = FALSE;

	/* we need to verify that this node actually accepts connections on the
	 * configured ports (damn liars!) */
	if (!FT_SESSION(c)->incoming || FT_SESSION(c)->child_eligibility == FALSE)
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
	TCPC      *c;
	FTPacket **listpkt;

	array_list (args, &c, &listpkt, NULL);

	assert (c != NULL);
	assert (listpkt != NULL);
	assert ((*listpkt) != NULL);

	/* avoid giving the user his ip address back */
	if (node == FT_NODE(c))
		return FALSE;

	/* this is used as a sentinel on the receiving end, so make sure its not
	 * possible to send prematurely */
	assert (node->ninfo.host != 0);

	/*
	 * The size of this packet has grown beyond the maximum allowed size
	 * for a single packet, so we need to chop it up here and send it
	 * along.  Please note that 22 is chosen because of the maximum size
	 * ft_packet_put_ip could theoretically put on the packet for IPv6
	 * addresses, which are not yet even supported.
	 */
	if (ft_packet_length (*listpkt) + 22 >= FT_PACKET_MAX)
	{
		ft_packet_send (c, *listpkt);

		/* construct a new packet to begin writing to and pretend nothing
		 * happened... */
		*listpkt = ft_packet_new (FT_NODELIST_RESPONSE, 0);

		/* too lazy to handle malloc failures */
		assert ((*listpkt) != NULL);
	}

	/* add this nodes information to the list */
	ft_packet_put_ip     (*listpkt, node->ninfo.host);
	ft_packet_put_uint16 (*listpkt, node->ninfo.port_openft, TRUE);
	ft_packet_put_uint16 (*listpkt, (uint16_t)(ft_node_class (node, FALSE)), TRUE);

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

static int nodelist_request (TCPC *c, FTPacket **listpkt,
                             ft_class_t klass, int nreq)
{
	Array *args = NULL;
	int    nodes;

	/* add the two arguments we will need in nodelist_add */
	array_push (&args, c);
	array_push (&args, listpkt);

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
	array_unset (&args);

	return nodes;
}

FT_HANDLER (ft_nodelist_request)
{
	FTPacket *reply;
	uint16_t  klass;
	uint16_t  nreq;
	int       nodes = 0;

	if (!(reply = ft_packet_new (FT_NODELIST_RESPONSE, 0)))
		return;

	while (ft_packet_remaining (packet) > 0)
	{
		klass = ft_packet_get_uint16 (packet, TRUE);
		nreq  = ft_packet_get_uint16 (packet, TRUE);

		/* pass the work along to the real function */
		nodes += nodelist_request (c, &reply, (ft_class_t)klass, nreq);
	}

	/*
	 * Add the final packet sentinel.  If multiple requests are delivered as
	 * a single command, there will only be one final sentinel for sanity
	 * sake.  If you need multiple sentinels for your design, use multiple
	 * requests.
	 */
	ft_packet_put_ip     (reply, 0);
	ft_packet_put_uint16 (reply, 0, TRUE);
	ft_packet_put_uint16 (reply, 0, TRUE);

	/*
	 * Please note that this may have already been called from
	 * nodelist_request to restrict the maximimze size of the outgoing packet
	 * so that we dont exceed FT_PACKET_MAX.
	 */
	ft_packet_send (c, reply);

#if 0
	FT->DBGSOCK (FT, c, "sent %i nodes", nodes);
#endif
}

FT_HANDLER (ft_nodelist_response)
{
	FTNode   *node;
	in_addr_t ip;
	in_port_t port;
	uint16_t  klass;
	BOOL      need_peers;
	BOOL      need_parents;
	int       conns, conns_max;
	int       n = 0;
	int       newconns = 0;

	/*
	 * Access the total number of connections that currently exist so that
	 * we can assess whether or not additional outgoing connections should be
	 * made.
	 */
	conns     = ft_netorg_length (FT_NODE_USER, FT_NODE_CONNECTING);
	conns    += ft_netorg_length (FT_NODE_USER, FT_NODE_CONNECTED);
	conns_max = FT_CFG_MAX_CONNECTIONS;

	/*
	 * The packet structure given back to us contains a list of nodes for us
	 * to use, so we will simply loop until we've reached the sentinel, which
	 * is defined as ip=0.  This condition may also be reached if the end of
	 * the packet stream is reached.  Keep in mind that multiple nodelist
	 * response commands may be needed to fully satisfy a nodelist request.
	 */
	for (;;)
	{
		ip    = ft_packet_get_ip     (packet);
		port  = ft_packet_get_uint16 (packet, TRUE);
		klass = ft_packet_get_uint16 (packet, TRUE);
		klass &= FT_NODE_CLASSPRI_MASK;

		/* sentinel reached, abort */
		if (ip == 0)
			break;

		n++;

		/*
		 * We need to set klass on registration because ft_node_set_class will
		 * work incorrectly if node->session == NULL and cause corruption of
		 * the ft_netorg_length caching.
		 *
		 * TODO: Should this be changed/fixed in ft_node_set_class and friends?
		 */
		if (!(node = ft_node_register_full (ip, 0, 0, klass, 0, 0, 0)))
			continue;

		/* we already have this information straight from the user, no need
		 * to run through this */
		if (node->session)
			continue;

		ft_node_set_port  (node, port);
#if 0
		ft_node_set_class (node, klass);
#endif

		if (conns + newconns > conns_max)
			continue;

		need_peers   = ft_conn_need_peers();
		need_parents = ft_conn_need_parents();

		if (need_peers || need_parents)
		{
			ft_purpose_t goal = 0;

			if (need_parents)
				goal = FT_PURPOSE_PARENT_TRY;
			else
				goal = FT_PURPOSE_UNDEFINED;

			if (ft_session_connect (node, goal) >= 0)
				newconns++;
		}
	}

#if 0
	FT->DBGSOCK (FT, c, "rcvd %i nodes (%i new conns)", n, newconns);
#endif

	ft_session_drop_purpose (FT_NODE(c), FT_PURPOSE_GET_NODES);
}

/*****************************************************************************/

FT_HANDLER (ft_nodecap_request)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_NODECAP_RESPONSE, 0)))
		return;

	/* append all our local capabilities to the packet */
#ifdef USE_ZLIB
	ft_packet_put_uint16 (pkt, 1, TRUE);   /* legacy cruft */
	ft_packet_put_str    (pkt, "ZLIB");
#endif /* USE_ZLIB */

	ft_packet_put_uint16 (pkt, 0, TRUE);
	ft_packet_send (c, pkt);
}

FT_HANDLER (ft_nodecap_response)
{
	uint16_t  key_id;
	char     *key;

	if (!(FT_SESSION(c)->cap))
		FT_SESSION(c)->cap = dataset_new (DATASET_LIST);

	while (ft_packet_remaining (packet) > 0)
	{
		key_id = ft_packet_get_uint16 (packet, TRUE);
		key    = ft_packet_get_str    (packet);

		if (key_id == 0 || !key)
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
