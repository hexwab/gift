/*
 * ft_protocol.c
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

#include "ft_openft.h"

#include "ft_netorg.h"

#include "ft_xfer.h"

#include "file.h"
#include "ft_search.h"
#include "ft_search_exec.h"
#include "parse.h"

#include "md5.h"
#include "meta.h"

/*****************************************************************************/

#define MAX_CHILDREN config_get_int (OPENFT->conf, "search/children=500")

/*****************************************************************************/

#define MIN_VER_USERS 2
#define MAX_VER_USERS 500

/* used to hastle people who dont upgrade */
static Dataset *ver_upgrade = NULL;

/*****************************************************************************/

typedef void (*ProtocolHandler) (Protocol *p, Connection *c, FTPacket *packet);
#define FT_HANDLER(func) static void ft_##func (Protocol *p, Connection *c, FTPacket *packet)

FT_HANDLER (version_request);
FT_HANDLER (version_response);
FT_HANDLER (nodeinfo_request);
FT_HANDLER (nodeinfo_response);
FT_HANDLER (nodelist_request);
FT_HANDLER (nodelist_response);
FT_HANDLER (nodecap_request);
FT_HANDLER (nodecap_response);
FT_HANDLER (ping_request);
FT_HANDLER (ping_response);
FT_HANDLER (session_request);
FT_HANDLER (session_response);

FT_HANDLER (child_request);
FT_HANDLER (child_response);
FT_HANDLER (addshare_request);
FT_HANDLER (addshare_response);
FT_HANDLER (remshare_request);
FT_HANDLER (remshare_response);
FT_HANDLER (modshare_request);
FT_HANDLER (modshare_response);
FT_HANDLER (stats_request);
FT_HANDLER (stats_response);

FT_HANDLER (search_request);
FT_HANDLER (search_response);
FT_HANDLER (browse_request);
FT_HANDLER (browse_response);

FT_HANDLER (push_request);
FT_HANDLER (push_response);

/*****************************************************************************/

struct _handler_table
{
	ft_uint16       command;
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
	{ FT_PUSH_RESPONSE,     ft_push_response        },

	{ 0,                    NULL                    }
};

/*****************************************************************************/

static int handle_command (Protocol *p, Connection *c, FTPacket *packet)
{
	struct _handler_table *handler;
	ft_uint16 cmd;

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

	GIFT_ERROR (("[%s] found no handler for cmd=0x%04x len=0x%04x",
	             net_ip_str (FT_NODE(c)->ip), packet->command, packet->len));

	return FALSE;
}

static void handle_stream_pkt (FTStream *stream, FTPacket *stream_pkt,
                               Connection *c)
{
	handle_command (openft_p, c, stream_pkt);
}

static int handle_stream (Protocol *p, Connection *c, FTPacket *packet)
{
	FTStream *stream;

	if (!(stream = ft_stream_get (c, FT_STREAM_RECV, packet)))
		return FALSE;

	ft_stream_recv (stream, packet, (FTStreamRecv) handle_stream_pkt, c);

	if (stream->eof)
		ft_stream_finish (stream);

	return TRUE;
}

int protocol_handle (Protocol *p, Connection *c, FTPacket *packet)
{
	ft_uint16 cmd;

	if (!packet)
		return FALSE;

	cmd = ft_packet_command (packet);

	if (ft_packet_flags (packet) & FT_PACKET_STREAM)
		return handle_stream (p, c, packet);

	return handle_command (p, c, packet);
}

/*****************************************************************************/

FT_HANDLER (version_request)
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

static void version_outdated (Connection *c)
{
	char *ver_str;
	ft_uint8 major = 0;
	ft_uint8 minor = 0;
	ft_uint8 micro = 0;
	ft_uint8 rev   = 0;

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

	openft_p->message (openft_p, generate_msg (ver_upgrade));
}

