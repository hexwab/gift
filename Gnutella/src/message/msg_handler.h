/*
 * $Id: msg_handler.h,v 1.3 2004/01/07 07:24:43 hipnod Exp $
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

#ifndef GIFT_GT_MSG_HANDLER_H_
#define GIFT_GT_MSG_HANDLER_H_

/*****************************************************************************/

#define MSG_DEBUG             gt_config_get_int("message/debug=0")

/*****************************************************************************/

/* Erm, ugly node state encoded in a query-hit */
typedef enum gt_eqhd_type1
{
	EQHD1_EMPTY       = 0x00,  /* no flags set */
	EQHD1_PUSH_FLAG   = 0x01,  /* send a push request for this result */
	EQHD1_HAS_BAD     = 0x02,  /* bad flag in qhd[2] is signficant,always 0? */
	EQHD1_HAS_BUSY    = 0x04,  /* busy ... */
	EQHD1_HAS_STABLE  = 0x08,  /* stable ... */
	EQHD1_HAS_SPEED   = 0x10,  /* speed ... */
	EQHD1_HAS_GGEP    = 0x20,  /* query-hit has GGEP encoded block */
} gt_eqhd1_t;

typedef enum gt_eqhd_type2
{
	EQHD2_EMPTY       = 0x00,  /* no flags set */
	EQHD2_HAS_PUSH    = 0x01,  /* set if push flag is significant */
	EQHD2_BAD_FLAG    = 0x02,  /* always 0? */
	EQHD2_BUSY_FLAG   = 0x04,  /* set if for no availability */
	EQHD2_STABLE_FLAG = 0x08,  /* set if transmitted an upload */
	EQHD2_SPEED_FLAG  = 0x10,  /* if set, speed is max attained upload speed */
	EQHD2_HAS_GGEP    = 0x20,  /* packet has GGEP */
} gt_eqhd2_t;

/*****************************************************************************/

#include "gt_node.h"
#include "gt_packet.h"

#include "message/gt_message.h"

/*****************************************************************************/

/*
 * A message handler function.
 */
typedef void (*GtMessageHandler) (GtNode *node, TCPC *c, GtPacket *packet);

#define MSG_HANDLER_ARG_NAMES \
	node, c, packet

#define GT_MSG_HANDLER(func) \
	void func (GtNode *node, TCPC *c, GtPacket *packet)

/*****************************************************************************/

#endif /* GIFT_GT_MSG_HANDLER_H_ */
