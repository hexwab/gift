/*
 * $Id: push.c,v 1.3 2004/03/24 06:36:12 hipnod Exp $
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
#include "msg_handler.h"

#include "gt_accept.h"
#include "gt_share_file.h"
#include "gt_share.h"

/*****************************************************************************/

typedef struct giv_connect
{
	uint32_t   index;
	char      *filename;
} giv_connect_t;

/*****************************************************************************/

static giv_connect_t *giv_connect_alloc (uint32_t index, const char *filename)
{
	giv_connect_t *giv;

	if (!(giv = malloc (sizeof(giv_connect_t))))
		return NULL;

	if (filename)
		giv->filename = STRDUP (filename);
	else
		giv->filename = NULL;

	giv->index = index;

	return giv;
}

static void giv_connect_free (giv_connect_t *giv)
{
	if (!giv)
		return;

	free (giv->filename);
	free (giv);
}

static char *giv_connect_str (giv_connect_t *giv)
{
	String *s;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	string_append  (s, "GIV ");
	string_appendf (s, "%u:", giv->index);
	string_appendf (s, "%s/", gt_guid_str (GT_SELF_GUID));

	if (giv->filename && !string_isempty (giv->filename))
		string_append (s, giv->filename);

	string_append (s, "\n\n");

	return string_free_keep (s);
}

/*****************************************************************************/

static void handle_giv_connect (int fd, input_id id, TCPC *c,
                                giv_connect_t *giv)
{
	char *str;
	int   ret;

	if (MSG_DEBUG)
		GT->DBGFN (GT, "entered");

	if (net_sock_error (fd))
	{
		if (MSG_DEBUG)
			GT->DBGFN (GT, "error connecting back: %s", GIFT_NETERROR ());

		tcp_close (c);
		return;
	}

	/* restore the index */
	c->udata = NULL;
	str = giv_connect_str (giv);

	if (MSG_DEBUG)
		GT->DBGSOCK (GT, c, "sending GIV response: %s", str);

	ret = tcp_send (c, str, strlen (str));
	free (str);

	if (ret <= 0)
	{
		if (MSG_DEBUG)
			GT->DBGFN (GT, "error sending: %s", GIFT_NETERROR ());

		tcp_close (c);
		return;
	}

	/* use this connection for something */
	input_remove (id);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)gt_handshake_dispatch_incoming, TIMEOUT_DEF);
}

static void giv_connect (int fd, input_id id, TCPC *c)
{
	giv_connect_t *giv;

	giv = c->udata;
	handle_giv_connect (fd, id, c, giv);

	giv_connect_free (giv);
}

static void gt_giv_request (GtNode *src, uint32_t index, in_addr_t ip,
                            in_port_t port, uint8_t hops)
{
	giv_connect_t *giv;
	char          *filename = NULL;
	Share         *share;
	GtShare       *gt_share;
	TCPC          *c;

	if (MSG_DEBUG)
		GT->DBGFN (GT, "entered");

	/* if the pushed IP address is local, forget about it */
	if (gt_is_local_ip (ip, src->ip))
		return;

	/* special case: if the node we got the giv from is local
	 * and the giv is from them (hops=0), don't connect to the
	 * external address but the internal */
	if (hops == 0 && gt_is_local_ip (src->ip, ip))
		ip = src->ip;

	/*
	 * Look for the index in the local shared database, if it is found
	 * keep track of the filename.
	 */
	if ((share = gt_share_local_lookup_by_index (index, NULL)) != NULL &&
	    (gt_share = share_get_udata (share, GT->name)) != NULL)
	{
		filename = gt_share->filename;
	}

	if (!(giv = giv_connect_alloc (index, filename)))
		return;

	if (!(c = tcp_open (ip, port, FALSE)))
	{
		giv_connect_free (giv);
		return;
	}

	c->udata = giv;

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)giv_connect, TIMEOUT_DEF);
}

GT_MSG_HANDLER(gt_msg_push)
{
	gt_guid_t  *client_guid;
	uint32_t    index;
	uint32_t    ip;
	uint16_t    port;
	uint8_t     hops;

	if (MSG_DEBUG)
		GT->DBGFN (GT, "entered");

	client_guid = gt_packet_get_ustr   (packet, 16);
	index       = gt_packet_get_uint32 (packet);
	ip          = gt_packet_get_ip     (packet);
	port        = gt_packet_get_port   (packet);

	hops = gt_packet_hops (packet);

	if (MSG_DEBUG)
	{
		GT->DBGSOCK (GT, c, "client_guid=%s index=%d ip=%s port=%hu",
		             gt_guid_str (client_guid), index, net_ip_str (ip), port);
	}

	if (gt_guid_cmp (client_guid, GT_SELF_GUID) == 0)
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
