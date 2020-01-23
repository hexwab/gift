/*
 * $Id: vendor.c,v 1.6 2004/03/26 11:44:13 hipnod Exp $
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

#include "gt_connect.h"         /* gt_connect_test */
#include "gt_bind.h"

#include "gt_utils.h"           /* make_str */

#include "transfer/push_proxy.h"

/*****************************************************************************/

#define EMPTY_VENDOR { 0, 0, 0, 0 }

#define vmsg_name(name) \
	name##_struct

#define declare_vmsg(name,vendor,id) \
	const struct gt_vendor_msg vmsg_name(name) = { vendor, id }; \
	const struct gt_vendor_msg *name = &name##_struct;

/* Globally visible vendor message structs */
declare_vmsg(GT_VMSG_MESSAGES_SUPP,    EMPTY_VENDOR, 0);
declare_vmsg(GT_VMSG_HOPS_FLOW,        "BEAR",       4);
declare_vmsg(GT_VMSG_TCP_CONNECT_BACK, "BEAR",       7);
declare_vmsg(GT_VMSG_PUSH_PROXY_REQ,   "LIME",      21);
declare_vmsg(GT_VMSG_PUSH_PROXY_ACK,   "LIME",      22);

/*****************************************************************************/

static GT_MSG_HANDLER(gt_msg_messages_supported);
static GT_MSG_HANDLER(gt_msg_hops_flow);
static GT_MSG_HANDLER(gt_msg_tcp_connect_back);
static GT_MSG_HANDLER(gt_msg_push_proxy_request);
static GT_MSG_HANDLER(gt_msg_push_proxy_acknowledgement);

/*****************************************************************************/

struct gt_vendor_table
{
	const struct gt_vendor_msg  *msg;
	GtMessageHandler             func;
	uint16_t                     version;
	BOOL                         in_msgs_supported; /* inside MessagesSupported */
};

static struct gt_vendor_table vendor_table[] =
{
	{ &vmsg_name(GT_VMSG_MESSAGES_SUPP),    gt_msg_messages_supported,
	  1,                                    FALSE },
	{ &vmsg_name(GT_VMSG_HOPS_FLOW),        gt_msg_hops_flow,
	  1,                                    FALSE  /*TODO: support receving*/ },
	{ &vmsg_name(GT_VMSG_TCP_CONNECT_BACK), gt_msg_tcp_connect_back,
	  1,                                    TRUE  },
	{ &vmsg_name(GT_VMSG_PUSH_PROXY_REQ),   gt_msg_push_proxy_request,
	  2,                                    TRUE  },
	{ &vmsg_name(GT_VMSG_PUSH_PROXY_ACK),   gt_msg_push_proxy_acknowledgement,
	  2,                                    FALSE },
};

static const size_t nr_vmsgs = sizeof(vendor_table) / sizeof(vendor_table[0]);

/*****************************************************************************/

static void vmsg_init (struct gt_vendor_msg *msg,
                       unsigned char *vendor_id, uint16_t id)
{
	/* initialize to zero because of potential structure alignment issues */
	memset (msg, 0, sizeof(struct gt_vendor_msg));

	memcpy (&msg->vendor_id, vendor_id, 4);
	msg->id = id;
}

static void append_vmsg (GtPacket *pkt, const struct gt_vendor_msg *msg,
                         uint16_t ver)
{
	gt_packet_put_ustr   (pkt, msg->vendor_id, 4);
	gt_packet_put_uint16 (pkt, msg->id);
	gt_packet_put_uint16 (pkt, ver);
}

void gt_vmsg_send_supported (GtNode *node)
{
	GtPacket *pkt;
	int       i;
	uint16_t  vector_len = 0;

	if (!dataset_lookupstr (node->hdr, "vendor-message"))
		return;

	if (!(pkt = gt_packet_vendor (GT_VMSG_MESSAGES_SUPP)))
		return;

	gt_packet_put_uint16 (pkt, 0);

	for (i = 0; i < nr_vmsgs; i++)
	{
		if (vendor_table[i].in_msgs_supported)
		{
			vector_len++;
			append_vmsg (pkt, vendor_table[i].msg, vendor_table[i].version);
		}
	}

	/* XXX: packet put_xxx functions don't work with gt_packet_seek :( */
	vector_len = htovl (vector_len);
	memcpy (&pkt->data[GNUTELLA_HDR_LEN + VMSG_HDR_LEN], &vector_len, 2);

	if (gt_packet_error (pkt))
	{
		gt_packet_free (pkt);
		return;
	}

	GT->DBGSOCK (GT, GT_CONN(node), "sending MessagesSupported");

	gt_packet_send (GT_CONN(node), pkt);
	gt_packet_free (pkt);
}

