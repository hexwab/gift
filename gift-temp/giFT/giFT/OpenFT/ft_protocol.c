/*
 * $Id: ft_protocol.c,v 1.52 2003/05/05 09:49:10 jasta Exp $
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

#include "ft_xfer.h"

#include "file.h"
#include "ft_search.h"
#include "ft_search_exec.h"
#include "ft_search_db.h"
#include "parse.h"

#include "md5.h"
#include "meta.h"

/*****************************************************************************/

/* manages search requests to minimize the possibility of duplicated queries,
 * see manage_searches() here */
static Dataset *searches = NULL;

/*****************************************************************************/

#define MIN_VER_USERS 10
#define MAX_VER_USERS 300

/* used to hastle people who dont upgrade */
static Dataset *ver_upgrade = NULL;

/*****************************************************************************/

typedef void (*ProtocolHandler) (Protocol *p, TCPC *c, FTPacket *packet);
#define FT_HANDLER(hfunc) static void hfunc (Protocol *p, TCPC *c, FTPacket *packet)

FT_HANDLER (ft_version_request);
FT_HANDLER (ft_version_response);
FT_HANDLER (ft_nodeinfo_request);
FT_HANDLER (ft_nodeinfo_response);
FT_HANDLER (ft_nodelist_request);
FT_HANDLER (ft_nodelist_response);
FT_HANDLER (ft_nodecap_request);
FT_HANDLER (ft_nodecap_response);
FT_HANDLER (ft_ping_request);
FT_HANDLER (ft_ping_response);
FT_HANDLER (ft_session_request);
FT_HANDLER (ft_session_response);

FT_HANDLER (ft_child_request);
FT_HANDLER (ft_child_response);
FT_HANDLER (ft_addshare_request);
FT_HANDLER (ft_addshare_response);
FT_HANDLER (ft_remshare_request);
FT_HANDLER (ft_remshare_response);
FT_HANDLER (ft_modshare_request);
FT_HANDLER (ft_modshare_response);
FT_HANDLER (ft_stats_request);
FT_HANDLER (ft_stats_response);

FT_HANDLER (ft_search_request);
FT_HANDLER (ft_search_response);
FT_HANDLER (ft_browse_request);
FT_HANDLER (ft_browse_response);

FT_HANDLER (ft_push_request);
FT_HANDLER (ft_push_forward);

/*****************************************************************************/

static struct _handler_table
{
	uint16_t        command;
	ProtocolHandler func;
}
protocol_handler[] =
{
	{ FT_VERSION_REQUEST,   ft_version_request      },
	{ FT_VERSION_RESPONSE,  ft_version_response     },
	{ FT_NODEINFO_REQUEST,  ft_nodeinfo_request     },
	{ FT_NODEINFO_RESPONSE, ft_nodeinfo_response    },
	{ FT_NODELIST_REQUEST,  ft_nodelist_request     },
	{ FT_NODELIST_RESPONSE, ft_nodelist_response    },
	{ FT_NODECAP_REQUEST,   ft_nodecap_request      },
	{ FT_NODECAP_RESPONSE,  ft_nodecap_response     },
	{ FT_PING_REQUEST,      ft_ping_request         },
	{ FT_PING_RESPONSE,     ft_ping_response        },
	{ FT_SESSION_REQUEST,   ft_session_request      },
	{ FT_SESSION_RESPONSE,  ft_session_response     },

	{ FT_CHILD_REQUEST,     ft_child_request        },
	{ FT_CHILD_RESPONSE,    ft_child_response       },
	{ FT_ADDSHARE_REQUEST,  ft_addshare_request     },
	{ FT_ADDSHARE_RESPONSE, ft_addshare_response    },
	{ FT_REMSHARE_REQUEST,  ft_remshare_request     },
	{ FT_REMSHARE_RESPONSE, ft_remshare_response    },
	{ FT_MODSHARE_REQUEST,  ft_modshare_request     },
	{ FT_MODSHARE_RESPONSE, ft_modshare_response    },
	{ FT_STATS_REQUEST,     ft_stats_request        },
	{ FT_STATS_RESPONSE,    ft_stats_response       },

	{ FT_SEARCH_REQUEST,    ft_search_request       },
	{ FT_SEARCH_RESPONSE,   ft_search_response      },
	{ FT_BROWSE_REQUEST,    ft_browse_request       },
	{ FT_BROWSE_RESPONSE,   ft_browse_response      },

	{ FT_PUSH_REQUEST,      ft_push_request         },
	{ FT_PUSH_FORWARD,      ft_push_forward         },

	{ 0,                    NULL                    }
};

/*****************************************************************************/

static int handle_command (Protocol *p, TCPC *c, FTPacket *packet)
{
	struct _handler_table *handler;
	uint16_t cmd;

	cmd = ft_packet_command (packet);

	/* locate the handler */
	for (handler = protocol_handler; handler->func; handler++)
	{
		if (cmd == handler->command)
		{
			(*handler->func) (p, c, packet);
			return TRUE;
		}
	}

	FT->err (FT, "[%s] found no handler for cmd=0x%04x len=0x%04x",
	         net_ip_str (FT_NODE(c)->ip), packet->command, packet->len);

	return FALSE;
}

static void handle_stream_pkt (FTStream *stream, FTPacket *stream_pkt,
                               TCPC *c)
{
	handle_command (FT, c, stream_pkt);
}