FT_HANDLER (version_response)
{
	ft_uint8 major;
	ft_uint8 minor;
	ft_uint8 micro;
	ft_uint8 rev;

	major = (ft_uint8)ft_packet_get_uint16 (packet, TRUE);
	minor = (ft_uint8)ft_packet_get_uint16 (packet, TRUE);
	micro = (ft_uint8)ft_packet_get_uint16 (packet, TRUE);
	rev   = (ft_uint8)ft_packet_get_uint16 (packet, TRUE);

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

FT_HANDLER (nodeinfo_request)
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

static void pending_packets (in_addr_t ip, unsigned short port,
                             unsigned short http_port, unsigned short klass)
{
	FTNode *node;

	/* we never registered this node, so this data must have been sent to us
	 * in error */
	if (!(node = ft_netorg_lookup (ip)))
		return;

	/* again, we didn't request packets be scheduled here, so we're not going
	 * to proceed any further */
	if (!node->session_queue)
		return;

	if (port == 0)
	{
		TRACE (("TODO: unable to redirect packets to firewalled users (%s)",
		        net_ip_str (ip)));
		return;
	}

	/* set connection parameters */
	ft_node_set_port      (node, port);
	ft_node_set_http_port (node, http_port);
	ft_node_set_class     (node, klass);

	/* actually make the outgoing connection */
	TRACE (("scheduling outgoing connection to %s:%hu",
	        net_ip_str (ip), port));
	ft_session_connect (node);
}

FT_HANDLER (nodeinfo_response)
{
	in_addr_t      ip;
	unsigned short klass;
	unsigned short port;
	unsigned short http_port;
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

static int nodelist_send (FTNode *node, Connection *c)
{
	unsigned short *p;

	/* avoid giving the user his ip address back */
	if (node == FT_NODE(c))
		return FALSE;

	/* make sure we don't give this user the same node over and over again */
	p = dataset_lookup (FT_SESSION(c)->nodelist, &node->ip, sizeof (node->ip));
	if (p && *p == node->port)
		return FALSE;

	if (!FT_SESSION(c)->nodelist)
		FT_SESSION(c)->nodelist = dataset_new (DATASET_HASH);

	dataset_insert (&FT_SESSION(c)->nodelist, &node->ip, sizeof (node->ip),
	                &node->port, sizeof (node->port));

	/* send the nodelist entry */
	ft_packet_sendva (c, FT_NODELIST_RESPONSE, 0, "Ihh",
	                  node->ip, node->port, (unsigned short)node->klass);
	return TRUE;
}

FT_HANDLER (nodelist_request)
{
	int   nodes;

	nodes = ft_netorg_foreach (NODE_SEARCH | NODE_INDEX, NODE_CONNECTED, 15,
	                           FT_NETORG_FOREACH(nodelist_send), c);

	/* we weren't able to come up with enough search/index nodes, try to give
	 * them at least something */
	if (nodes < 10)
	{
		nodes += ft_netorg_foreach (NODE_USER, NODE_CONNECTED, 15,
		                            FT_NETORG_FOREACH(nodelist_send), c);
	}

	/* eof */
	ft_packet_sendva (c, FT_NODELIST_RESPONSE, 0, NULL);

	TRACE (("%s: gave %i nodes", ft_node_fmt (FT_NODE(c)), nodes));
}

FT_HANDLER (nodelist_response)
{
	FTNode   *node;
	ft_uint32 ip;
	ft_uint16 port;
	ft_uint16 klass;
	int       conns;

	/* nodelist EOF */
	if (packet->len == 0)
		return;

	ip    = ft_packet_get_ip     (packet);
	port  = ft_packet_get_uint16 (packet, TRUE);
	klass = ft_packet_get_uint16 (packet, TRUE);

	if (!(node = ft_node_register (ip)))
		return;

	/* we already have this information straight from the user, no need
	 * to run through this */
	if (node->session)
		return;

	ft_node_set_port  (node, port);
	ft_node_set_class (node, klass);

	conns  = ft_netorg_length (NODE_USER, NODE_CONNECTING);
	conns += ft_netorg_length (NODE_USER, NODE_CONNECTED);

	if (conns < FT_MAX_CONNECTIONS)
		ft_session_connect (node);
}

/*****************************************************************************/

static int add_nodecap (Dataset *d, DatasetNode *node, FTPacket *packet)
{
	ft_packet_put_uint16 (packet, 1, TRUE);
	ft_packet_put_str (packet, node->key);
	return FALSE;
}

FT_HANDLER (nodecap_request)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_NODECAP_RESPONSE, 0)))
		return;

	dataset_foreach (FT_SELF->session->cap,
	                 DATASET_FOREACH (add_nodecap), pkt);

	ft_packet_put_uint16 (pkt, 0, TRUE);
	ft_packet_send (c, pkt);
}

