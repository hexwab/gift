/*
 * $Id: gt_protocol.c,v 1.58 2003/06/09 20:24:25 hipnod Exp $
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

#include "gt_gnutella.h"

#include "src/meta.h"
#include "sha1.h"

#include "gt_node.h"
#include "gt_netorg.h"

#include "gt_connect.h"

#include "gt_packet.h"
#include "gt_protocol.h"

#include "gt_search.h"
#include "gt_search_exec.h"

#include "gt_query_route.h"

#include "gt_share.h"
#include "gt_share_file.h"

#include "gt_utils.h"
#include "gt_xfer.h"

#include "gt_accept.h"

/*****************************************************************************/

typedef void (*ProtocolHandler) (TCPC *c, GtPacket *packet);

#define GTLA_HANDLER(func) \
	static void gt_##func (TCPC *c, GtPacket *packet)

GTLA_HANDLER (ping_request);
GTLA_HANDLER (ping_response);

GTLA_HANDLER (bye_request);
GTLA_HANDLER (push_request);
GTLA_HANDLER (query_route);
GTLA_HANDLER (query_request);
GTLA_HANDLER (query_response);

/*****************************************************************************/

static struct _handler_table
{
	uint8_t command;
	ProtocolHandler func;
}
protocol_handler[] =
{
	{ GT_PING_REQUEST,     gt_ping_request    },
	{ GT_PING_RESPONSE,    gt_ping_response   },
	{ GT_BYE_REQUEST,      gt_bye_request     },
	{ GT_QUERY_ROUTE,      gt_query_route     },
	{ GT_PUSH_REQUEST,     gt_push_request    },
	{ GT_QUERY_REQUEST,    gt_query_request   },
	{ GT_QUERY_RESPONSE,   gt_query_response  },
	{ 0x00,                NULL               }
};

struct _search_reply
{
	uint8_t     ttl;
	uint8_t     results;  /* number of results to on the current packet */
	GtPacket   *packet;   /* the current packet to stack results on */
	gt_guid_t  *guid;
	int         dontfree; /* workaround queue_add() calling the free func */
};

/*****************************************************************************/

static int protocol_handle_command (TCPC *c, GtPacket *packet)
{
	struct _handler_table *handler;
	unsigned char command;

	if (!packet)
		return FALSE;

	command = gt_packet_command (packet);

#if 0
	{
		int i;
		printf ("%s: len:%hu, cmd:%hx, { ", __PRETTY_FUNCTION__,
				packet->len, packet->command);

		for (i = 0; i < packet->len; i++)
		{
			if (isalpha (packet->data[i]) || isdigit (packet->data[i]))
				printf ("%c", packet->data[i]);
			else
				printf ("%02x", packet->data[i]);

			if (i + 1 < packet->len)
			{
				if (!isdigit (packet->data[i+1]) && !isalpha (packet->data[i+1]))
					printf(" ");
			}
			else
				printf(" ");
		}

		printf ("}\n");
	}
#endif

	/* locate the handler */
	for (handler = protocol_handler; handler->func; handler++)
	{
		if (command == handler->command)
		{
			handler->func (c, packet);
			return TRUE;
		}
	}

	GIFT_ERROR (("[%s] found no handler for cmd %hx, payload %hx",
				 net_ip_str (GT_NODE(c)->ip), command,
				 gt_packet_payload_len (packet)));

	return FALSE;
}

/* main node communication loop
 *
 * handle reading of packets from a node connection (at this point
 * indiscriminant as to whether it is a search or user node) */