static int handle_stream (Protocol *p, TCPC *c, FTPacket *packet)
{
	FTStream *stream;

	if (!(stream = ft_stream_get (c, FT_STREAM_RECV, packet)))
		return FALSE;

	ft_stream_recv (stream, packet, (FTStreamRecv) handle_stream_pkt, c);

	if (stream->eof)
		ft_stream_finish (stream);

	return TRUE;
}

int protocol_handle (Protocol *p, TCPC *c, FTPacket *packet)
{
	uint16_t cmd;

	if (!packet)
		return FALSE;

	cmd = ft_packet_command (packet);

	if (ft_packet_flags (packet) & FT_PACKET_STREAM)
		return handle_stream (p, c, packet);

	return handle_command (p, c, packet);
}

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
		ft_version_request (p, c, packet);
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
	                  inforeq->ip, inforeq->klass,
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

	/* preserve previous context-specific status */
	if (FT_NODE(c)->klass & NODE_CHILD)
		klass |= NODE_CHILD;

	if (klass & NODE_SEARCH && FT_NODE(c)->klass & NODE_PARENT)
		klass |= NODE_PARENT;

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

FT_HANDLER (ft_nodelist_request)
{
	Array    *args = NULL;
	FTPacket *listpkt;
	int       nodes;

	if (!(listpkt = ft_packet_new (FT_NODELIST_RESPONSE, 0)))
		return;

	/* add the two arguments we will need in nodelist_add */
	push (&args, c);
	push (&args, listpkt);

	nodes = ft_netorg_foreach (NODE_SEARCH | NODE_INDEX, NODE_CONNECTED, 15,
	                           FT_NETORG_FOREACH(nodelist_add), &args);

	/* we weren't able to come up with enough search/index nodes, try to give
	 * them at least something */
	if (nodes < 10)
	{
		nodes += ft_netorg_foreach (NODE_USER, NODE_CONNECTED, 20,
		                            FT_NETORG_FOREACH(nodelist_add), &args);
	}

	/* we're done w/ this :) */
	unset (&args);

	/* deliver the packet with all nodes compacted in one */
	ft_packet_send (c, listpkt);

	FT->DBGSOCK (FT, c, "sent %i nodes", nodes);
}

FT_HANDLER (ft_nodelist_response)
{
	FTNode   *node;
	in_addr_t ip;
	in_port_t port;
	uint16_t  klass;
	int       conns;
	int       n = 0;

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

		if (!(node = ft_node_register (ip)))
			continue;

		/* we already have this information straight from the user, no need
		 * to run through this */
		if (node->session)
			continue;

		ft_node_set_port  (node, port);
		ft_node_set_class (node, klass);

		conns  = ft_netorg_length (NODE_USER, NODE_CONNECTING);
		conns += ft_netorg_length (NODE_USER, NODE_CONNECTED);

		if (conns < FT_MAX_CONNECTIONS)
			ft_session_connect (node);

		n++;
	}

	FT->DBGSOCK (FT, c, "rcvd %i new nodes", n);
}

/*****************************************************************************/

static int add_nodecap (Dataset *d, DatasetNode *node, FTPacket *packet)
{
	ft_packet_put_uint16 (packet, 1, TRUE);
	ft_packet_put_str (packet, node->key);
	return FALSE;
}

FT_HANDLER (ft_nodecap_request)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_NODECAP_RESPONSE, 0)))
		return;

	dataset_foreach (FT_SELF->session->cap,
	                 DATASET_FOREACH (add_nodecap), pkt);

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

/*****************************************************************************/

FT_HANDLER (ft_child_request)
{
	unsigned short response = FALSE;

	/* user has requested to be our child, lets hope we're a search node ;) */
	if (!(FT_SELF->klass & NODE_SEARCH))
		return;

	/* the final stage in negotiation
	 * NOTE: its here because it prevents a race condition in the child */
	if (packet->len > 0)
	{
		uint16_t reply;

		reply = ft_packet_get_uint16 (packet, TRUE);

		if (reply)
			ft_node_add_class (FT_NODE(c), NODE_CHILD);
		else
			ft_node_remove_class (FT_NODE(c), NODE_CHILD);

		return;
	}

	/*
	 * If either this node is already our child, or they have dirty records on
	 * our node, they should NOT be authorized.  They need to wait until
	 * this issue is resolved before they come back :)
	 */
	if (!ft_shost_get (FT_NODE(c)->ip))
	{
		/* if we have fewer than FT_MAX_CHILDREN, response is TRUE */
		if (ft_netorg_length (NODE_CHILD, NODE_CONNECTED) < FT_MAX_CHILDREN)
			response = TRUE;
	}

	/* reply to the child-status request */
	ft_packet_sendva (c, FT_CHILD_RESPONSE, 0, "h", response);
}