/*****************************************************************************/

GT_MSG_HANDLER(gt_msg_vendor)
{
	struct gt_vendor_msg vmsg;
	unsigned char       *vendor;
	int                  i;
	uint16_t             id;
	uint16_t             version;

	if (gt_packet_hops (packet) != 0 && gt_packet_ttl (packet) != 1)
		return;

	vendor  = gt_packet_get_ustr   (packet, 4);
	id      = gt_packet_get_uint16 (packet);
	version = gt_packet_get_uint16 (packet);

	/* initialize a copy for making a single call to memcmp */
	vmsg_init (&vmsg, vendor, id);

	if (gt_packet_error (packet))
	{
		if (MSG_DEBUG)
			GT->DBGSOCK (GT, c, "Error parsing vendor message");

		return;
	}

	for (i = 0; i < nr_vmsgs; i++)
	{
		if (memcmp (vendor_table[i].msg, &vmsg, sizeof(vmsg)) == 0 &&
		    version <= vendor_table[i].version)
		{
			vendor_table[i].func (MSG_HANDLER_ARG_NAMES);
			return;
		}
	}

	if (MSG_DEBUG)
	{
		GT->DBGSOCK (GT, c, "No handler for vendor message %s/%dv%d",
		             make_str (vendor, 4), id, version);
	}
}

/*****************************************************************************/

static struct gt_vendor_table *find_in_vmsg_table (gt_vendor_msg_t *vmsg)
{
	int i;

	for (i = 0; i < nr_vmsgs; i++)
	{
		if (memcmp (vendor_table[i].msg, vmsg, sizeof(*vmsg)) == 0)
			return &vendor_table[i];
	}

	return NULL;
}

static GT_MSG_HANDLER(gt_msg_messages_supported)
{
	gt_vendor_msg_t vmsg;
	unsigned char  *vendor_id;
	int             i;
	uint16_t        id;
	uint16_t        version;
	uint16_t        vector_len;

	vector_len = gt_packet_get_uint16 (packet);

	if (gt_packet_error (packet))
		return;

	/*
	 * Track the supported messages in a dataset on this node.
	 */
	for (i = 0; i < vector_len; i++)
	{
		struct gt_vendor_table *entry;

		vendor_id  = gt_packet_get_ustr   (packet, 4);
		id         = gt_packet_get_uint16 (packet);
		version    = gt_packet_get_uint16 (packet);

		if (gt_packet_error (packet))
			break;

		vmsg_init (&vmsg, vendor_id, id);

		if (!(entry = find_in_vmsg_table (&vmsg)))
		    continue;

		/* only send the minimum supported by both ends to this node */
		version = MIN (version, entry->version);

		/* track support for this vendor message */
		dataset_insert (&node->vmsgs_supported, &vmsg, sizeof(vmsg),
		                &version, sizeof(version));
	}

	/* send our batch of vendor messages now */
	gt_bind_completed_connection (node);
}

/*****************************************************************************/

static GT_MSG_HANDLER(gt_msg_hops_flow)
{
}

/*****************************************************************************/

static GT_MSG_HANDLER(gt_msg_tcp_connect_back)
{
	in_port_t port;

	port = gt_packet_get_port (packet);

	if (!port)
		return;

	gt_connect_test (node, port);
}

/*****************************************************************************/

static GT_MSG_HANDLER(gt_msg_push_proxy_request)
{
	return;
}

static GT_MSG_HANDLER(gt_msg_push_proxy_acknowledgement)
{
	in_addr_t ip;
	in_port_t port;

	ip   = gt_packet_get_ip   (packet);
	port = gt_packet_get_port (packet);

	if (gt_packet_error (packet))
		return;

	gt_push_proxy_del (node);
	gt_push_proxy_add (node, ip, port);
}
