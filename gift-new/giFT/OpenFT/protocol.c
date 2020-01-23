/*
 * protocol.c
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

#include "openft.h"

#include "netorg.h"

#include "xfer.h"

#include "file.h"
#include "search.h"
#include "parse.h"

#ifdef USE_ZLIB
# include "share_comp.h"
#endif /* USE_ZLIB */

/*****************************************************************************/

extern Config *openft_conf;
extern Connection *ft_self;

/*****************************************************************************/

#define MAX_CHILDREN config_get_int (openft_conf, "search/children=500")

/*****************************************************************************/

typedef void (*ProtocolHandler) (Protocol *p, Connection *c, FTPacket *packet);
#define FT_HANDLER(func) static void ft_##func (Protocol *p, Connection *c, FTPacket *packet)

FT_HANDLER (version_request);
FT_HANDLER (version_response);

FT_HANDLER (version_request_003);
FT_HANDLER (version_response_003);

FT_HANDLER (class_request);
FT_HANDLER (class_response);

FT_HANDLER (child_request);
FT_HANDLER (child_response);

FT_HANDLER (share_request);
FT_HANDLER (share_response);

FT_HANDLER (search_request);
FT_HANDLER (search_response);

FT_HANDLER (nodelist_request);
FT_HANDLER (nodelist_response);

FT_HANDLER (push_request);
FT_HANDLER (push_response);

FT_HANDLER (nodeinfo_request);
FT_HANDLER (nodeinfo_response);

FT_HANDLER (stats_request);
FT_HANDLER (stats_response);

FT_HANDLER (ping_request);
FT_HANDLER (ping_response);

FT_HANDLER (modshare_request);
FT_HANDLER (modshare_response);

FT_HANDLER (nodecap_request);
FT_HANDLER (nodecap_response);

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

	/* attempt to maintain some minor (and temporary) backwards compatibility
	 * with OpenFT 0.0.3 nodes. */
	{ 0x0012,               ft_version_request_003  },
	{ 0x0013,               ft_version_response_003 },

	{ FT_CLASS_REQUEST,     ft_class_request        },
	{ FT_CLASS_RESPONSE,    ft_class_response       },

	{ FT_CHILD_REQUEST,     ft_child_request        },
	{ FT_CHILD_RESPONSE,    ft_child_response       },

	{ FT_SHARE_REQUEST,     ft_share_request        },
	{ FT_SHARE_RESPONSE,    ft_share_response       },

	{ FT_SEARCH_REQUEST,    ft_search_request       },
	{ FT_SEARCH_RESPONSE,   ft_search_response      },

	{ FT_NODELIST_REQUEST,  ft_nodelist_request     },
	{ FT_NODELIST_RESPONSE, ft_nodelist_response    },

	{ FT_PUSH_REQUEST,      ft_push_request         },
	{ FT_PUSH_RESPONSE,     ft_push_response        },

	{ FT_NODEINFO_REQUEST,  ft_nodeinfo_request     },
	{ FT_NODEINFO_RESPONSE, ft_nodeinfo_response    },

	{ FT_STATS_REQUEST,     ft_stats_request        },
	{ FT_STATS_RESPONSE,    ft_stats_response       },

	{ FT_PING_REQUEST,      ft_ping_request         },
	{ FT_PING_RESPONSE,     ft_ping_response        },

	{ FT_MODSHARE_REQUEST,  ft_modshare_request     },
	{ FT_MODSHARE_RESPONSE, ft_modshare_response    },

	{ FT_NODECAP_REQUEST,   ft_nodecap_request      },
	{ FT_NODECAP_RESPONSE,  ft_nodecap_response     },

	{ 0,                    NULL                    }
};

/*****************************************************************************/

int protocol_handle_command (Protocol *p, Connection *c, FTPacket *packet)
{
	struct _handler_table *handler;
	ft_uint16              cmd;

	if (!packet)
		return FALSE;

#if 0
	GIFT_TRACE (("cmd=%hu len=%hu", packet->command, packet->len));
	TRACE_MEM (packet->data, MIN(256, packet->len));
#endif

	cmd = packet->command & ~FT_PACKET_COMPRESSED;

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
	             net_ip_str (NODE (c)->ip), packet->command, packet->len));

	return FALSE;
}