FT_HANDLER (ft_child_response)
{
	uint16_t response;

	if (!(FT_NODE(c)->klass & NODE_SEARCH))
		return;

	response = ft_packet_get_uint16 (packet, TRUE);

	/* they refused our request */
	if (!response)
	{
		FT->DBGSOCK (FT, c, "request refused");
		ft_node_remove_class (FT_NODE(c), NODE_PARENT);
		return;
	}

	/* figure out if we still need them */
	if (!ft_conn_need_parents ())
	{
		/* gracefully inform this node that we have decided not to accept
		 * them as our parent */
		ft_packet_sendva (c, FT_CHILD_REQUEST, 0, "h", FALSE);
		return;
	}

	/* accept */
	ft_packet_sendva (c, FT_CHILD_REQUEST, 0, "h", TRUE);

	if (FT_NODE(c)->klass & NODE_PARENT)
		return;

	ft_node_add_class (FT_NODE(c), NODE_PARENT);

	/* we have a new parent, submit our shares and once complete
	 * ft_share_local_submit will notify them of our sharing eligibility so
	 * that they may report it to users searching for our files */
	ft_share_local_submit (c);
}

/*****************************************************************************/

static int is_child (TCPC *c)
{
	if (!(FT_SELF->klass & NODE_SEARCH))
		return FALSE;

	return (FT_NODE(c)->klass & NODE_CHILD);
}

FT_HANDLER (ft_addshare_request)
{
	uint32_t       size;
	char          *mime;
	unsigned char *md5;
	char          *path;
	char          *meta_key, *meta_value;
	Dataset       *meta = NULL;

	if (!is_child (c))
		return;

	size = ft_packet_get_uint32 (packet, TRUE);
	mime = ft_packet_get_str    (packet);
	md5  = ft_packet_get_ustr   (packet, 16);
	path = ft_packet_get_str    (packet);

	if (!mime || !md5 || !path || size == 0)
		return;

	while ((meta_key = ft_packet_get_str (packet)))
	{
		if (!(meta_value = ft_packet_get_str (packet)))
			break;

		dataset_insert (&meta, meta_key, STRLEN_0 (meta_key), meta_value, 0);
	}

	ft_share_add (FT_SESSION(c)->verified, FT_NODE(c)->ip, FT_NODE(c)->port,
	              FT_NODE(c)->http_port, FT_NODE(c)->alias,
	              (off_t)size, md5, mime, path, meta);

	dataset_clear (meta);
}

FT_HANDLER (ft_addshare_response)
{
}

/*****************************************************************************/

FT_HANDLER (ft_remshare_request)
{
	unsigned char *md5;

	if (!is_child (c))
		return;

	/* remove all shares
	 * NOTE: this keeps the user authorized as a child assuming the shares
	 * will be resynced soon */
	if (!ft_packet_length (packet))
	{
		ft_shost_remove (FT_NODE(c)->ip);
		return;
	}

	if (!(md5 = ft_packet_get_ustr (packet, 16)))
		return;

	ft_share_remove (FT_NODE(c)->ip, md5);
}

FT_HANDLER (ft_remshare_response)
{
}

/*****************************************************************************/

static int submit_digest_index (FTNode *node, TCPC *child)
{
	unsigned long shares = 0;
	double        size   = 0.0;        /* MB */

	if (!ft_shost_digest (FT_NODE(child)->ip, &shares, &size, NULL))
		return FALSE;

	/* submit the digest */
	ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, "hIll",
	                  2 /* SUBMIT DIGEST */,
	                  FT_NODE(child)->ip, shares, (unsigned long)size);

	return TRUE;
}

/* send this users share digest to all index nodes */
static void submit_digest (TCPC *c)
{
	ft_netorg_foreach (NODE_INDEX, NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(submit_digest_index), c);
}

FT_HANDLER (ft_modshare_request)
{
	uint16_t trans_comp;
	uint32_t avail;

	if (!is_child (c))
		return;

	trans_comp = ft_packet_get_uint16 (packet, TRUE);
	avail      = ft_packet_get_uint32 (packet, TRUE);

	if (trans_comp)
	{
		ft_search_db_sync (ft_shost_get (FT_NODE(c)->ip));
		submit_digest (c);
	}

	ft_shost_avail (FT_NODE(c)->ip, (unsigned long)avail);
}

FT_HANDLER (ft_modshare_response)
{
}

/*****************************************************************************/

/* multipurpose function.  may either submit a users digest to an INDEX node
 * or retrieve the total shares an INDEX node is aware of */
FT_HANDLER (ft_stats_request)
{
	uint16_t request;

	/* this command is for INDEX nodes only! */
	if (!(FT_SELF->klass & NODE_INDEX))
		return;

	request = ft_packet_get_uint16 (packet, TRUE);

	switch (request)
	{
	 case 1: /* RETRIEVE INFO */
		{
			unsigned long users  = 0;
			unsigned long shares = 0;
			double        size   = 0.0; /* GB */

			ft_stats_get (&users, &shares, &size);

			ft_packet_sendva (c, FT_STATS_RESPONSE, 0, "lll",
			                  users, shares, (unsigned long) size);
		}
		break;
	 case 2: /* SUBMIT USER SHARES REPORT */
		{
			uint32_t user;             /* user who owns these files */
			uint32_t shares;
			uint32_t size;             /* MB */

			user   = ft_packet_get_ip     (packet);
			shares = ft_packet_get_uint32 (packet, TRUE);
			size   = ft_packet_get_uint32 (packet, TRUE);

			/* handle processing of this data */
			ft_stats_add (FT_NODE(c)->ip, user, shares, size);
		}
		break;
	 case 3: /* REMOVE USER SHARES REPORT */
		{
			uint32_t user;

			user = ft_packet_get_ip (packet);

			ft_stats_remove (FT_NODE(c)->ip, user);
		}
		break;
	 default:
		break;
	}
}