FT_HANDLER (nodecap_response)
{
	ft_uint16 key_id;
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

FT_HANDLER (ping_request)
{
	FT_SESSION(c)->heartbeat += 2;
	ft_packet_sendva (c, FT_PING_RESPONSE, 0, NULL);
}

FT_HANDLER (ping_response)
{
	FT_SESSION(c)->heartbeat += 2;
}

/*****************************************************************************/

/*
 * Session negotiation is incomplete.  These functions very lazily trust the
 * assumptions laid out by the handshaking stage.  Fix later.
 */
FT_HANDLER (session_request)
{
	if (FT_SESSION(c)->stage != 3)
		return;

	ft_session_stage (c, 3);
}

FT_HANDLER (session_response)
{
	ft_uint16 reply;

	if (FT_SESSION(c)->stage != 3)
		return;

	reply = ft_packet_get_uint16 (packet, TRUE);

	if (reply)
		ft_session_stage (c, 3);
}

/*****************************************************************************/

FT_HANDLER (child_request)
{
	unsigned short response;

	/* user has requested to be our child, lets hope we're a search node ;) */
	if (!(FT_SELF->klass & NODE_SEARCH))
		return;

	/* the final stage in negotiation
	 * NOTE: its here because it prevents a race condition in the child */
	if (packet->len > 0)
	{
		ft_uint16 reply;

		reply = ft_packet_get_uint16 (packet, TRUE);

		if (reply)
			ft_node_add_class (FT_NODE(c), NODE_CHILD);
		else
			ft_node_remove_class (FT_NODE(c), NODE_CHILD);

		return;
	}

	/* if we have fewer than MAX_CHILDREN, response is TRUE */
	response = (ft_netorg_length (NODE_CHILD, NODE_CONNECTED) < MAX_CHILDREN);

	/* reply to the child-status request */
	ft_packet_sendva (c, FT_CHILD_RESPONSE, 0, "h", response);
}

FT_HANDLER (child_response)
{
	ft_uint16 response;

	if (!(FT_NODE(c)->klass & NODE_SEARCH))
		return;

	response = ft_packet_get_uint16 (packet, TRUE);

	/* they refused our request */
	if (!response)
	{
		TRACE_SOCK (("request refused"));
		ft_node_remove_class (FT_NODE(c), NODE_PARENT);
		return;
	}

	/* figure out if we still need them */
	if (!validate_share_submit ())
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

static int is_child (Connection *c)
{
	if (!(FT_SELF->klass & NODE_SEARCH))
		return FALSE;

	return (FT_NODE(c)->klass & NODE_CHILD);
}

FT_HANDLER (addshare_request)
{
	ft_uint32      size;
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

FT_HANDLER (addshare_response)
{
}

/*****************************************************************************/

FT_HANDLER (remshare_request)
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

FT_HANDLER (remshare_response)
{
}

/*****************************************************************************/

static int submit_digest_index (FTNode *node, Connection *child)
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
static void submit_digest (Connection *c)
{
	ft_netorg_foreach (NODE_INDEX, NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(submit_digest_index), c);
}

FT_HANDLER (modshare_request)
{
	ft_uint16 trans_comp;
	ft_uint32 avail;

	if (!is_child (c))
		return;

	trans_comp = ft_packet_get_uint16 (packet, TRUE);
	avail      = ft_packet_get_uint32 (packet, TRUE);

	if (trans_comp)
	{
		ft_shost_sync (ft_shost_get (FT_NODE(c)->ip));
		submit_digest (c);
	}

	ft_shost_avail (FT_NODE(c)->ip, (unsigned long)avail);
}

FT_HANDLER (modshare_response)
{
}

/*****************************************************************************/

/* multipurpose function.  may either submit a users digest to an INDEX node
 * or retrieve the total shares an INDEX node is aware of */
FT_HANDLER (stats_request)
{
	ft_uint16 request;

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
			ft_uint32     user;   /* user who owns these files */
			ft_uint32     shares;
			ft_uint32     size;   /* MB */

			user   = ft_packet_get_ip     (packet);
			shares = ft_packet_get_uint32 (packet, TRUE);
			size   = ft_packet_get_uint32 (packet, TRUE);

			/* handle processing of this data */
			ft_stats_add (FT_NODE(c)->ip, user, shares, size);
		}
		break;
	 case 3: /* REMOVE USER SHARES REPORT */
		{
			ft_uint32 user;

			user = ft_packet_get_ip (packet);

			ft_stats_remove (FT_NODE(c)->ip, user);
		}
		break;
	 default:
		break;
	}
}

FT_HANDLER (stats_response)
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
struct _search_reply
{
	ft_uint32  id;                     /* remote search id */
	FTStream  *stream;                 /* stream context */
};

struct _search_reply *search_reply_new (ft_uint32 id, FTStream *stream)
{
	struct _search_reply *reply;

	if (!(reply = malloc (sizeof (struct _search_reply))))
		return NULL;

	reply->id = id;
	reply->stream = stream;

	return reply;
}

void search_reply_free (struct _search_reply *reply)
{
	ft_stream_finish (reply->stream);
	free (reply);
}

/*****************************************************************************/

static int add_meta (Dataset *d, DatasetNode *node, FTPacket *p)
{
	ft_packet_put_str (p, node->key);
	ft_packet_put_str (p, node->value);
	return FALSE;
}

static void send_result (Connection *c, struct _search_reply *reply,
                         FileShare *file, FTSHost *shost)
{
	FTPacket  *p;
	ShareHash *sh;
	char      *path;

	if (!(sh = share_hash_get (file, "MD5")))
		return;

	/* if an hpath was supplied (local share) we want to keep the
	 * output consistent */
	path = (SHARE_DATA(file)->hpath ?
	        SHARE_DATA(file)->hpath : SHARE_DATA(file)->path);

	/* begin packet */
	if (!(p = ft_packet_new (FT_SEARCH_RESPONSE, 0)))
		return;

	ft_packet_put_uint32 (p, reply->id, TRUE);
	ft_packet_put_ip     (p, shost->host);
	ft_packet_put_uint16 (p, shost->ft_port, TRUE);
	ft_packet_put_uint16 (p, shost->http_port, TRUE);
	ft_packet_put_str    (p, shost->alias);
	ft_packet_put_uint32 (p, shost->availability, TRUE);
	ft_packet_put_uint32 (p, (ft_uint32)file->size, TRUE);
	ft_packet_put_ustr   (p, sh->hash, sh->len);
	ft_packet_put_str    (p, file->mime);
	ft_packet_put_str    (p, path);

	meta_foreach (file, DATASET_FOREACH (add_meta), p);

	if (reply->stream)
		ft_stream_send (reply->stream, p);
	else
		ft_packet_send (c, p);
}

static int search_request_result (Connection *c, FileShare *file,
                                  struct _search_reply *reply)
{
	FTShare *share;

	if (!file)
	{
		ft_uint32 id = reply->id;
		search_reply_free (reply);
		ft_packet_sendva (c, FT_SEARCH_RESPONSE, 0, "l", id);

		return FALSE;
	}

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	/* dont send shares they submitted
	 * NOTE: if share->host_share is NULL, the user has disconnected and
	 * share hasnt been freed because it still exists in use here */
	if (share->shost && FT_NODE(c)->ip != share->shost->host)
		send_result (c, reply, file, share->shost);

	return FALSE;
}

static int search_request_result_free (Connection *c, FileShare *file,
                                       struct _search_reply *reply)
{
	FTShare *share;

	if (!file)
		return FALSE;

	/* remove temporary list of active files from this node.  for more details
	 * as to why this is stored, see search_exec.c:add_result */
	if ((share = share_lookup_data (file, "OpenFT")) && share->shost)
		share->shost->files = list_remove (share->shost->files, file);

	ft_share_unref (file);
	return FALSE;
}

FT_HANDLER (search_request)
{
	struct _search_reply *reply;
	List      *ptr;
	ft_uint32  id;
	ft_uint16  type;
	void      *query;
	void      *exclude;
	char      *query_str;
	char      *exclude_str;
	char      *realm;
	size_t     results = 0;

	if (!(FT_SELF->klass & NODE_SEARCH))
		return;

	id   = ft_packet_get_uint32 (packet, TRUE);
	type = ft_packet_get_uint16 (packet, TRUE);

	if (type & FT_SEARCH_HIDDEN)
	{
		query   = ft_packet_get_array (packet, 4, 0, TRUE);
		exclude = ft_packet_get_array (packet, 4, 0, TRUE);

		query_str   = "*hidden*";
		exclude_str = "";
	}
	else
	{
		query   = ft_packet_get_str (packet);
		exclude = ft_packet_get_str (packet);

		query_str   = query;
		exclude_str = exclude;
	}

	realm = ft_packet_get_str (packet);

	/* execute the search
	 * TODO: this blocks and that is bad */
	ptr = ft_search (&results, type | FT_SEARCH_LOCAL, realm, query, exclude);

	if (type & FT_SEARCH_FILENAME)
	{
		TRACE_SOCK (("[%u:%hu]: '%s' (%s/%s)...%i result(s)",
		             id, type, query_str, exclude_str, realm, results));
	}

	if (!(reply = search_reply_new (id, NULL)))
		return;

	/* only use stream compression if we have a useful amount of results */
	if (results >= 5)
		reply->stream = ft_stream_get (c, FT_STREAM_SEND, NULL);

	/* ptr will be taken care of by queue_add */
	queue_add (c,
	           (QueueWriteFunc) search_request_result,
	           (QueueWriteFunc) search_request_result_free,
	           ptr, reply);
}

static FTSHost *create_share_host (in_addr_t host,
                                   unsigned short port, unsigned short http_port,
                                   char *alias, unsigned long avail)
{
	FTSHost *shost;

	if (!(shost = ft_shost_new (TRUE, host, port, http_port, alias)))
		return NULL;

	shost->availability = avail;

	return shost;
}

static FileShare *create_result (Connection *c, FTPacket *packet, int browse,
                                 FTSHost **shost)
{
	FileShare     *file;
	ft_uint32      host;
	ft_uint16      port;
	ft_uint16      http_port;
	char          *alias;
	ft_uint32      avail;
	ft_uint32      size;
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
		TRACE_SOCK (("unable to create result"));
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

FT_HANDLER (search_response)
{
	FTSearch   *search;
	FileShare  *file  = NULL;
	FTSHost    *shost = NULL;
	ft_uint32   id;

	if (!(FT_NODE(c)->klass & NODE_SEARCH))
		return;

	id = ft_packet_get_uint32 (packet, TRUE);

	/* host searches are invalid in this context */
	if (!(search = ft_event_id_data (id, "FTSearch")) ||
	    (search->type & FT_SEARCH_HOST))
		return;

	/* packet->len == 4 when an EOF came in */
	if (packet->len > 4)
	{
		if (!(file = create_result (c, packet, FALSE, &shost)))
			return;
	}

	/* push to the interface protocol */
	ft_search_reply (search, c, file);
	destroy_result (file, shost);
}

/*****************************************************************************/

static int send_browse (Dataset *d, DatasetNode *node,
                        struct _search_reply *reply)
{
	FTPacket  *p;
	FileShare *file;
	ShareHash *sh;

	if (!(file = node->value))
		return FALSE;

	assert (SHARE_DATA(file)->hpath != NULL);

	if (!(p = ft_packet_new (FT_BROWSE_RESPONSE, 0)))
		return FALSE;

	if (!(sh = share_hash_get (file, "MD5")))
		return TRUE;

	ft_packet_put_uint32 (p, reply->id, TRUE);
	ft_packet_put_uint32 (p, upload_availability (), TRUE);   /* TODO: tmp */
	ft_packet_put_uint32 (p, (ft_uint32)file->size, TRUE);
	ft_packet_put_ustr   (p, sh->hash, sh->len);
	ft_packet_put_str    (p, file->mime);
	ft_packet_put_str    (p, SHARE_DATA(file)->hpath);

	meta_foreach (file, DATASET_FOREACH (add_meta), p);

	assert (reply->stream != NULL);
	ft_stream_send (reply->stream, p);

	return TRUE;
}

FT_HANDLER (browse_request)
{
	struct _search_reply *reply;
	ft_uint32 id;

	id = ft_packet_get_uint32 (packet, TRUE);

	if (!(reply = search_reply_new (id, ft_stream_get (c, FT_STREAM_SEND, NULL))))
		return;

	/* send shares */
	share_foreach (DATASET_FOREACH (send_browse), reply);
	search_reply_free (reply);

	/* terminate */
	ft_packet_sendva (c, FT_BROWSE_RESPONSE, 0, "l", id);
}

FT_HANDLER (browse_response)
{
	FTSearch  *browse;
	FileShare *file  = NULL;
	FTSHost   *shost = NULL;
	ft_uint32  id;

	id = ft_packet_get_uint32 (packet, TRUE);

	if (!(browse = ft_event_id_data (id, "FTSearch")) ||
	    browse->type != FT_SEARCH_HOST)
		return;

	assert (net_ip (browse->query) == FT_NODE(c)->ip);

	if (ft_packet_length (packet) > 4)
	{
		if (!(file = create_result (c, packet, TRUE, &shost)))
			return;
	}

	ft_search_reply (browse, OPENFT->ft, file);
	destroy_result (file, shost);
}

/*****************************************************************************/

FT_HANDLER (push_request)
{
	ft_uint32  ip;
	ft_uint16  port;
	char      *file;
	ft_uint32  start;
	ft_uint32  stop;

	ip    = ft_packet_get_ip     (packet);
	port  = ft_packet_get_uint16 (packet, TRUE);
	file  = ft_packet_get_str    (packet);
	start = ft_packet_get_uint32 (packet, TRUE);
	stop  = ft_packet_get_uint32 (packet, TRUE);

	/* ip || port == 0 in this instance means that we are to send this file
	 * back to the search node contacting us here */
	if (!ip || !port)
	{
		ip   = FT_NODE(c)->ip;
		port = FT_NODE(c)->http_port;
	}

	if (!file || !stop)
	{
		TRACE_SOCK (("incomplete request"));
		return;
	}

#if 0
	TRACE_SOCK (("received push request for %s:%hu, %s (%u - %u)",
	             net_ip_str (ip), port, file,
	             (unsigned int) start,
	             (unsigned int) stop));
#endif

	http_client_push (ip, port, file, (off_t) start, (off_t) stop);
}

/* NOTE: the usage of push_response is temporary in order to at least
 * fake backwards compatibility */
FT_HANDLER (push_response)
{
	FTPacket *pkt;
	ft_uint32 ip;
	ft_uint16 port;
	char     *request;
	ft_uint32 start;
	ft_uint32 stop;

	ip      = ft_packet_get_ip     (packet);
	port    = ft_packet_get_uint16 (packet, TRUE); /* unused */
	request = ft_packet_get_str    (packet);
	start   = ft_packet_get_uint32 (packet, TRUE);
	stop    = ft_packet_get_uint32 (packet, TRUE);

	if (!request)
	{
		TRACE_SOCK (("incompleted request"));
		return;
	}

#if 0
	TRACE_SOCK (("relaying push request to %s:%hu for %s",
				 net_ip_str (ip), port, request));
#endif

	if (!(pkt = ft_packet_new (FT_PUSH_REQUEST, 0)))
		return;

	ft_packet_put_ip     (pkt, FT_NODE(c)->ip);
	ft_packet_put_uint16 (pkt, FT_NODE(c)->http_port, TRUE);
	ft_packet_put_str    (pkt, request);
	ft_packet_put_uint32 (pkt, start, TRUE);
	ft_packet_put_uint32 (pkt, stop, TRUE);

	/* pass this data along to the user who will be actually sending the
	 * file */
	ft_packet_sendto (ip, pkt);
}