void gnutella_connection (int fd, input_id id, TCPC *c)
{
	FDBuf         *buf;
	uint32_t       len;
	unsigned char *data;
	int            n;

	/* grab a data buffer */
	buf = tcp_readbuf (c);

	/* ok this nasty little hack here is executed so that we can maintain
	 * a single command buffer for len/command and the linear data
	 * stream to follow */
	len = buf->flag + GNUTELLA_HDR_LEN;

	if ((n = fdbuf_fill (buf, len)) < 0)
	{
		GT->DBGFN (GT, "error reading, shutting down fd %d (error %s)", c->fd,
		           GIFT_NETERROR());
		gt_node_disconnect (c);
		return;
	}

	/* we successfully read a portion of the current packet, but not all of
	 * it ... come back when we have more data */
	if (n > 0)
		return;

	data = fdbuf_data (buf, NULL);
	assert (data != NULL);

	/* packet length is guaranteed > 23 here */
	len = get_payload_len (data);

	if (len > GT_PACKET_MAX)
	{
		GIFT_ERROR (("packet too large: %u; %u max", len, GT_PACKET_MAX));
		gt_node_disconnect (c);
		return;
	}

	if (buf->flag || len == 0) /* we read a complete packet */
	{
		GtPacket  *packet;
		int        ret;

		/* reset */
		buf->flag = 0;
		fdbuf_release (buf);

		if (!(packet = gt_packet_unserialize (data, len + GNUTELLA_HDR_LEN)))
			return;

		gt_packet_log (packet, c, FALSE);

		/*
		 * TODO: sanitize hops/ttl values  before
		 *       passing on to the rest of the code
		 */
		ret = protocol_handle_command (c, packet);

		gt_packet_free (packet);

		/* TODO - should we do this? */
		if (!ret)
		{
			GT->dbgsock (GT, c, "invalid packet");
			return;
		}

	}
	else if (buf->flag == 0)
	{
		/* we read a header block with non-zero length
		 * waiting on the socket */
		buf->flag = len;
	}
}

/******************************************************************************/
/* MESSAGE HANDLING */

/* reply to a ping packet */
static void ping_reply_self (GtPacket *packet, TCPC *c)
{
	unsigned long  files, size_kb;
	double         size_mb;
	GtPacket      *reply;

	share_index (&files, &size_mb);
	size_kb = size_mb * 1024;

	if (!(reply = gt_packet_reply (packet, GT_PING_RESPONSE)))
		return;

	gt_packet_put_port   (reply, GT_SELF->gt_port);
	gt_packet_put_ip     (reply, GT_NODE(c)->my_ip);
	gt_packet_put_uint32 (reply, (uint32_t)files);
	gt_packet_put_uint32 (reply, (uint32_t)size_kb);

	if (gt_packet_error (reply))
	{
		gt_packet_free (reply);
		return;
	}

	gt_packet_send (c, reply);
}

/* send info about node dst to node c */
static TCPC *send_status (TCPC *c, GtNode *node, void **data)
{
	GtPacket   *pkt = (GtPacket *)   data[0];
	TCPC       *dst = (TCPC *)       data[1];
	GtPacket   *reply;

	/* don't send a ping for the node itself */
	if (c == dst)
		return NULL;

	if (!(reply = gt_packet_reply (pkt, GT_PING_RESPONSE)))
		return NULL;

	gt_packet_put_port   (reply, node->gt_port);
	gt_packet_put_ip     (reply, node->ip);
	gt_packet_put_uint32 (reply, node->files);
	gt_packet_put_uint32 (reply, node->size_kb);

	/* set the number of hops travelled to 1 */
	gt_packet_set_hops (reply, 1);

	if (gt_packet_error (reply))
	{
		gt_packet_free (reply);
		return NULL;
	}

	gt_packet_send (dst, reply);
	return NULL;
}

static void handle_crawler_ping (GtPacket *packet, TCPC *c)
{
	void *data[2];

	data[0] = packet;
	data[1] = c;

	/* reply ourselves */
	ping_reply_self (packet, c);

	/* send pings from connected hosts */
	gt_conn_foreach ((ConnForeachFunc) send_status, data,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);
}

static int need_connections (void)
{
	BOOL am_ultrapeer;

	am_ultrapeer = GT_SELF->klass & GT_NODE_ULTRA;

	/* send a pong if we need connections, but do this
	 * only if this is a search node: leaves shouldnt send pongs */
	if (gt_conn_need_connections (GT_NODE_ULTRA) > 0 && am_ultrapeer)
		return TRUE;

	/* pretend we need connections temporarily even if we don't in order to
	 * figure out whether we are firewalled or not */
	if (gt_uptime () < 10 * EMINUTES && GT_SELF->firewalled)
		return TRUE;

	return FALSE;
}