FT_HANDLER (ft_stats_response)
{
	unsigned long users;
	unsigned long shares;
	unsigned long size; /* GB */

	/* ignore this data from a non-index node */
	if (!(FT_NODE(c)->klass & NODE_INDEX))
		return;

	users  = ft_packet_get_uint32 (packet, TRUE);
	shares = ft_packet_get_uint32 (packet, TRUE);
	size   = ft_packet_get_uint32 (packet, TRUE);

	/* set this index nodes share stats */
	FT_SESSION(c)->stats.users  = users;
	FT_SESSION(c)->stats.shares = shares;
	FT_SESSION(c)->stats.size   = size;
}

/*****************************************************************************/

/* structure for fulfilling search/browse requests */
struct sreply
{
	TCPC      *c;                      /* connection to deliver to */
	in_addr_t  srcorig;                /* original searching user */
	ft_guid_t *guid;                   /* remote search id */
	FTStream  *stream;                 /* stream context */
};

struct sreply *sreply_new (TCPC *c, in_addr_t srcorig, ft_guid_t *guid,
                           FTStream *stream)
{
	struct sreply *reply;

	if (!(reply = malloc (sizeof (struct sreply))))
		return NULL;

	reply->c       = c;
	reply->srcorig = srcorig;
	reply->guid    = guid;
	reply->stream  = stream;

	return reply;
}

void sreply_free (struct sreply *reply)
{
	ft_stream_finish (reply->stream);
	free (reply);
}

void sreply_send (struct sreply *reply, FTPacket *p)
{
	if (reply->stream)
		ft_stream_send (reply->stream, p);
	else
		ft_packet_send (reply->c, p);
}

static int add_meta (Dataset *d, DatasetNode *node, FTPacket *p)
{
	ft_packet_put_str (p, node->key);
	ft_packet_put_str (p, node->value);
	return FALSE;
}