/*****************************************************************************/

FT_HANDLER (version_request)
{
	ft_packet_send (c, FT_VERSION_RESPONSE, "%hu%hu%hu",
	                OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO);
}

FT_HANDLER (version_response)
{
	ft_uint16 major;
	ft_uint16 minor;
	ft_uint16 micro;
	ft_uint32 ver_remote;
	ft_uint32 ver_local;

	major = ft_packet_get_int (packet, 2, TRUE);
	minor = ft_packet_get_int (packet, 2, TRUE);
	micro = ft_packet_get_int (packet, 2, TRUE);

	/* apply some magic */
	ver_remote = ft_make_version (major, minor, micro);
	ver_local  = ft_make_version (OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO);

	NODE (c)->version = ver_remote;

	/* only annoy users w/ an older version */
	if (ver_remote > ver_local)
	{
		TRACE_SOCK (("new protocol version (%hu.%hu.%hu) -- you are using %hu.%hu.%hu",
		             major, minor, micro,
		             OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO));
	}

	/* if this is actually older, disconnect them */
	if (ver_remote < ver_local)
	{
		/* reset the vitality of this host */
		NODE (c)->vitality = 0;

		/* force a response so that this node understands why we just
		 * disconnected them */
		ft_version_request (p, c, packet);

		node_disconnect (c);
		return;
	}

	/* finish handshake */
	if (ver_remote == ver_local)
	{
	}
}

/*****************************************************************************/
/* TEMPORARY */

FT_HANDLER (version_request_003)
{
	ft_version_response_003 (p, c, packet);
}

FT_HANDLER (version_response_003)
{
	/* disconnect the 0.0.3 user */
	NODE (c)->vitality = 0;

	ft_packet_send (c, 0x0013, "%hu%hu%hu",
	                OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO);

	node_disconnect (c);
}

/*****************************************************************************/

FT_HANDLER (class_request)
{
	ft_packet_send (c, FT_CLASS_RESPONSE, "%hu", NODE (ft_self)->class);
}

FT_HANDLER (class_response)
{
	ft_uint16 klass;

	if (packet->len < 2)
		return;

	klass = ft_packet_get_int (packet, 2, TRUE);

	/* preserve previous context-specific status */
	if (NODE (c)->class & NODE_CHILD)
		klass |= NODE_CHILD;

	if (klass & NODE_SEARCH && NODE (c)->class & NODE_PARENT)
		klass |= NODE_PARENT;

	node_class_set (c, klass);
}

/*****************************************************************************/

FT_HANDLER (child_request)
{
	unsigned short response;

	/* user has requested to be our child, lets hope we're a search node ;) */
	if (!(NODE (ft_self)->class & NODE_SEARCH))
		return;

	/* the final stage in negotiation
	 * NOTE: its here because it prevents a race condition in the child */
	if (packet->len > 0)
	{
		ft_uint16 reply;

		reply = ft_packet_get_int (packet, 2, TRUE);

		if (reply)
			node_class_add (c, NODE_CHILD);
		else
			node_class_remove (c, NODE_CHILD);

		return;
	}

	/* if we have fewer than MAX_CHILDREN, response is TRUE */
	response = (conn_length (NODE_CHILD, NODE_CONNECTED) < MAX_CHILDREN);

	ft_packet_send (c, FT_CHILD_RESPONSE, "%hu", response);
}