GTLA_HANDLER (ping_request)
{
	time_t   last_ping_time, now;
	uint8_t ttl, hops;

	now = time (NULL);

	ttl  = gt_packet_ttl (packet);
	hops = gt_packet_hops (packet);

	last_ping_time = GT_NODE(c)->last_ping_time;
	GT_NODE(c)->last_ping_time = now;

	/* this tests if this host is up, so reply */
	if ((ttl == 1 && (hops == 0 || hops == 1)) || need_connections ())
	{
		ping_reply_self (packet, c);

		if (ttl == 1)
			return;
	}
	else if (ttl == 2 && hops == 0)
	{
		/* crawler ping: respond with all connected nodes */
		handle_crawler_ping (packet, c);
		return;
	}

	/* dont re-broadcast pings from search nodes if we are not one */
	if ((GT_NODE(c)->klass & GT_NODE_ULTRA) && !(gt_self->klass & GT_NODE_ULTRA))
	   return;

#if 0
	/* notify this host when the pong cache gets full */
	pong_cache_waiter_add (c, packet);
#endif

	/* dont accept pings too often */
	if (now - last_ping_time < 30 * ESECONDS)
		return;

#if 0
	if (!pong_cache_reply (c, packet))
	{
		/* refill the pong cache */
		pong_cache_refill ();
		return;
	}

	pong_cache_waiter_remove (c);
#endif
}

static int is_power_of_two (uint32_t num)
{
	uint32_t i;

	for (i = 0x80000000; i != 0; i >>= 1)
		if (num == i)
		    return TRUE;

	return FALSE;
}

GTLA_HANDLER (ping_response)
{
	uint16_t port;
	uint32_t ip;
	uint32_t files;
	uint32_t size_kb;

	port    = gt_packet_get_port   (packet);
	ip      = gt_packet_get_ip     (packet);
	files   = gt_packet_get_uint32 (packet);
	size_kb = gt_packet_get_uint32 (packet);

	/* update stats and port */
	if (gt_packet_ttl (packet) == 1 && gt_packet_hops (packet) == 0)
	{
		/* check if this is the first ping response on this connection */
		if (GT_NODE(c)->state == GT_NODE_CONNECTING_2)
		{
			/* mark this node as now connected */
			gt_node_state_set (GT_NODE(c), GT_NODE_CONNECTED);

			/* reset (should be submit) the routing table */
			if ((GT_NODE(c)->klass & GT_NODE_ULTRA) &&
				!(gt_self->klass & GT_NODE_ULTRA))
			{
				query_route_table_submit (c);
			}

			/* submit unfinished searches soon */
			gt_searches_submit (c, 15 * SECONDS);
		}

		if (ip == GT_NODE(c)->ip)
		{
			if (GT_NODE(c)->gt_port != port || !GT_NODE(c)->verified)
			{
				/* update the port */
				GT_NODE(c)->gt_port = port;

				/* verify the port like OpenFT does */
				gt_connect_test (GT_NODE(c), GT_NODE(c)->gt_port);
			}

			/* update stats information */
			GT_NODE(c)->size_kb = size_kb;
			GT_NODE(c)->files   = files;
		}

		return;
	}

	if (!gt_nodes_full (NULL))
	{
		GtNodeClass  klass = GT_NODE_NONE;
		GtNode      *n;

		/* LimeWire marks ultrapeer pongs by making files a power of two */
		if (is_power_of_two (files))
			klass = GT_NODE_ULTRA;

		/* don't change the state of nodes we are connected to, because
		 * our information is authoritative
		 *
		 * NOTE: there is a race here with the GT_NODE_CONNECTED_1 state,
		 *       where they could change the node's state... */
		if ((n = gt_node_lookup (ip, port)) && n->state & GT_NODE_CONNECTED)
			return;

		/* don't register this node if its local and the peer isnt */
		if (gt_is_local_ip (ip, GT_NODE(c)->ip))
			return;

		gt_node_register (ip, port, klass, size_kb, files);
	}

#if 0
	pong_cache_add (c, packet);
#endif
}

/* sent upon connection-close by some nodes */
GTLA_HANDLER (bye_request)
{
	uint16_t  code;
	char     *reason;

	code   = gt_packet_get_uint16 (packet);
	reason = gt_packet_get_str    (packet);

	/* log the message and code and be done with it */
	GIFT_INFO (("%s:%hu sent bye packet: code %hu, reason: %s",
	            net_ip_str (GT_NODE(c)->ip), GT_NODE(c)->gt_port, code, reason));

	/* we incur the TIME_WAIT penalty instead of the remote node if we
	 * close before they do */
	gt_node_disconnect (c);
}

