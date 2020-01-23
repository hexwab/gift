/*
 * protocol.h
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

#ifndef __OPENFT_PROTOCOL_H__
#define __OPENFT_PROTOCOL_H__

/**************************************************************************/

#include "packet.h"

/**************************************************************************/

/* protocol commands */
enum {
	FT_CLASS_REQUEST = 0,
	FT_CLASS_RESPONSE,

	FT_SHARE_REQUEST,
	FT_SHARE_RESPONSE,

	FT_SEARCH_REQUEST,
	FT_SEARCH_RESPONSE,

	FT_NODELIST_REQUEST,
	FT_NODELIST_RESPONSE,

	FT_PUSH_REQUEST,
	FT_PUSH_RESPONSE,

	FT_NODEINFO_REQUEST,
	FT_NODEINFO_RESPONSE,

	FT_STATS_REQUEST,
	FT_STATS_RESPONSE,

	FT_PING_REQUEST,
	FT_PING_RESPONSE
};

/**************************************************************************/

int protocol_handle_command (Protocol *p, Connection *c, FTPacket *packet);

/**************************************************************************/

#endif /* __OPENFT_PROTOCOL_H__ */
