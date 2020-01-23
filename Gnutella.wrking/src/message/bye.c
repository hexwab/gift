/*
 * $Id: bye.c,v 1.2 2004/01/07 07:27:18 hipnod Exp $
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

/* sent upon connection-close by some nodes */
GT_MSG_HANDLER(gt_msg_bye)
{
	uint16_t  code;
	char     *reason;

	code   = gt_packet_get_uint16 (packet);
	reason = gt_packet_get_str    (packet);

	/* log the message and code and be done with it */
	if (MSG_DEBUG)
	{
		GT->DBGFN (GT, "%s:%hu sent bye packet: code %hu, reason: %s",
		           net_ip_str (GT_NODE(c)->ip), GT_NODE(c)->gt_port,
		           code, reason);
	}

	/* we incur the TIME_WAIT penalty instead of the remote node if we
	 * close before they do */
	gt_node_disconnect (c);
}