FT_HANDLER (child_response)
{
	ft_uint16   response;

	if (!(NODE (c)->class & NODE_SEARCH))
		return;

	response = ft_packet_get_int (packet, 2, TRUE);

	/* they refused our request */
	if (!response)
	{
		TRACE_SOCK (("request refused"));
		node_class_remove (c, NODE_PARENT);
		return;
	}

	/* figure out if we still need them */
	if (!validate_share_submit ())
	{
		/* gracefully inform this node that we have decided not to accept
		 * them as our parent */
		ft_packet_send (c, FT_CHILD_REQUEST, "%hu", FALSE);

		return;
	}

	/* accept */
	ft_packet_send (c, FT_CHILD_REQUEST, "%hu", TRUE);

	if (NODE (c)->class & NODE_PARENT)
		return;

	node_class_add (c, NODE_PARENT);

	/* we have a new parent, submit our shares and once complete
	 * ft_share_local_submit will notify them of our sharing eligibility so
	 * that they may report it to users searching for our files */
	ft_share_local_submit (c);
}

/*****************************************************************************/

static Connection *submit_share_digest (Connection *c, Node *node,
                                        Connection *child)
{
	unsigned long shares = 0;
	double        size   = 0.0; /* MB */

	ft_share_stats_get_digest (NODE (child)->ip, &shares, &size, NULL, NULL);

	/* submit the digest */
	ft_packet_send (c, FT_STATS_REQUEST, "%hu+I%lu%lu",
	                2 /* submit digest */,
	                NODE (child)->ip, shares, (unsigned long) size);

	return NULL;
}

FT_HANDLER (share_request)
{
	/* we need to be a search node and they need to be our child already */
	if (!(NODE (ft_self)->class & NODE_SEARCH) &&
	    !(NODE (c)->class & NODE_CHILD))
	{
		return;
	}

	/* finished submitting files, load from cache */
	if (packet->len == 0)
	{
		FT_HostShare *h_share;

		/* hmm, whatever */
		if (!(NODE (c)->shares_file) || !(NODE (c)->shares_path))
			return;

		fclose (NODE (c)->shares_file);
		NODE (c)->shares_file = NULL;

		/* create the parent structure */
		h_share = ft_host_share_add (NODE (c)->verified, NODE (c)->ip,
		                             NODE (c)->port, NODE (c)->http_port);

		if (packet->command & FT_PACKET_COMPRESSED)
		{
#ifdef USE_ZLIB
			char *new_path;

			/* uncompress the file supplied */
			new_path = share_comp_read (NODE (c)->shares_path);
			unlink (NODE (c)->shares_path);
			free (NODE (c)->shares_path);
			NODE (c)->shares_path = new_path;
#else /* !USE_ZLIB */
			/* TODO -- handle this gracefully */
			TRACE (("damnit, this shouldnt happen"));
			return;
#endif /* USE_ZLIB */
		}

		/* load from the db into ram */
		ft_share_import (h_share, NODE (c)->shares_path);
		unlink (NODE (c)->shares_path);

		/* kill the shares path */
		free (NODE (c)->shares_path);
		NODE (c)->shares_path = NULL;

		/* submit this digest to the INDEX nodes */
		conn_foreach ((ConnForeachFunc) submit_share_digest, c,
		              NODE_INDEX, NODE_CONNECTED, 0);
		return;
	}

	if (!(NODE (c)->shares_file))
	{
		char *path;
		int   comp;

		if (NODE (c)->shares_path)
			free (NODE (c)->shares_path);

		comp = packet->command & FT_PACKET_COMPRESSED;

		/* TODO -- path should be more unique than this */
		path =
		    gift_conf_path ("OpenFT/db/%s.tmp%s", net_ip_str (NODE (c)->ip),
		                    (comp) ? ".gz" : "");

		NODE (c)->shares_path = STRDUP (path);

		if (!(NODE (c)->shares_file = fopen (NODE (c)->shares_path, "wb")))
		{
			GIFT_ERROR (("Can't open %s: %s", NODE (c)->shares_path,
						GIFT_STRERROR ()));
			return;
		}
	}

	/* write the data */
	fwrite (packet->data, sizeof (char), packet->len, NODE (c)->shares_file);

#if 0
	{
		ft_uint32  size;
		char      *md5;
		char      *filename;

		size     = ft_packet_get_int (packet, 4, TRUE);
		md5      = ft_packet_get_str (packet);
		filename = ft_packet_get_str (packet);

		/* actually create and place the share */
		ft_share_add (NODE (c)->verified, NODE (c)->ip, NODE (c)->port,
		              NODE (c)->http_port, size, md5, filename);
	}
#endif
}

