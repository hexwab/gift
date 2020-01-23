/*
 * ft_protocol.h
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

#ifndef __FT_PROTOCOL_H
#define __FT_PROTOCOL_H

/**************************************************************************/

/**
 * @file ft_protocol.h
 *
 * @brief Handles received OpenFT packet data
 */

/*
 * SAMPLE SESSION HANDSHAKING (see ft_session.c for more information):
 *
 * NON-FIREWALLED                  FIREWALLED
 *
 *
 * STAGE 1                         STAGE 1
 * -------                         -------
 *
 * -> version request
 *                                 <- version request
 * -> version response
 *
 *                                 STAGE 2 (version accepted)
 *                                 --------------------------
 *
 *                                 <- version response
 *
 * STAGE 2 (version accepted)
 * --------------------------
 *
 * -> nodeinfo request
 *                                 <- nodeinfo request
 *                                 <- nodeinfo response
 *
 * STAGE 3 (ports tested)
 * ----------------------
 *
 * -> session request
 *                                 [ ignored due to stage 2 ]
 * -> nodeinfo response
 *
 *                                 STAGE 3 (ports tested)
 *                                 ----------------------
 *
 *                                 <- session request
 *
 * STAGE 4 (session accepted)
 * --------------------------
 *
 * -> session response
 *
 *                                 STAGE 4 (session accepted)
 *                                 --------------------------
 *
 *                                 <- session response
 *
 * [ ignored due to stage 4 ]
 *
 * -> child request
 *
 * ...
 *
 */

/*****************************************************************************/

#include "ft_packet.h"

/**************************************************************************/

/**
 * Commands
 */
typedef enum
{
	/**
	 * @name Handshaking
	 * Authentication and handshaking.
	 */

	FT_VERSION_REQUEST = 0,
	FT_VERSION_RESPONSE,

	FT_NODEINFO_REQUEST,
	FT_NODEINFO_RESPONSE,

	FT_NODELIST_REQUEST,
	FT_NODELIST_RESPONSE,

	FT_NODECAP_REQUEST,
	FT_NODECAP_RESPONSE,

	FT_PING_REQUEST,
	FT_PING_RESPONSE,

	FT_SESSION_REQUEST,
	FT_SESSION_RESPONSE,

	/**
	 * @name Sharing
	 * Share negotiation and statistics
	 */

	FT_CHILD_REQUEST   = 100,
	FT_CHILD_RESPONSE,

	FT_ADDSHARE_REQUEST,
	FT_ADDSHARE_RESPONSE,

	FT_REMSHARE_REQUEST,
	FT_REMSHARE_RESPONSE,

	FT_MODSHARE_REQUEST,
	FT_MODSHARE_RESPONSE,

	FT_STATS_REQUEST,
	FT_STATS_RESPONSE,

	/**
	 * @name Data Querying
	 * Searching and browsing
	 */

	FT_SEARCH_REQUEST  = 200,
	FT_SEARCH_RESPONSE,

	FT_BROWSE_REQUEST,
	FT_BROWSE_RESPONSE,

	/**
	 * @name Transfers
	 * Transfer assistance
	 */

	FT_PUSH_REQUEST    = 300,
	FT_PUSH_RESPONSE
} FTCommand;

/**************************************************************************/

/**
 * Handle the supplied packet.
 *
 * @return Boolean success or failure.
 */
int protocol_handle (Protocol *p, Connection *c, FTPacket *packet);

/**************************************************************************/

#endif /* __FT_PROTOCOL_H */