/* create a table for routing queries from a child node
 * disabled for now because ultrapeer mode doesnt work yet */
GTLA_HANDLER (query_route)
{
#if 0
	uint8_t   type;
	uint32_t len;
	uint8_t   largest_val;
	uint8_t   seq_no;
	uint8_t   seq_size;
	uint8_t   compressed;
	uint8_t   bits;
	size_t    size;

	GT->DBGFN (GT, "entered");

	type = gt_packet_get_uint8 (packet);

	/* TODO: rate-limit clients calling query_route; timeouts */

	switch (type)
	{
	 case 0: /* reset table */
		len         = gt_packet_get_uint32 (packet);
		largest_val	= gt_packet_get_uint8  (packet);

		if (GT_NODE(c)->query_router)
			query_router_free (GT_NODE(c)->query_router);

		GT_NODE(c)->query_router = query_router_new (len, largest_val);

		GT->DBGFN (GT, "reset table: len = %u, largest val = %u",
		           len, largest_val);
		break;

	 case 1: /* patch table */
		seq_no     = gt_packet_get_uint8 (packet);
		seq_size   = gt_packet_get_uint8 (packet);
		compressed = gt_packet_get_uint8 (packet);
		bits       = gt_packet_get_uint8 (packet);

		GT->DBGFN (GT, "patch table: seq_no=%i seq_size=%i compressed=%i bits=%i",
		           seq_no, seq_size, compressed, bits);

		/* size of the patch is the packet length minus len of patch header */
		size = gt_packet_payload_len (packet) - 5;

		GT->DBGFN (GT, "size = %u, packet->offset = %u", size, packet->offset);
		query_router_update (GT_NODE(c)->query_router, seq_no, seq_size,
		                     compressed, bits, &packet->data[packet->offset],
		                     size);
		break;

	 default:
		GT->DBGFN (GT, "unknown query-route message type: %d", type);
		break;
	}
#endif
}

static void push_connect (int fd, input_id id, TCPC *c)
{
	uint32_t  index;
	char     *str;

	GT->DBGFN (GT, "entered");

	if (net_sock_error (fd))
	{
		GT->DBGFN (GT, "error connecting back: %s", GIFT_NETERROR ());
		tcp_close (c);
		return;
	}

	/* restore the index */
	memcpy (&index, &c->udata, MIN (sizeof (c->udata), sizeof (index)));

	str = stringf ("GIV %u:%s/\n\n", index, gt_guid_str (gt_client_guid));
	GT->dbgsock (GT, c, "sending GIV response: %s", str);

	if (tcp_send (c, str, strlen (str)) <= 0)
	{
		GT->DBGFN (GT, "error sending: %s", GIFT_NETERROR ());
		tcp_close (c);
		return;
	}

	/* use this connection for something */
	input_remove (id);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)gnutella_determine_method, TIMEOUT_DEF);
}

static void gt_giv_request (GtNode *src, uint32_t index, in_addr_t ip,
                            in_port_t port, uint8_t hops)
{
	TCPC  *c;

	GT->DBGFN (GT, "entered");

	/* skip request if we are busy, and they could have connected anyway */
	if (upload_availability () == 0 && !gt_self->firewalled)
		return;

	/* if the pushed IP address is local, forget about it */
	if (gt_is_local_ip (ip, src->ip))
		return;

	/* special case: if the node we got the push from is local
	 * and the push is from them (hops=0), don't connect to the
	 * external address but the internal */
	if (hops == 0 && gt_is_local_ip (src->ip, ip))
		ip = src->ip;

	if (!(c = tcp_open (ip, port, FALSE)))
		return;

	/* store the index directly. this way no memory is necessary */
	memcpy (&c->udata, &index, MIN (sizeof (index), sizeof (c->udata)));

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)push_connect, TIMEOUT_DEF);
}