FT_HANDLER (share_response)
{
	TRACE_SOCK (("deprecated command"));
}

/*****************************************************************************/

static int search_request_result (Connection *c, FileShare *file,
                                  unsigned long *id)
{
	FT_Share *share;

	if (!file)
	{
		ft_packet_send (c, FT_SEARCH_RESPONSE, "%lu", P_INT (id));
		return FALSE;
	}

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	/* dont send shares they submitted
	 * NOTE: if share->host_share is NULL, the user has disconnected and
	 * share hasnt been freed because it still exists in use here */
	if (share->host_share && NODE (c)->ip != share->host_share->host)
	{
		/* use hpath when present (search_local will return results w/
		 * valid hpaths) */
		ft_packet_send (c, FT_SEARCH_RESPONSE, "%lu+I%hu%hu%lu%lu%s%s",
		                P_INT (id),
		                share->host_share->host,
		                share->host_share->port, share->host_share->http_port,
		                share->host_share->availability,
		                file->size, file->sdata->md5,
		                (file->sdata->hpath ? file->sdata->hpath : file->sdata->path));
	}

	return FALSE;
}

static int search_request_result_free (Connection *c, FileShare *file,
                                       unsigned long *id)
{
	if (!file)
		return FALSE;

	share_unref (file);

	return FALSE;
}

