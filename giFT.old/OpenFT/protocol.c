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

#include "http.h"

#include "file.h"
#include "search.h"
#include "parse.h"
#include "meta.h"
#include "enum.h"

/*****************************************************************************/

/**/extern Connection *ft_self;

/*****************************************************************************/

typedef void (*ProtocolHandler) (Protocol *p, Connection *c, FTPacket *packet);
#define FT_HANDLER(func) static void ft_##func (Protocol *p, Connection *c, FTPacket *packet)

FT_HANDLER (class_request);
FT_HANDLER (class_response);

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

/*****************************************************************************/

struct _search_result_data {
	Connection *c;
	char *packet;
};

struct _handler_table
{
	ft_uint16       command;
	ProtocolHandler func;
}
protocol_handler[] =
{
	{ FT_CLASS_REQUEST,     ft_class_request },
	{ FT_CLASS_RESPONSE,    ft_class_response },

	{ FT_SHARE_REQUEST,     ft_share_request },
	{ FT_SHARE_RESPONSE,    ft_share_response },

	{ FT_SEARCH_REQUEST,    ft_search_request },
	{ FT_SEARCH_RESPONSE,   ft_search_response },

	{ FT_NODELIST_REQUEST,  ft_nodelist_request },
	{ FT_NODELIST_RESPONSE, ft_nodelist_response },

	{ FT_PUSH_REQUEST,      ft_push_request },
	{ FT_PUSH_RESPONSE,     ft_push_response },

	{ FT_NODEINFO_REQUEST,  ft_nodeinfo_request },
	{ FT_NODEINFO_RESPONSE, ft_nodeinfo_response },

	{ FT_STATS_REQUEST,     ft_stats_request },
	{ FT_STATS_RESPONSE,    ft_stats_response },

	{ FT_PING_REQUEST,      ft_ping_request },
	{ FT_PING_RESPONSE,     ft_ping_response },

	{ 0,                    NULL }
};

/*****************************************************************************/

int protocol_handle_command (Protocol *p, Connection *c, FTPacket *packet)
{
	struct _handler_table *handler;

	if (!packet)
		return FALSE;

#if 0
	{
		int i;
		printf ("%s: %hu, %hu, { ", __PRETTY_FUNCTION__,
				packet->len, packet->command);

		/* temp -- debugging */
		for (i = 0; i < packet->len; i++)
			printf ("0x%02x ", packet->data[i]);

		printf ("}\n");
	}
#endif

	/* locate the handler */
	for (handler = protocol_handler; handler->func; handler++)
	{
		if (packet->command == handler->command)
		{
			(*handler->func) (p, c, packet);
			return TRUE;
		}
	}

	GIFT_ERROR (("[%s] found no handler for %hu %hu",
				 net_ip_str (NODE (c)->ip), packet->len, packet->command));

	return FALSE;
}

/*****************************************************************************/

FT_HANDLER (class_request)
{
	ft_packet_send (c, FT_CLASS_RESPONSE, "%hu", NODE (ft_self)->class);
}

FT_HANDLER (class_response)
{
	if (packet->len < 2)
		return;

	node_class_set (c, ft_packet_get_int (packet, 2, TRUE));
}

/*****************************************************************************/

FT_HANDLER (share_request)
{
	FileShare *file;
	ft_uint32  size;
	char      *md5;
	char      *filename;

	/* flushed all files...no longer a child of ours */
	if (packet->len == 0)
	{
		openft_share_remove_by_host (NODE (c)->ip);
		node_class_remove (c, NODE_CHILD);
		return;
	}

	size     = ft_packet_get_int (packet, 4, TRUE);
	md5      = ft_packet_get_str (packet);
	filename = ft_packet_get_str (packet);

	file = openft_share_new (net_ip_str (NODE (c)->ip), NODE (c)->port,
	                         NODE (c)->http_port, size, md5, filename);

	/* new child */
	if (!(NODE (c)->class & NODE_CHILD))
	{
		/* they have been accepted as our child */
		ft_packet_send (c, FT_SHARE_RESPONSE, "%lu", 0);
	}

	node_class_add (c, NODE_CHILD);

	openft_share_add (file);
}

FT_HANDLER (share_response)
{
	if (packet->len == 0)
		node_class_remove (c, NODE_PARENT);
	else
		node_class_add (c, NODE_PARENT);
}

/*****************************************************************************/