GTLA_HANDLER (push_request)
{
	gt_guid_t  *client_guid;
	uint32_t    index;
	uint32_t    ip;
	uint16_t    port;
	uint8_t     hops;

	GT->DBGFN (GT, "entered");

	client_guid = gt_packet_get_ustr   (packet, 16);
	index       = gt_packet_get_uint32 (packet);
	ip          = gt_packet_get_ip     (packet);
	port        = gt_packet_get_port   (packet);

	hops = gt_packet_hops (packet);

	GT->dbgsock (GT, c, "client_guid=%s index=%d ip=%s port=%hu",
	             gt_guid_str (client_guid), index, net_ip_str (ip), port);

	if (gt_guid_cmp (client_guid, gt_client_guid) == 0)
	{
		/* TODO: we should not respond if we get a lot of these */
		gt_giv_request (GT_NODE(c), index, ip, port, hops);
		return;
	}

#if 0
	if ((dst_c = push_cache_lookup (client->guid)))
		gt_route_forward_packet (dst_c, packet);
#endif
}

/* this needs to be faster+more generic */
static unsigned char *parse_sha1 (char *data)
{
	char          *urn;
	char          *sha1, *sha1_0;
	char          *hash_str;
	unsigned char *parsed;
	int            len;

	if (!(urn = strstr (data, "urn:sha1")))
		return NULL;

	string_sep (&data, "urn:");
	string_sep (&data, "sha1:");

	sha1_0 = sha1 = data;

	if (!sha1)
		return FALSE;

	while (*sha1 && isalnum (*sha1))
		sha1++;

	if ((len = sha1 - sha1_0) != 32)
	{
		GT->DBGFN (GT, "bad size on sha1: %i\n", len);
		return FALSE;
	}

	if (!(hash_str = STRDUP_N (sha1_0, len)))
		return FALSE;

	if (!(parsed = sha1_bin (hash_str)))
	{
		GT->DBGFN (GT, "error parsing sha1 str '%s'", hash_str);
		free (hash_str);
		return FALSE;
	}

	free (hash_str);
	return parsed;
}

static void parse_text_meta (char *data, Dataset **meta)
{
	int      rate, freq, min, sec;
	int      n;

	if (!data)
		return;

	n = sscanf (data, "%d kbps %d khz %d:%d", &rate, &freq, &min, &sec);

	if (n != 4)
		return;

	dataset_insertstr (meta, "bitrate",   stringf ("%li", rate * 1000));
	dataset_insertstr (meta, "frequency", stringf ("%u", freq * 1000));
	dataset_insertstr (meta, "duration",  stringf ("%i", min * 60 + sec));
}

static void parse_extended_data (char *data, unsigned char **r_hash,
                                 Dataset **r_meta)
{
	char    *sha1 = NULL;

	if (r_hash)
		*r_hash = NULL;
	if (r_meta)
		*r_meta = NULL;

	if (!data)
		return;

	string_lower (data);

	while (!string_isempty (data))
	{
		if (r_hash && (sha1 = parse_sha1 (data)))
		{
			free (*r_hash);
			*r_hash = sha1;
		}

		if (r_meta)
			parse_text_meta (data, r_meta);

		string_sep (&data, "\x1C");  /* separator character for extensions */
	}
}

static int append_result (GtPacket *packet, FileShare *file)
{
	GtShare    *share;
	Hash       *hash;

	if (!(share = share_get_udata (file, gt_proto->name)))
		return FALSE;

	/* search results
	 * format: <index#> <file size> <file name> <extra data(include hash)> */
	gt_packet_put_uint32 (packet, share->index);
	gt_packet_put_uint32 (packet, file->size);
	gt_packet_put_str    (packet, share->filename);

	/*
	 * This is the information that goes "between the nulls" in a
	 * query hit. The first null comes after the filename.
	 *
	 * This is a bit specific and icky. It should be abstracted away.
	 */
	if ((hash = share_get_hash (file, "SHA1")))
	{
		char *sha1;

		assert (hash->len == SHA1_BINSIZE);

		if ((sha1 = sha1_string (hash->data)))
		{
			char  buf[128];
			int   len;

			/* make the hash be uppercase */
			string_upper (sha1);

			len = strlen (sha1);
			assert (len == SHA1_STRLEN);

			snprintf (buf, sizeof (buf) - 1, "urn:sha1:%s", sha1);
			len += strlen ("urn:sha1:");

			gt_packet_put_ustr (packet, buf, len);
			free (sha1);
		}
	}

	/* put the second null there */
	gt_packet_put_uint8 (packet, 0);

	if (gt_packet_error (packet))
	{
		gt_packet_free (packet);
		return FALSE;
	}

	return TRUE;
}

