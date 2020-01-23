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
enum
{
	/* AUTHENTICATION AND HANDSHAKING */

	FT_VERSION_REQUEST = 0,
	FT_VERSION_RESPONSE,

	FT_CLASS_REQUEST,
	FT_CLASS_RESPONSE,

	FT_NODEINFO_REQUEST,
	FT_NODEINFO_RESPONSE,

	FT_NODELIST_REQUEST,
	FT_NODELIST_RESPONSE,

	FT_NODECAP_REQUEST,
	FT_NODECAP_RESPONSE,

	FT_PING_REQUEST,
	FT_PING_RESPONSE,

	/* SHARE NEGOTIATION AND STATS */

	FT_CHILD_REQUEST   = 100,
	FT_CHILD_RESPONSE,

	FT_SHARE_REQUEST,
	FT_SHARE_RESPONSE,

	FT_MODSHARE_REQUEST,
	FT_MODSHARE_RESPONSE,

	FT_STATS_REQUEST,
	FT_STATS_RESPONSE,

	/* SEARCHING */

	FT_SEARCH_REQUEST  = 200,
	FT_SEARCH_RESPONSE,

	/* DOWNLOADING */

	FT_PUSH_REQUEST    = 300,
	FT_PUSH_RESPONSE
};

/**************************************************************************/

int protocol_handle_command (Protocol *p, Connection *c, FTPacket *packet);

/**************************************************************************/

#endif /* __OPENFT_PROTOCOL_H__ */