static void search_request_result (Connection *c, FileShare *file,
								   unsigned long id)
{
	OpenFT_Share *openft;

	if (!file)
	{
		TRACE_SOCK (("finished %lu", id));
		ft_packet_send (c, FT_SEARCH_RESPONSE, "%lu", id);
		return;
	}

	openft = dataset_lookup (file->data, "OpenFT");

	/* this result is already being destroyed, simply unref it and move on */
	if (openft->destroying)
	{
		openft_share_unref (file);
		return;
	}

	/* dont send shares they submitted */
	if (NODE (c)->ip != openft->host) 
	{
		/* use hpath when present (search_local will return results w/
		 * valid hpaths) */
		int len = 0;
		struct _search_result_data sr;
		char *data = ft_packet_data (c, "%lu%lu%hu%hu%lu%s%s", id,
					     ntohl (openft->host), openft->port, openft->http_port,
					     file->size, file->md5,
					     (file->hpath ? file->hpath : file->path), &len, NULL);

		sr.c=c;

		/* add the tags one by one */
		list_foreach(file->metadata, search_request_callback, &sr);

		ft_packet_send_data (c, FT_SEARCH_RESPONSE, data, len);
	}

	openft_share_unref (file);
}

static void search_request_callback(void *data, void *udata)
{
	Metadata *md=data;
	struct _search_result_data *sr=udata;
	switch (md->type) {
	case META_INTEGER:
		TRACE(("int: %d", md->value.int_val));
		sr->packet = ft_packet_data (sr->c, "%p", md->value.int_val,
					     &sr->len, sr->packet);
		break;
	case META_STRING:
		TRACE(("string: %s", md->value.str_val));
		sr->packet = ft_packet_data (sr->c, "%s", md->value.str_val,
					     &sr->len, sr->packet);
		break;
	case META_ENUM:
		TRACE(("enum: %d", md->value.enum_val));
		sr->packet = ft_packet_data (sr->c, "%p", md->value.enum_val,
					     &sr->len, sr->packet);
		break;
	}
}

static void search_request_result_free (Connection *c, FileShare *file,
										unsigned long id)
{
	/* openft_share_free (file); */
}

FT_HANDLER (search_request)
{
	List      *ptr;
	ft_uint32  id;
	ft_uint16  type;
	char      *query;
	char      *exclude;
	char      *realm;
	size_t     results = 0;
	size_t     size_min, size_max;
	size_t     kbps_min, kbps_max;

	if (!(NODE (ft_self)->class & NODE_SEARCH))
		return;

	id       = ft_packet_get_int (packet, 4, TRUE);
	type     = ft_packet_get_int (packet, 2, TRUE);
	query    = ft_packet_get_str (packet);
	exclude  = ft_packet_get_str (packet);
	realm    = ft_packet_get_str (packet);
	size_min = ft_packet_get_int (packet, 4, TRUE);
	size_max = ft_packet_get_int (packet, 4, TRUE);
	kbps_min = ft_packet_get_int (packet, 4, TRUE);
	kbps_max = ft_packet_get_int (packet, 4, TRUE);

	ptr = ft_search (&results, type | SEARCH_LOCAL, query, exclude, realm,
	                 size_min, size_max, kbps_min, kbps_max);

	TRACE_SOCK (("[%lu:%i]: '%s' (%s/%s)...%i result(s)",
	             id, type, query, exclude, realm, results));

	/* ptr will be taken care of by queue_add */
	queue_add (c,
	           (QueueWriteFunc) search_request_result,
	           (QueueWriteFunc) search_request_result_free,
	           ptr, (void *) id);
}

FT_HANDLER (search_response)
{
	FileShare *file = NULL;
	ft_uint32  id;
	ft_uint32  host;
	ft_uint16  port;
	ft_uint16  http_port;
	ft_uint32  size;
	char      *md5;
	char      *filename;
	ft_uint32  tag;
	List      *metadata;

	id = ft_packet_get_int (packet, 4, TRUE);

	/* packet->len == 4 when an EOF came in */
	if (packet->len > 4)
	{
		OpenFT_Share *openft;

		host      = ft_packet_get_int (packet, 4, FALSE);
		port      = ft_packet_get_int (packet, 2, TRUE);
		http_port = ft_packet_get_int (packet, 2, TRUE);
		size      = ft_packet_get_int (packet, 4, TRUE);
		md5       = ft_packet_get_str (packet);
		filename  = ft_packet_get_str (packet);
		metadata  = NULL;

		while ((tag = ft_packet_get_packed_int(packet))) {
			TRACE(("Processing tag 0x%x",tag));
			switch (tag & META_MASK) {
			case META_INTEGER:
				add_tag_integer(&metadata, tag, ft_packet_get_packed_int(packet));
				break;
			case META_STRING:
				add_tag_string(&metadata, tag, ft_packet_get_str(packet));
				break;
			case META_ENUM:
				add_tag_enum(&metadata, tag, ft_packet_get_packed_int(packet));
				break;
			}
			/* XXX this trace has a memory leak */
			TRACE(("tag: %s=%s", get_tag_name(((Metadata *)(list_last(metadata)->data))->type), tag_to_string(list_last(metadata)->data)));
		}

#if 0
		add_tag_enum(&metadata, TAG_REALM, 3);
		/* XXX this trace has a memory leak */
		TRACE(("tag: %s=%s", get_tag_name(((Metadata *)(list_last(metadata)->data))->type), tag_to_string(list_last(metadata)->data)));
#endif

		if (!host)
			host = NODE (c)->ip;

		file = openft_share_new (net_ip_str (host), port, http_port,
		                         size, md5, filename);

		openft = dataset_lookup (file->data, "OpenFT");

		/* host may have lied...if this is the case, lets fix it. */
		if (openft->host == 0)
			openft->host = NODE (c)->ip;
	}

#if 0
	if (!file)
		TRACE_SOCK (("finished reply"));
#endif

	search_reply (id, c, file);

	if (file)
		openft_share_unref (file);
}