/* add a trailer to the packets */
static void transmit_results (TCPC *c, GtPacket *packet, uint8_t hits)
{
	QueryHitDescriptor1 qhd1   = 0;
	QueryHitDescriptor2 qhd2   = 0;

	/* set the push bit as significant */
	qhd2 |= QHD2_HAS_PUSH;
	/* set the busy bit as significant */
 	qhd1 |= QHD1_HAS_BUSY;

	/*
	 * We shouldnt mark ourselves firewalled if the destination is
	 * a local ip address and ttl == 1. However, for greater TTLs,
	 * there's no knowing if we should mark it or not...
	 */
	if (gt_self->firewalled)
		qhd1 |= QHD1_PUSH_FLAG;

	if (upload_availability () == 0)
		qhd2 |= QHD2_BUSY_FLAG;

	/* add the query hit descriptor
	 * <vendor id> <length> <qhd_data1> <qhd_data2> <private_data> */
	gt_packet_put_ustr   (packet, (unsigned char *) "GIFT", 4);
	gt_packet_put_uint8  (packet, 2);
	gt_packet_put_uint8  (packet, qhd1);
	gt_packet_put_uint8  (packet, qhd2);

	/* client identifier */
	gt_packet_put_ustr (packet, gt_client_guid, 16);

	if (gt_packet_error (packet))
	{
		gt_packet_free (packet);
		return;
	}

#if 0
	GT->DBGFN (GT, "packet before twiddling result number: (will twiddle %i)", hits);
	TRACE_MEM (packet->data, packet->len);
#endif

	/* rewind the packet to the search hit count and replace the hitcount
	 * it is the first byte after the header
	 * XXX: this should use a facility of gt_packet */
	packet->data[GNUTELLA_HDR_LEN] = hits;

#if 0
	GT->DBGFN (GT, "packet after twiddling:");
	TRACE_MEM (packet->data, packet->len);
#endif

	if (LOG_RESULT_PACKETS)
		GT->dbg (GT, "transmitting %i", hits);

	/* send the reply along the path to the node that queried us */
	gt_packet_send (c, packet);
}

static int query_request_result (TCPC *c, FileShare *file,
                                 struct _search_reply *reply)
{
	GtPacket *packet;

	/* this tells controls whether query_request_result_free() frees
	 * unrefs the FileShare */
	reply->dontfree = FALSE;

	if (!file)
	{
		/* send any remaining data */
		if (reply->packet)
			transmit_results (c, reply->packet, reply->results);

		return FALSE;
	}

	packet = reply->packet;

	if (packet)
	{
		/* send the packet if the max results per packet is reached
		 * or the size of the packet is large */
		if (reply->results == 255 || gt_packet_payload_len (packet) > 2000)
		{
			transmit_results (c, packet, reply->results);

			reply->packet  = NULL;
			reply->results = 0;

			/* handle this item again */
			reply->dontfree = TRUE;
			return TRUE;
		}

		if (append_result (packet, file))
			reply->results++;

		return FALSE;
	}

	/* allocate a packet */
	if (!(packet = gt_packet_new (GT_QUERY_RESPONSE, reply->ttl, reply->guid)))
	{
		GIFT_ERROR (("mem failure?"));
		return FALSE;
	}

	/* setup the search header */
	gt_packet_put_uint8  (packet, 0);
	gt_packet_put_port   (packet, (GT_SELF->firewalled ? 0 : GT_SELF->gt_port));
	gt_packet_put_ip     (packet, GT_NODE(c)->my_ip);
	gt_packet_put_uint32 (packet, 0); /* speed (kbits) */

	if (gt_packet_error (packet))
	{
		GIFT_ERROR (("failed seting up search result packet"));
		gt_packet_free (packet);
		return FALSE;
	}

	reply->packet = packet;

	/* handle this item again */
	reply->dontfree = TRUE;
	return TRUE;
#if 0
	GtPacket *reply;

	if (!(guid = gt_guid_new (guid)))
		return;

	if (!(reply = gt_packet_reply_new (packet, GT_QUERY_RESPONSE)))
	{
		free (guid)
		return;
	}

