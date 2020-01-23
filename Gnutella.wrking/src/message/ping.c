/*
 * $Id: ping.c,v 1.4 2004/03/05 17:49:40 hipnod Exp $
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
#include "message/msg_handler.h"

#include "gt_node_list.h"
#include "gt_bind.h"
#include "gt_netorg.h"

#include "gt_stats.h"
#include "gt_share.h"

/******************************************************************************/

BOOL gt_is_pow2 (uint32_t num)
{
	return BOOL_EXPR (num > 0 && (num & (num-1)) == 0);
}

static uint32_t get_shared_size (unsigned long size_mb)
{
	uint32_t size_kb;

	size_kb = size_mb * 1024;

	if (GT_SELF->klass & GT_NODE_ULTRA)
		/* TODO: round up to nearest power of two >= 8 here */;
	else if (gt_is_pow2 (size_kb))
		size_kb += 5; /* unmakes all powers of two, including 1 */

	return size_kb;
}

/* reply to a ping packet */
static void ping_reply_self (GtPacket *packet, TCPC *c)
{
	unsigned long  files, size_kb;
	double         size_mb;
	GtPacket      *reply;

	share_index (&files, &size_mb);
	size_kb = get_shared_size (size_mb);

	if (!(reply = gt_packet_reply (packet, GT_MSG_PING_REPLY)))
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
	gt_packet_free (reply);
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

	if (!(reply = gt_packet_reply (pkt, GT_MSG_PING_REPLY)))
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
	gt_packet_free (reply);

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
	gt_conn_foreach (GT_CONN_FOREACH(send_status), data,
	                 GT_NODE_NONE, GT_NODE_CONNECTED, 0);
}

static BOOL need_connections (void)
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

GT_MSG_HANDLER(gt_msg_ping)
{
	time_t   last_ping_time, now;
	uint8_t  ttl, hops;

	now = time (NULL);

	ttl  = gt_packet_ttl (packet);
	hops = gt_packet_hops (packet);

	last_ping_time = GT_NODE(c)->last_ping_time;
	GT_NODE(c)->last_ping_time = now;

	if ((ttl == 1 && (hops == 0 || hops == 1))    /* tests if host is up */
	 || GT_NODE(c)->state == GT_NODE_CONNECTING_2 /* need to reply */
	 || need_connections ())                      /* we need connections */
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
	if ((GT_NODE(c)->klass & GT_NODE_ULTRA) && !(GT_SELF->klass & GT_NODE_ULTRA))
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
