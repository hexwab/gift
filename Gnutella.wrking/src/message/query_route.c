/*
 * $Id: query_route.c,v 1.1 2004/01/04 03:57:53 hipnod Exp $
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

#include "gt_query_route.h"

/*****************************************************************************/

/* create a table for routing queries from a child node
 * disabled for now because ultrapeer mode doesnt work yet */
GT_MSG_HANDLER(gt_msg_query_route)
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