	/* host info */
	gt_packet_put_uint8  (reply, 1);                      /* number of hits */
	gt_packet_put_port   (reply, gt_self->gt_port);
	gt_packet_put_ip     (reply, GT_NODE(c)->my_ip);
	gt_packet_put_uint32 (reply, 1000 * 1024);            /* speed (kbits) */

	/* search results */
	gt_packet_put_uint32 (reply, 1);                      /* index */
	gt_packet_put_uint32 (reply, 10);                     /* file size */
	gt_packet_put_str    (reply, "hello.mp3");            /* file name */
	gt_packet_put_str    (reply, "urn:sha1:ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");

	/* query hit descriptor */
	gt_packet_put_ustr   (reply, (unsigned char *) "GIFT", 4); /* vendor code */
	gt_packet_put_uint8  (reply, 4);                     /* public sect len */
	gt_packet_put_uint8  (reply, 0x1c);                  /* status bits */
	gt_packet_put_uint8  (reply, 0x19);                  /* more status bits */
	gt_packet_put_uint16 (reply, 0);                     /* private data */

	/* client identifier */
	gt_packet_put_ustr   (reply, guid, 16);

	/* send the reply along the path to the node that queried us */
	gt_packet_send (c, reply);
#endif
}

static int query_request_result_free (TCPC *c, FileShare *file,
                                      struct _search_reply *reply)
{
	GtShare *share;

	if (!file)
	{
		free (reply->guid);
		free (reply);
		return FALSE;
	}

	/* the queue system calls the free func even if the queue func
	 * asked for a repeat, so we need to prevent freeing more than once.. */
	if (reply->dontfree)
		return FALSE;

	/* just a sanity check */
	if (file && !(share = share_get_udata (file, gt_proto->name)))
		return FALSE;

	return FALSE;
}

/* This emulates the old queue interface */
static int send_result (FileShare *file, void **args)
{
	TCPC                 *c     = args[0];
	struct _search_reply *reply = args[1];

	while (query_request_result (c, file, reply))
		;

	query_request_result_free (c, file, reply);
	return TRUE;
}

static void send_results (TCPC *c, List *results,
                          struct _search_reply *reply)
{
	void *args[] = { c, reply };

	results = list_foreach_remove (results, (ListForeachFunc)send_result, args);
	assert (results == NULL);

	query_request_result (c, NULL, reply);
	query_request_result_free (c, NULL, reply);
}

GTLA_HANDLER (query_request)
{
	char         *query;
	char         *extended;
	gt_guid_t    *guid;
	List         *ptr;
	uint8_t       ttl;
	uint8_t       hops;
	GtQueryFlags          flags;
	unsigned char        *hash;
	GtSearchType          type;
	struct _search_reply *reply;

	flags     = gt_packet_get_uint16 (packet);
	query     = gt_packet_get_str    (packet);
	extended  = gt_packet_get_str    (packet);

	/* Don't reply if the host is firewalled and we are too */
	if ((flags & QF_HAS_FLAGS) && (flags & QF_FIREWALLED) && gt_self->firewalled)
	{
		GT->dbg (GT, "not searching, flags=%02x", flags);
		return;
	}

	parse_extended_data (extended, &hash, NULL);

	if (hash)
		type = GT_SEARCH_HASH;
	else
		type = GT_SEARCH_KEYWORD;

#if 0
	GT->DBGFN (GT, "min_speed = %hu, query = '%s', extended data = '%s'",
	           min_speed, query, extended);
#endif

	ttl  = gt_packet_ttl  (packet);
	hops = gt_packet_hops (packet);

	ptr = gt_search_exec (query, type, hash, ttl, hops);
	free (hash);

	if (!ptr)
		return;

	if (!(reply = malloc (sizeof (struct _search_reply))))
	{
		list_free (ptr);
		return;
	}

	memset (reply, 0, sizeof (struct _search_reply));

	/* set the ttl of the reply to be +1 the hops the request travelled */
	reply->ttl = gt_packet_hops (packet) + 1;

	guid = gt_packet_guid (packet);

	/* use the guid of the packet in replying to results */
	reply->guid = gt_guid_dup (guid);

	send_results (c, ptr, reply);
}

static void add_meta (ds_data_t *key, ds_data_t *value, FileShare *file)
{
	char *keystr   = key->data;
	char *valuestr = value->data;

	share_set_meta (file, keystr, valuestr);
}