/*****************************************************************************/

static void nodelist_queue_write (Connection *c, Node *response,
                                  void *udata)
{
	/* EOF of nodelist */
	if (!response)
	{
		NODE (c)->sent_list = TRUE;
		ft_packet_send (c, FT_NODELIST_RESPONSE, NULL);
		return;
	}

	/* be curteous ;) */
	if (NODE (c)->ip == response->ip)
		return;

	/* ip is in network order, so we need to swap it so that it
	 * can be swapped again in ft_packet_send ... */
	ft_packet_send (c, FT_NODELIST_RESPONSE, "%lu%hu%hu",
	              ntohl (response->ip), response->port, response->class);
}

static void nodelist_queue_destroy (Connection *c, Node *response,
									void *udata)
{
	free (response);
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
		return;
	}

	/* ip should remain in network order */
	ip    = ft_packet_get_int (packet, 4, FALSE);
	port  = ft_packet_get_int (packet, 2, TRUE);
	klass = ft_packet_get_int (packet, 2, TRUE);

	node_register (ip, port, -1, klass, TRUE);
}

/*****************************************************************************/

FT_HANDLER (push_request)
{
	Transfer         *transfer;
	Chunk            *chunk;
	OpenFT_Transfer  *xfer;
	FileShare        *file_share;
	unsigned long     ip;
	unsigned short    port;
	char             *file;

	ip   = ft_packet_get_int (packet, 4, FALSE);
	port = ft_packet_get_int (packet, 2, TRUE);
	file = ft_packet_get_str (packet);

	/* port == 0 in this instance means that we are to send this file back to
	 * the search node contacting us here */
	if (!ip || !port)
	{
		ip   = inet_addr (net_ip_str (NODE (c)->ip));
		port = NODE (c)->http_port;
	}

	TRACE_SOCK (("received push request for %s:%hu, %s",
	             net_ip_str (ip), port, file));

	/* if file_share is NULL, it will be caught and handled more appropriately
	 * in a later function... */
	file_share = openft_share_local_find (file);

	/* NOTE: range request is NOT handled here.  Once we establish the
	 * remote connection, the user will reiterate the range it wishes to
	 * receive */
	xfer       = http_transfer_new (ft_upload, ip, port, file, 0, 0);
	xfer->hash = (file_share ? STRDUP (file_share->md5) : NULL);

	transfer = upload_new (p, net_ip_str (ip), xfer->hash,
	                       file_basename (file), file, 0, 0);

	chunk       = transfer->chunks->data;
	chunk->data = xfer;

	http_push_file (chunk);
}

FT_HANDLER (push_response)
{
	TRACE_SOCK (("undefined function"));
}

/*****************************************************************************/

FT_HANDLER (nodeinfo_request)
{
	/* this function is for padding...currently only port is actually
	 * transmitted, IP is 0 */
	ft_packet_send (c, FT_NODEINFO_RESPONSE, "%lu%hu%hu", 0,
	              NODE (ft_self)->port, NODE (ft_self)->http_port);
}

FT_HANDLER (nodeinfo_response)
{
	unsigned long ip;
	unsigned short port;
	unsigned short http_port;

	ip        = ft_packet_get_int (packet, 4, FALSE);
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

FT_HANDLER (stats_request)
{
	unsigned long users  = 1;
	unsigned long shares = 0;
	unsigned long megs   = 0;
	double        megs_local;

	/* local stats */
	share_index (&shares, &megs_local);
	megs += megs_local;

	/* children stats */
	users  += NODE (ft_self)->users;
	shares += NODE (ft_self)->shares;
	megs   += NODE (ft_self)->megs;

	ft_packet_send (c, FT_STATS_RESPONSE, "%lu%lu%lu",
	                users, shares, megs);
}

FT_HANDLER (stats_response)
{
	unsigned long users;
	unsigned long shares;
	unsigned long megs;

	users  = ft_packet_get_int (packet, 4, TRUE);
	shares = ft_packet_get_int (packet, 4, TRUE);
	megs   = ft_packet_get_int (packet, 4, TRUE);

	openft_share_stats_set (c, users, shares, megs);
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