FT_HANDLER (search_request)
{
	List      *ptr;
	ft_uint32  id;
	ft_uint16  type;
	void      *query;
	void      *exclude;
	char      *query_str;
	char      *exclude_str;
	char      *realm;
	size_t     results = 0;
	size_t     size_min, size_max;
	size_t     kbps_min, kbps_max;

	if (!(NODE (ft_self)->class & NODE_SEARCH))
		return;

	id       = ft_packet_get_int (packet, 4, TRUE);
	type     = ft_packet_get_int (packet, 2, TRUE);

	if (type & SEARCH_HIDDEN)
	{
		query   = ft_packet_get_array (packet, 4, TRUE);
		exclude = ft_packet_get_array (packet, 4, TRUE);

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

	realm    = ft_packet_get_str (packet);
	size_min = ft_packet_get_int (packet, 4, TRUE);
	size_max = ft_packet_get_int (packet, 4, TRUE);
	kbps_min = ft_packet_get_int (packet, 4, TRUE);
	kbps_max = ft_packet_get_int (packet, 4, TRUE);

	ptr = ft_search (&results, type | SEARCH_LOCAL, query, exclude, realm,
	                 size_min, size_max, kbps_min, kbps_max);

	if (type & SEARCH_FILENAME)
	{
		TRACE_SOCK (("[%u:%hu]: '%s' (%s/%s)...%i result(s)",
		             id, type, query_str, exclude_str, realm, results));
	}

	/* ptr will be taken care of by queue_add */
	queue_add (c,
	           (QueueWriteFunc) search_request_result,
	           (QueueWriteFunc) search_request_result_free,
	           ptr, I_PTR (id));
}

FT_HANDLER (search_response)
{
	FileShare    *file = NULL;
	FT_HostShare *host_share = NULL;
	ft_uint32     id;
	ft_uint32     host;
	ft_uint16     port;
	ft_uint16     http_port;
	ft_uint32     avail;
	ft_uint32     size;
	char         *md5;
	char         *filename;

	if (!(NODE (c)->class & NODE_SEARCH))
		return;

	id = ft_packet_get_int (packet, 4, TRUE);

	/* packet->len == 4 when an EOF came in */
	if (packet->len > 4)
	{
		host      = ft_packet_get_ip  (packet);
		port      = ft_packet_get_int (packet, 2, TRUE);
		http_port = ft_packet_get_int (packet, 2, TRUE);
		avail     = ft_packet_get_int (packet, 4, TRUE);
		size      = ft_packet_get_int (packet, 4, TRUE);
		md5       = ft_packet_get_str (packet);
		filename  = ft_packet_get_str (packet);

		/* if host is 0, assume that these are local shares from that node */
		if (!host)
			host = NODE (c)->ip;

		host_share = ft_host_share_new (TRUE, host, port, http_port);
		host_share->availability = avail;

		file = ft_share_new (host_share, size, md5, filename);
	}

	/* push to the interface protocol */
	search_reply (id, c, file);

	if (file)
	{
		ft_host_share_free (host_share);
		share_unref (file);
	}
}

/*****************************************************************************/

/* check if this connection is worthless to us...ie, a user node that has
 * just finished nodelist transaction */
static void check_worthless (Connection *c)
{
#if 0
	if (NODE (ft_self)->class != NODE_USER ||
		NODE (c)->class != NODE_USER)
	{
		return;
	}

	if (NODE (c)->sent_list && NODE (c)->recv_list)
	{
		/* it's worthless, prevent an infinite recursion by causing the
		 * conditions that got us here to become false */
		NODE (c)->recv_list = FALSE;
		NODE (c)->sent_list = FALSE;

		TRACE_SOCK (("worthless node"));
		node_disconnect (c);
	}
#endif
}

static int nodelist_queue_write (Connection *c, Node *response,
                                 void *udata)
{
	/* EOF of nodelist */
	if (!response)
	{
		NODE (c)->sent_list = TRUE;
		ft_packet_send (c, FT_NODELIST_RESPONSE, NULL);

		/* WARNING: if there is data on the write queue and this node is
		 * worthless, this will end up being called again.  check worthless
		 * is intelligent and can deal with this, though */
		check_worthless (c);

		return FALSE;
	}

	/* be curteous ;) */
	if (NODE (c)->ip == response->ip)
		return FALSE;

	/* ip is in network order, so we need to swap it so that it
	 * can be swapped again in ft_packet_send ... */
	ft_packet_send (c, FT_NODELIST_RESPONSE, "+I%hu%hu",
	                response->ip, response->port, response->class);

	return FALSE;
}

static int nodelist_queue_destroy (Connection *c, Node *response,
                                   void *udata)
{
	free (response);

	return FALSE;
}

static Connection *nodelist_output (Connection *c, Node *node,
                                    List **data)
{
	Node *copy;

	/* copy the data...just to be safe */
	copy = malloc (sizeof (Node));
	memset (copy, 0, sizeof (Node));

	copy->ip    = node->ip;
	copy->class = node->class & ~(NODE_CHILD | NODE_PARENT);
	copy->port  = node->port;

	/* add this new data to the write queue */
	*data = list_append (*data, copy);

	return NULL;
}

FT_HANDLER (nodelist_request)
{
	List *out_list = NULL;

	conn_foreach ((ConnForeachFunc) nodelist_output, &out_list,
	              NODE_USER, NODE_CONNECTED, 0);

	queue_add (c,
	           (QueueWriteFunc) nodelist_queue_write,
	           (QueueWriteFunc) nodelist_queue_destroy,
	           out_list, NULL);
}

FT_HANDLER (nodelist_response)
{
	ft_uint32 ip;
	ft_uint16 port;
	ft_uint16 klass;

	/* nodelist EOF */
	if (packet->len == 0)
	{
		NODE (c)->recv_list = TRUE;
		check_worthless (c);
		return;
	}

	ip    = ft_packet_get_ip  (packet);
	port  = ft_packet_get_int (packet, 2, TRUE);
	klass = ft_packet_get_int (packet, 2, TRUE);

	node_register (ip, port, -1, klass, TRUE);
}

/*****************************************************************************/

FT_HANDLER (push_request)
{
	ft_uint32  ip;
	ft_uint16  port;
	char      *file;
	ft_uint32  start;
	ft_uint32  stop;

	ip    = ft_packet_get_ip  (packet);
	port  = ft_packet_get_int (packet, 2, TRUE);
	file  = ft_packet_get_str (packet);
	start = ft_packet_get_int (packet, 4, TRUE);
	stop  = ft_packet_get_int (packet, 4, TRUE);

	/* ip || port == 0 in this instance means that we are to send this file
	 * back to the search node contacting us here */
	if (!ip || !port)
	{
		ip   = NODE (c)->ip;
		port = NODE (c)->http_port;
	}

	if (!file || !stop)
	{
		TRACE_SOCK (("incomplete request"));
		return;
	}

	TRACE_SOCK (("received push request for %s:%hu, %s (%u - %u)",
	             net_ip_str (ip), port, file, start, stop));

	http_client_push (ip, port, file, start, stop);
}

/* NOTE: the usage of push_response is temporary in order to at least
 * fake backwards compatibility */
FT_HANDLER (push_response)
{
	ft_uint32 ip;
	ft_uint16 port;
	char     *request;
	ft_uint32 start;
	ft_uint32 stop;

	ip      = ft_packet_get_ip  (packet);
	port    = ft_packet_get_int (packet, 2, TRUE);   /* unused */
	request = ft_packet_get_str (packet);
	start   = ft_packet_get_int (packet, 4, TRUE);
	stop    = ft_packet_get_int (packet, 4, TRUE);

	if (!request)
	{
		TRACE_SOCK (("incompleted request"));
		return;
	}

#if 0
	TRACE_SOCK (("relaying push request to %s:%hu for %s",
				 net_ip_str (ip), port, request));
#endif

	/* pass this data along to the user who will be actually sending the
	 * file */
	ft_packet_send_indirect (ip, FT_PUSH_REQUEST, "+I%hu%s%lu%lu",
	                         NODE (c)->ip, NODE (c)->http_port,
	                         request, start, stop);
}

/*****************************************************************************/

FT_HANDLER (nodeinfo_request)
{
	/* this function is for padding...currently only port is actually
	 * transmitted, IP is 0 */
	ft_packet_send (c, FT_NODEINFO_RESPONSE, "+I%hu%hu", 0,
	                NODE (ft_self)->port, NODE (ft_self)->http_port);
}

FT_HANDLER (nodeinfo_response)
{
	unsigned long  ip;
	unsigned short port;
	unsigned short http_port;

	ip        = ft_packet_get_ip  (packet);
	port      = ft_packet_get_int (packet, 2, TRUE);
	http_port = ft_packet_get_int (packet, 2, TRUE);

	node_conn_set (c, 0, port, http_port);

	/* we need to verify that this node actually accepts connections on the
	 * configured ports (damn liars!) */
	if (NODE (c)->incoming)
	{
		/* they may have just changed ports, not set...so if this node had been
		 * previously verified, we need to do it again */
		NODE (c)->verified = FALSE;
		ft_accept_test (c);
	}
}

/*****************************************************************************/

/* multipurpose function.  may either submit a users digest to an INDEX node
 * or retrieve the total shares an INDEX node is aware of */
FT_HANDLER (stats_request)
{
	ft_uint16 request;

	/* this command is for INDEX nodes only! */
	if (!(NODE (ft_self)->class & NODE_INDEX))
		return;

	request = ft_packet_get_int (packet, 2, TRUE);

	switch (request)
	{
	 case 1: /* RETRIEVE INFO */
		{
			unsigned long users  = 0;
			unsigned long shares = 0;
			double        size   = 0.0; /* GB */

			ft_share_stats_get (&users, &shares, &size);

			ft_packet_send (c, FT_STATS_RESPONSE, "%lu%lu%lu",
							users, shares, (unsigned long) size);
		}
		break;
	 case 2: /* SUBMIT USER SHARES REPORT */
		{
			ft_uint32     user;   /* user who owns these files */
			ft_uint32     shares;
			ft_uint32     size;   /* MB */

			user   = ft_packet_get_ip  (packet);
			shares = ft_packet_get_int (packet, 4, TRUE);
			size   = ft_packet_get_int (packet, 4, TRUE);

			/* handle processing of this data */
			ft_share_stats_add (NODE (c)->ip, user, shares, size);
		}
		break;
	 case 3: /* REMOVE USER SHARES REPORT */
		{
			ft_uint32 user;

			user = ft_packet_get_ip (packet);

			ft_share_stats_remove (NODE (c)->ip, user);
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
	if (!(NODE (c)->class & NODE_INDEX))
		return;

	users  = ft_packet_get_int (packet, 4, TRUE);
	shares = ft_packet_get_int (packet, 4, TRUE);
	size   = ft_packet_get_int (packet, 4, TRUE);

	/* set this index nodes share stats */
	NODE (c)->stats.users  = users;
	NODE (c)->stats.shares = shares;
	NODE (c)->stats.size   = size;
}

/*****************************************************************************/

FT_HANDLER (ping_request)
{
	ft_packet_send (c, FT_PING_RESPONSE, NULL);
}

FT_HANDLER (ping_response)
{
	if (!(NODE (c)->heartbeat))
	{
		GIFT_WARN (("heartbeat is already 0"));
		return;
	}

	NODE (c)->heartbeat--;
}

/*****************************************************************************/

FT_HANDLER (modshare_request)
{
	ft_uint16 request;

	if (!(NODE (c)->class & NODE_CHILD))
		return;

	/* flush all files this node has submitted
	 * NOTE: this keeps them authorized as a child assuming they will soon
	 * resync back up */
	if (packet->len == 0)
	{
		ft_share_remove_by_host (NODE (c)->ip, FALSE);
		return;
	}

	request = ft_packet_get_int (packet, 2, TRUE);

	switch (request)
	{
	 case 1: /* HIDE SHARES */
		ft_share_disable (NODE (c)->ip);
		break;
	 case 2: /* SHOW SHARES */
		ft_share_enable (NODE (c)->ip);
		break;
	 case 3: /* SUBMIT MAX UPLOADS */
		{
			ft_uint16 uploads;
			ft_uint16 max_uploads;

			uploads     = ft_packet_get_int (packet, 2, TRUE);
			max_uploads = ft_packet_get_int (packet, 2, TRUE);

#if 0
			TRACE (("received upload data: %hu / %i",
			        uploads, (signed int) max_uploads));
#endif

			/* this is horribly inefficient and just plain stupid to do
			 * it like this */
			ft_share_set_uploads (NODE (c)->ip, uploads);
			ft_share_set_limit   (NODE (c)->ip, (signed short) max_uploads);
		}
	 case 4: /* SUBMIT UPLOAD REPORT */
		{
			ft_uint16 uploads;

			uploads = ft_packet_get_int (packet, 2, TRUE);

			ft_share_set_uploads (NODE (c)->ip, uploads);
		}
	 default:
		break;
	}
}

FT_HANDLER (modshare_response)
{
	if (!(NODE (c)->class & NODE_PARENT))
		return;
}

/*****************************************************************************/

static int construct_nodecap (unsigned long key, char *cap, FTPacket **packet)
{
	ft_packet_append (packet, "%hu%s", 1, cap);

	return TRUE;
}

FT_HANDLER (nodecap_request)
{
	FTPacket *p_out;

	p_out = ft_packet_new (FT_NODECAP_RESPONSE, NULL, 0);

	hash_table_foreach (NODE (ft_self)->cap, (HashFunc) construct_nodecap,
	                    &p_out);

	ft_packet_append (&p_out, "%hu", 0);

	ft_packet_send_constructed (c, p_out);
	ft_packet_free (p_out);
}

FT_HANDLER (nodecap_response)
{
	ft_uint16 key_id;
	char     *key;

	if (!(NODE (c)->cap))
		NODE (c)->cap = dataset_new ();

	while ((key_id = ft_packet_get_int (packet, 2, TRUE)))
	{
		if (!(key = ft_packet_get_str (packet)))
			continue;

		dataset_insert (NODE (c)->cap, key, STRDUP (key));
	}
}