void gt_query_hits_parse (GtPacket *packet, GtSearch *search,
                          TCPC *c, gt_guid_t *client_guid)
{
	uint8_t      count;
	uint16_t     port;
	uint32_t     host;
	uint32_t     speed;
	FileShare   *results[255];
	int          i, availability = 1;
	int          firewalled      = FALSE;
	int          total;

	count = gt_packet_get_uint8  (packet);
	port  = gt_packet_get_port   (packet);
	host  = gt_packet_get_ip     (packet);
	speed = gt_packet_get_uint32 (packet);

	for (i = 0; i < count; i++)
	{
		uint32_t       index;
		uint32_t       size;
		char          *fname, *data;
		unsigned char *hash = NULL;
		Dataset       *meta = NULL;
		FileShare     *file;

		index = gt_packet_get_uint32 (packet);
		size  = gt_packet_get_uint32 (packet);
		fname = gt_packet_get_str    (packet);
		data  = gt_packet_get_str    (packet);

		/* If there was an error parsing the packet (not enough results),
		 * stop parsing */
		if (gt_packet_error (packet))
			break;

		if (!fname)
		{
			results[i] = NULL;
			continue;
		}

		parse_extended_data (data, &hash, &meta);

		if (!(file = gt_share_new (fname, index, size, hash)))
		{
			GIFT_ERROR (("error making fileshare, why?"));

			dataset_clear (meta);
			free (hash);

			/* make sure we find out about it if we're missing results ;) */
			assert (0);

			results[i] = NULL;
			continue;
		}

		dataset_foreach (meta, DS_FOREACH(add_meta), file);

		dataset_clear (meta);
		free (hash);

		results[i] = file;
	}

	total = i;

	/* look for the query hit descriptor */
	if (!gt_packet_error (packet) &&
	    packet->len - packet->offset >= 16 + 7 /* min qhd len */)
	{
		unsigned char *vendor;
		uint8_t        qhd_len;
		uint8_t        qhd[2];

		vendor  = gt_packet_get_ustr  (packet, 4);
		qhd_len = gt_packet_get_uint8 (packet);
		qhd[0]  = gt_packet_get_uint8 (packet);
		qhd[1]  = gt_packet_get_uint8 (packet);

		/* set availability to 0 or 1 depending on the busy flag */
		availability = (qhd[0] & QHD1_HAS_BUSY) && !(qhd[1] & QHD2_BUSY_FLAG);

		firewalled = (qhd[0] & QHD1_PUSH_FLAG) && (qhd[1] & QHD2_HAS_PUSH);

#if 0
		GT->DBGFN (GT, "vendor = %s, qhd_len = %u, qhd_0 = %x, qhd_1 = %x, "
		        "availability = %i, firewalled = %i",
		        make_str (vendor, 4), qhd_len, qhd[0], qhd[1],
		        availability, firewalled);
#endif
	}

	/* send the results to the interface protocol */
	for (i = 0; i < total; i++)
	{
		if (results[i])
		{
			gt_search_reply (search, c, host, port, client_guid, availability,
			                 firewalled, results[i]);

			gt_share_unref (results[i]);
		}
	}
}

/* should split this up into two routines */
GTLA_HANDLER (query_response)
{
	GtSearch   *search;
	int         save_offset;
	gt_guid_t  *client_guid;

	/* Each client has a unique identifier at the end of the
	 * packet.  Grab that first. */
	if (packet->len < 16)
	{
		GT->dbgsock (GT, c, "illegal query response packet, < 16 bytes");
		return;
	}

	/* hack the offset in the packet */
	save_offset = packet->offset;
	packet->offset = packet->len - 16;

	client_guid = gt_packet_get_ustr (packet, 16);

	/* put the offset back */
	packet->offset = save_offset;

	if (!(search = gt_search_find (gt_packet_guid (packet)))
		/*&& query_cache_lookup (packet->guid)*/)
	{
		/* TODO: support forwarding of query responses by
		 * looking up their destinations in the guid cache */

		/*gt_route_forward_packet (packet, c);*/

		/* add the client GUID to the push cache: in case of a
		 * push request we know where to send it */
		/*push_cache_add (client_guid, c);*/

		return;
	}

	gt_query_hits_parse (packet, search, c, client_guid);
}