static void sreply_result (struct sreply *reply, FileShare *file,
                           FTSHost *shost)
{
	FTPacket *p;
	Hash     *hash;
	char     *path;

	if (!(hash = share_hash_get (file, "MD5")))
		return;

	/* if an hpath was supplied (local share) we want to keep the
	 * output consistent */
	path = (SHARE_DATA(file)->hpath ?
	        SHARE_DATA(file)->hpath : SHARE_DATA(file)->path);

	/* begin packet */
	if (!(p = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return;

	/* add the requested id the search response so that the searcher knows
	 * what this result is */
	ft_packet_put_ustr   (p, reply->guid, FT_GUID_SIZE);

	/* add space for the nodes forwarding this query to add our ip address, so
	 * that the original parent that replied to the search may be tracked */
	ft_packet_put_ip     (p, 0);
	ft_packet_put_uint16 (p, FT_SELF->port, TRUE);

	/* add the address and contact information of the user who owns this
	 * particular search result */
	ft_packet_put_ip     (p, shost->host);
	ft_packet_put_uint16 (p, shost->ft_port, TRUE);
	ft_packet_put_uint16 (p, shost->http_port, TRUE);
	ft_packet_put_str    (p, shost->alias);

	/* add this users "availability", that is, how many open upload slots thye
	 * currently have */
	ft_packet_put_uint32 (p, shost->availability, TRUE);

	/* add the result file information */
	ft_packet_put_uint32 (p, (uint32_t)file->size, TRUE);
	ft_packet_put_ustr   (p, hash->data, hash->len);
	ft_packet_put_str    (p, file->mime);
	ft_packet_put_str    (p, path);

	/* add meta data */
	meta_foreach (file, DATASET_FOREACH (add_meta), p);

	/* off ya go */
	sreply_send (reply, p);
}

/*****************************************************************************/

static void search_term (TCPC *c, ft_guid_t *guid, struct sreply *reply)
{
	FTPacket *pkt;

	if (reply)
		sreply_free (reply);

	if (!(pkt = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return;

	ft_packet_put_ustr (pkt, guid, FT_GUID_SIZE);
	ft_packet_send (c, pkt);
}

static int search_result_logic (FileShare *file, struct sreply *reply)
{
	FTShare *share;
	FTSHost *shost;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	if (!(shost = share->shost))
		return FALSE;

	/* first drop all results that we matched from the node who requested
	 * the search directly */
	if (shost->host == FT_NODE(reply->c)->ip)
		return FALSE;

	/* next drop all the results that matched the original source of the
	 * search, which may be the same as the condition above, but we cant
	 * guarantee that */
	if (shost->host == reply->srcorig)
		return FALSE;

	/* now actually send the result */
	sreply_result (reply, file, share->shost);
	return TRUE;
}

static int search_result (FileShare *file, struct sreply *reply)
{
	int ret;

	/* special exception */
	if (!file)
		return TRUE;

	/* calling result_reply for better flow control :) */
	ret = search_result_logic (file, reply);
	ft_share_unref (file);

	/* the return value determines whether or not to "accept" this result as
	 * part of the searches nmax */
	return ret;
}

struct sparams
{
	ft_guid_t     *guid;
	in_addr_t      srcorig;            /* original sender of the search */
	FTNode        *node;               /* node that requested the search */
	uint16_t       ttl;                /* remaining time-to-live */
	uint16_t       nmax;               /* maximum results desired */
	uint16_t       type;               /* query type */
	void          *query;
	void          *exclude;
	char          *realm;
};

static int exec_search (TCPC *c, struct sparams *srch)
{
	FTNode        *chk_parent;
	struct sreply *reply;
	char          *query_str;
	char          *exclude_str;
	ft_guid_t     *guid = srch->guid;  /* shorthand */
	FTSearchType   type = 0x00;
	int            n;                  /* number of search results */

	/* this is used for debugging only, see below */
	if (srch->type & FT_SEARCH_HIDDEN)
	{
		query_str   = "*hidden*";
		exclude_str = "";
	}
	else
	{
		query_str   = srch->query;
		exclude_str = srch->exclude;
	}

	/* gather the entity that will be used for search manipulation */
	reply = sreply_new (c, srch->srcorig, guid,
	                    ft_stream_get (c, FT_STREAM_SEND, NULL));

	if (!reply)
	{
		/* if we can't create the stream object to write these results, we
		 * really can't do anything */
		search_term (c, guid, reply);
		return 0;
	}

	/*
	 * Determine the "real" search type to pass to ft_search.  This logic is
	 * used to optimize away FT_SEARCH_LOCAL when our parent is the searching
	 * node, as we know they can already provide all those results
	 * themselves.  It would be extremely wasteful to ask the network to
	 * carry that along.
	 */
	type = (FTSearchType)srch->type;

	if (!(chk_parent = ft_netorg_lookup (srch->srcorig)) ||
	    !(chk_parent->klass & NODE_PARENT))
		type |= FT_SEARCH_LOCAL;

	/* execute the search, calling sreply_result after each successful
	 * match */
	n = ft_search (srch->nmax, (FTSearchResultFn)search_result, reply,
	               type, srch->realm, srch->query, srch->exclude);

	/* provide debugging information */
	if (srch->type & FT_SEARCH_FILENAME)
	{
		FT->DBGSOCK (FT, c, "[%hu:%i]: '%s' (%s/%s)...%i/%i result(s)",
		             (unsigned short)srch->type, (int)srch->ttl,
					 query_str, exclude_str, srch->realm, n, srch->nmax);
	}

	/* destroy the search data */
	search_term (c, guid, reply);

	/* we need to give back the number of successful search results so that
	 * the broadcast can decrement */
	return n;
}

static int forward_search (FTNode *node, struct sparams *srch)
{
	FTSearchFwd *sfwd;
	FTPacket    *pkt;

	/* refuse to send the search back to absolute original originator of
	 * the search, that would just be evil */
	if (srch->srcorig == node->ip)
		return FALSE;

	/* refuse to send the search back to the person that asked us to forward
	 * the search (if src != srcorig) */
	if (srch->node == node)
		return FALSE;

	/* create the data structure and associatations needed to coordinate
	 * the response from `node' (and send it to srch->src) */
	if (!(sfwd = ft_search_fwd_new (srch->guid, srch->node->ip, node->ip)))
		return FALSE;

	/* actually send the search */
	if (!(pkt = ft_packet_new (FT_SEARCH_REQUEST, 0)))
		return FALSE;

	/* see ft_search.c:send_search for more information about what all this
	 * jazz means */
	ft_packet_put_ustr   (pkt, srch->guid, FT_GUID_SIZE);
	ft_packet_put_ip     (pkt, srch->srcorig);
	ft_packet_put_uint16 (pkt, srch->ttl, TRUE);
	ft_packet_put_uint16 (pkt, srch->nmax, TRUE);
	ft_packet_put_uint16 (pkt, srch->type, TRUE);

	if (srch->type & FT_SEARCH_HIDDEN)
	{
		ft_packet_put_uarray (pkt, 4, srch->query, TRUE);
		ft_packet_put_uarray (pkt, 4, srch->exclude, TRUE);
	}
	else
	{
		ft_packet_put_str (pkt, srch->query);
		ft_packet_put_str (pkt, srch->exclude);
	}

	ft_packet_put_str (pkt, srch->realm);

	/* deliver the search query */
	if (ft_packet_send (FT_CONN(node), pkt) < 0)
	{
		ft_search_fwd_finish (sfwd);
		return FALSE;
	}

	FT->DBGSOCK (FT, FT_CONN(node), "forwarded %s...",
	             ft_guid_fmt (srch->guid));

	return TRUE;
}

struct slookup
{
	ft_guid_t guid[FT_GUID_SIZE];
	in_addr_t src;
};

static int clear_search (struct slookup *data)
{
	dataset_remove (searches, data, sizeof (struct slookup));
	free (data);

	return FALSE;
}

static int manage_searches (struct sparams *srch)
{
	struct slookup *data;

	if (!srch->guid || !srch->srcorig)
		return FALSE;

	if (!(data = malloc (sizeof (struct slookup))))
		return FALSE;

	memcpy (data->guid, srch->guid, FT_GUID_SIZE);
	data->src = srch->srcorig;

	/* ensure that we havent already processing this search */
	if (dataset_lookup (searches, data, sizeof (struct slookup)))
	{
		free (data);
		return FALSE;
	}

	if (!searches)
		searches = dataset_new (DATASET_HASH);

	/*
	 * Add the search to the list of currently "active" requests.  We do this
	 * so that future requests from the same user with the same id will not
	 * be satisfied (we may accidentally get the result broadcasted from
	 * another peer).  We want to add a timer as this search may one day
	 * become valid again, and we don't want to waste the memory anyway.
	 *
	 * Please note that memory management of `data' will occur in the timeout,
	 * not here.
	 */
	dataset_insert (&searches, data, sizeof (struct slookup), "slookup", 0);
	timer_add (15 * MINUTES, (TimerCallback)clear_search, data);

	return TRUE;
}

/* returns TRUE if the parameters were adjusted, otherwise FALSE */
static int clamp_params (struct sparams *srch)
{
	int ret = FALSE;

	if (srch->nmax > FT_MAX_RESULTS)
	{
		srch->nmax = FT_MAX_RESULTS;
		ret = TRUE;
	}

	if (srch->ttl > FT_MAX_TTL)
	{
		srch->ttl = FT_MAX_TTL;
		ret = TRUE;
	}

	return ret;
}

FT_HANDLER (ft_search_request)
{
	struct sparams params, *srch;
	int            results;

	if (!(FT_SELF->klass & NODE_SEARCH))
		return;

	/* i really hate the '.' operator */
	memset (&params, 0, sizeof (params));
	srch = &params;

	srch->guid    = ft_packet_get_ustr   (packet, 16);
	srch->node    = FT_NODE(c);
	srch->srcorig = ft_packet_get_ip     (packet);
	srch->ttl     = ft_packet_get_uint16 (packet, TRUE);
	srch->nmax    = ft_packet_get_uint16 (packet, TRUE);
	srch->type    = ft_packet_get_uint16 (packet, TRUE);

	/* ensure that the search params supplied are sane according to our
	 * local nodes max configuration */
	if (clamp_params (srch))
	{
		FT->DBGSOCK (FT, c, "clamped nmax=%hu, ttl=%hu",
		         	(unsigned short)srch->nmax, (unsigned short)srch->ttl);
	}

	/* the original node that executes this search will record the src as 0,
	 * leaving the inner-most nodes (us) to fill in the blank */
	if (srch->srcorig == 0)
		srch->srcorig = srch->node->ip;

	/* detect duplicate queries, and ignore them */
	if (!manage_searches (srch))
	{
		FT->DBGSOCK (FT, c, "refusing search request, already seen this!");
		search_term (c, srch->guid, NULL);
		return;
	}

	if (srch->type & FT_SEARCH_HIDDEN)
	{
		srch->query   = ft_packet_get_array (packet, 4, 0, TRUE);
		srch->exclude = ft_packet_get_array (packet, 4, 0, TRUE);
	}
	else
	{
		srch->query   = ft_packet_get_str (packet);
		srch->exclude = ft_packet_get_str (packet);
	}

	srch->realm = ft_packet_get_str (packet);

	/* execute a search on our local node before forwarding begins */
	results = exec_search (c, srch);

	/* if the search still has time-to-live, and we have results left to
	 * satisfy, forward this search along to all of our peers (this behaviour
	 * is also temporary, pending a search optimization discussed on
	 * gift-openft a short while ago) */
	if (srch->ttl > 1 && results < srch->nmax)
	{
		int n;

		/* adjust the new header */
		srch->ttl--;
		srch->nmax -= results;

		FT->DBGSOCK (FT, c, "%s[%i:%i]: forward broadcast for %s...",
		             ft_guid_fmt (srch->guid), (int)srch->ttl, (int)srch->nmax,
		             net_ip_str (srch->srcorig));

		n = ft_netorg_foreach (NODE_SEARCH, NODE_CONNECTED, FT_SEARCH_PEERS,
		                       FT_NETORG_FOREACH(forward_search), srch);

		FT->DBGSOCK (FT, c, "%s: %i peers used",
		             ft_guid_fmt (srch->guid), n);
	}
}

static FTSHost *create_share_host (in_addr_t host,
                                   in_port_t port, in_port_t http_port,
                                   char *alias, unsigned long avail)
{
	FTSHost *shost;

	if (!(shost = ft_shost_new (TRUE, host, port, http_port, alias)))
		return NULL;

	shost->availability = avail;

	return shost;
}

static FileShare *create_result (TCPC *c, FTPacket *packet, int browse,
                                 FTSHost **shost)
{
	FileShare     *file;
	uint32_t       host;
	uint16_t       port;
	uint16_t       http_port;
	char          *alias;
	uint32_t       avail;
	uint32_t       size;
	unsigned char *md5;
	char          *filename;
	char          *mime;
	char          *meta_key;
	char          *meta_value;

	if (!browse)
	{
		host      = ft_packet_get_ip     (packet);
		port      = ft_packet_get_uint16 (packet, TRUE);
		http_port = ft_packet_get_uint16 (packet, TRUE);
		alias     = ft_packet_get_str    (packet);
	}
	else
	{
		host      = FT_NODE(c)->ip;
		port      = FT_NODE(c)->port;
		http_port = FT_NODE(c)->http_port;
		alias     = FT_NODE(c)->alias;
	}

	avail     = ft_packet_get_uint32 (packet, TRUE);
	size      = ft_packet_get_uint32 (packet, TRUE);
	md5       = ft_packet_get_ustr   (packet, 16);
	mime      = ft_packet_get_str    (packet);
	filename  = ft_packet_get_str    (packet);

	/* if host is 0, assume that these are local shares from that node */
	if (host == 0)
		host = FT_NODE(c)->ip;

	if (!(*shost = create_share_host (host, port, http_port, alias, avail)))
		return NULL;

	if (!(file = ft_share_new (*shost, size, md5, mime, filename)))
	{
		FT->DBGSOCK (FT, c, "unable to create result");
		ft_shost_free (*shost);
		*shost = NULL;
		return NULL;
	}

	while ((meta_key = ft_packet_get_str (packet)))
	{
		if (!(meta_value = ft_packet_get_str (packet)))
			break;

		meta_set (file, meta_key, meta_value);
	}

	return file;
}

static void destroy_result (FileShare *file, FTSHost *shost)
{
	if (!file)
		return;

	ft_shost_free (shost);
	ft_share_unref (file);
}

static void search_response (TCPC *c, FTPacket *pkt, FTSearch *srch,
                             FTNode *parent)
{
	FileShare *file  = NULL;
	FTSHost   *shost = NULL;

	/* do not bother handling results that cant be replied to any longer,
	 * and also do not bother with browse searches, which are not valid in
	 * this context */
	if (!srch->event)
		return;

	/* this cant happen with the new search code, but couldve happened with
	 * the old, so we'll leave the test just to be on the safe side */
	assert (!(srch->params.type & FT_SEARCH_HOST));

	/* end-of-search is designated by a completely empty result set (other than
	 * the id we already read off) */
	if (ft_packet_length (pkt) > FT_GUID_SIZE)
	{
		if (!(file = create_result (c, pkt, FALSE, &shost)))
			return;
	}

	/* push to the interface protocol */
	ft_search_reply (srch, file, parent);
	destroy_result (file, shost);
}

static void searchfwd_response (TCPC *c, FTPacket *pkt, FTSearchFwd *sfwd,
                                FTNode *parent)
{
	FTNode        *src;
	FTPacket      *fwd;
	in_addr_t      host;               /* node that owns the result in pkt */
	unsigned char *data;
	size_t         len = 0;

	/* lookup the node that originally requested that we forward this query,
	 * so that we may reply with the new results we just got */
	if (!(src = ft_netorg_lookup (sfwd->src)))
	{
		FT->DBGSOCK (FT, c, "cant find %s, route lost!",
		             net_ip_str (sfwd->src));
		return;
	}

	/* search nodes that respond to their own queries will set host to 0
	 * assuming that we will translate accordingly before we pass it along */
	if ((host = ft_packet_get_ip (pkt)) == 0)
		host = FT_NODE(c)->ip;

	if (!(fwd = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return;

	/* we have to add this part back manually because we've already read it
	 * off the input packet */
	ft_packet_put_ustr   (fwd, sfwd->guid, FT_GUID_SIZE);
	ft_packet_put_ip     (fwd, parent->ip);
	ft_packet_put_uint16 (fwd, parent->port, TRUE);
	ft_packet_put_ip     (fwd, host);

	/* copy the rest of the message verbatim */
	if ((data = ft_packet_get_raw (pkt, &len)))
		ft_packet_put_raw (fwd, data, len);

	ft_packet_send (FT_CONN(src), fwd);
}

FT_HANDLER (ft_search_response)
{
	FTSearch    *srch;
	FTSearchFwd *sfwd;
	ft_guid_t   *guid;
	FTNode      *parent;
	in_addr_t    parent_addr;
	in_port_t    parent_port;

	if (!(FT_NODE(c)->klass & NODE_SEARCH))
		return;

	/* get the semi-unique id to use for lookup */
	if (!(guid = ft_packet_get_ustr (packet, FT_GUID_SIZE)))
	{
		FT->DBGSOCK (FT, c, "bogus search result, no guid");
		return;
	}

	/* grab the parent contact info that responded to this query */
	parent_addr = ft_packet_get_ip (packet);
	parent_port = ft_packet_get_uint16 (packet, TRUE);

	/* if this is the first time the search is being forwarded back, we
	 * need to adjust the header sent out by the parent to reflect their
	 * actual address */
	if (parent_addr == 0)
	{
		parent_addr = FT_NODE(c)->ip;
		parent_port = FT_NODE(c)->port;
	}

	/* permanently register this node information so that we may use an
	 * FTNode handle with the API */
	if (!(parent = ft_node_register (parent_addr)))
	{
		FT->DBGFN (FT, "unable to register %s", net_ip_str (parent_addr));
		return;
	}

	/* do not trust the remote node if we already have information here */
	if (parent->port == 0)
		ft_node_set_port (parent, parent_port);
	else if (parent->port != parent_port)
	{
		/* give myself a little warning for when the karma system is
		 * implemented */
		FT->DBGSOCK (FT, c, "node may have just tried to lie to us");
	}

	/*
	 * First look for a local search that matches this id, meaning that this
	 * node is replying directly to us.  I think it would be wise to double
	 * check that the node we are receiving results from is one that we
	 * originally sent the request to.  Unfortunately, we cant use the
	 * waiting_on dataset as forwarded results will come after this node has
	 * finished its directly reply to us.  Perhaps another similar data
	 * structure should be used.
	 *
	 * If that fails, try to find a forward request that we may have made to
	 * this node.  In that event, we will simply copy the results verbatim
	 * back to source of the forward (the node that is respoding to us now is
	 * considered the destination).
	 */
	if ((srch = ft_search_find (guid)))
		search_response (c, packet, srch, parent);
	else if ((sfwd = ft_search_fwd_find (guid, FT_NODE(c)->ip)))
		searchfwd_response (c, packet, sfwd, parent);
	else
	{
		FT->DBGSOCK (FT, c, "search result not locally matched, ignoring");
		return;
	}
}

/*****************************************************************************/

static int send_browse (Dataset *d, DatasetNode *node, struct sreply *reply)
{
	FTPacket  *p;
	FileShare *file;
	Hash      *hash;

	if (!(file = node->value))
		return FALSE;

	assert (SHARE_DATA(file)->hpath != NULL);

	if (!(p = ft_packet_new (FT_BROWSE_RESPONSE, 0)))
		return FALSE;

	if (!(hash = share_hash_get (file, "MD5")))
		return TRUE;

	ft_packet_put_ustr   (p, reply->guid, FT_GUID_SIZE);
	ft_packet_put_uint32 (p, upload_availability (), TRUE);   /* TODO: tmp */
	ft_packet_put_uint32 (p, (uint32_t)file->size, TRUE);
	ft_packet_put_ustr   (p, hash->data, hash->len);
	ft_packet_put_str    (p, file->mime);
	ft_packet_put_str    (p, SHARE_DATA(file)->hpath);

	meta_foreach (file, DATASET_FOREACH (add_meta), p);

	assert (reply->stream != NULL);
	ft_stream_send (reply->stream, p);

	return TRUE;
}

FT_HANDLER (ft_browse_request)
{
	struct sreply *reply;
	FTPacket      *pkt;
	ft_guid_t     *guid;

	if (!(guid = ft_packet_get_ustr (packet, FT_GUID_SIZE)))
		return;

	reply = sreply_new (c, 0, guid, ft_stream_get (c, FT_STREAM_SEND, NULL));
	if (!reply)
		return;

	/* send shares */
	share_foreach (DATASET_FOREACH (send_browse), reply);
	sreply_free (reply);

	/* terminate */
	if (!(pkt = ft_packet_new (FT_BROWSE_RESPONSE, 0)))
		return;

	ft_packet_put_ustr (pkt, guid, FT_GUID_SIZE);
	ft_packet_send (c, pkt);
}

FT_HANDLER (ft_browse_response)
{
	FTBrowse  *browse;
	FileShare *file  = NULL;
	FTSHost   *shost = NULL;
	ft_guid_t *guid;

	if (!(guid = ft_packet_get_ustr (packet, FT_GUID_SIZE)))
		return;

	if (!(browse = ft_browse_find (guid, FT_NODE(c)->ip)))
		return;

	if (ft_packet_length (packet) > FT_GUID_SIZE)
	{
		if (!(file = create_result (c, packet, TRUE, &shost)))
			return;
	}

	ft_browse_reply (browse, file);
	destroy_result (file, shost);
}

/*****************************************************************************/

FT_HANDLER (ft_push_request)
{
	in_addr_t  ip;
	in_port_t  port;
	char      *file;
	off_t      start;
	off_t      stop;

	ip    =        ft_packet_get_ip     (packet);
	port  =        ft_packet_get_uint16 (packet, TRUE);
	file  =        ft_packet_get_str    (packet);
	start = (off_t)ft_packet_get_uint32 (packet, TRUE);
	stop  = (off_t)ft_packet_get_uint32 (packet, TRUE);

	/* ip || port == 0 in this instance means that we are to send this file
	 * back to the search node contacting us here */
	if (ip == 0 || port == 0)
	{
		ip   = FT_NODE(c)->ip;
		port = FT_NODE(c)->http_port;
	}

	/* sanity check on the data received */
	if (!file || stop == 0)
	{
		FT->DBGSOCK (FT, c, "incomplete request");
		return;
	}

	/* make an outgoing http connection and advertise that we can fulfill
	 * the push request */
	http_client_push (ip, port, file, start, stop);
}

FT_HANDLER (ft_push_forward)
{
	FTPacket *fwd;
	in_addr_t ip;
	char     *request;
	uint32_t  start;
	uint32_t  stop;

	ip      = ft_packet_get_ip     (packet);
	request = ft_packet_get_str    (packet);
	start   = ft_packet_get_uint32 (packet, TRUE);
	stop    = ft_packet_get_uint32 (packet, TRUE);

	if (!request || stop == 0)
	{
		FT->DBGSOCK (FT, c, "incompleted request");
		return;
	}

	if (!(fwd = ft_packet_new (FT_PUSH_REQUEST, 0)))
		return;

	/* reconstruct the complete request using the directly connected peers
	 * address information */
	ft_packet_put_ip     (fwd, FT_NODE(c)->ip);
	ft_packet_put_uint16 (fwd, FT_NODE(c)->http_port, TRUE);

	/* put the original data back on */
	ft_packet_put_str    (fwd, request);
	ft_packet_put_uint32 (fwd, start, TRUE);
	ft_packet_put_uint32 (fwd, stop, TRUE);

	/*
	 * Pass this data along to the user who will be actually sending the
	 * file to the user making this request.
	 *
	 * NOTE:
	 *
	 * No further relayed communication will take place.  If successful, the
	 * node will connect back to the user and advertise the file, otherwise
	 * it will just be dropped off into oblivion right here.  Life is rough,
	 * eh? :)
	 */
	ft_packet_sendto (ip, fwd);
}
