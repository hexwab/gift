/*
 * $Id: ft_protocol.h,v 1.11 2003/05/05 09:49:10 jasta Exp $
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
	/*************************************************************************/
	/**
	 * @name Handshaking
	 * Authentication and handshaking.
	 */

	/**
	 * Explicit exchange of version information.  This is the first stage in
	 * handshaking, and the process will be aborted if a non-compatible
	 * change is detected, defined by a major, minor, or micro change
	 * currently.
	 */
	FT_VERSION_REQUEST = 0,
	FT_VERSION_RESPONSE,

	/**
	 * Exchange info about a single node.  The most common usage here is to
	 * request the direct peers information in case the cache is stale or
	 * another node has provided false info.
	 */
	FT_NODEINFO_REQUEST,
	FT_NODEINFO_RESPONSE,

	/**
	 * Leak a small portion of the current nodes cache to aid the remote peer
	 * in full reconnection to the network.  This command is optimized using
	 * a single OpenFT packet.
	 */
	FT_NODELIST_REQUEST,
	FT_NODELIST_RESPONSE,

	/**
	 * Exchange a list of supported OpenFT features.  Providedly mostly for
	 * function padding later, however it is currently used to negotiate
	 * stream compression through ZLib.
	 */
	FT_NODECAP_REQUEST,
	FT_NODECAP_RESPONSE,

	/**
	 * Ensures that the remote peer is still actively replying to OpenFT
	 * requests.  This is done as the main connection timer ticks to ensure
	 * that no connections have become stale as waiting for the kernel to
	 * time out may take way too long.
	 */
	FT_PING_REQUEST,
	FT_PING_RESPONSE,

	/**
	 * Final stage in the session handshaking is to actually authorize a full
	 * fledge session.  This command exists to prevent race conditions and to
	 * gracefully inform users that a full OpenFT connection can not be
	 * authorized.
	 */
	FT_SESSION_REQUEST,
	FT_SESSION_RESPONSE,

	/*************************************************************************/
	/**
	 * @name Sharing
	 * Share negotiation and statistics
	 */

	/**
	 * Negotiate child status with the remote peer.  Allows for graceful
	 * rejection by remote search nodes.
	 */
	FT_CHILD_REQUEST   = 100,
	FT_CHILD_RESPONSE,

	/**
	 * Adds a single share to be index by the remote parent node.  Please
	 * note that addition, removal, and modification of share data is only
	 * allowed by the remote node after child status has been accepted.
	 */
	FT_ADDSHARE_REQUEST,
	FT_ADDSHARE_RESPONSE,

	/**
	 * Remove a single share from the remote parent node by it's unique
	 * md5sum.
	 */
	FT_REMSHARE_REQUEST,
	FT_REMSHARE_RESPONSE,

	/**
	 * Poorly constructed command that allows a peer to modify sharing
	 * information, such as maximum number of upload slots currently
	 * available.  I sincerely hope that this is redone in the future.
	 */
	FT_MODSHARE_REQUEST,
	FT_MODSHARE_RESPONSE,

	/**
	 * Deprecated interface for querying and manipulating remote stats
	 * databases.  This was useful as a temporary solution only, and now that
	 * virtual clustering has been implemented, it will no longer function
	 * properly.  A major redesign will be required.
	 */
	FT_STATS_REQUEST,
	FT_STATS_RESPONSE,

	/*************************************************************************/
	/**
	 * @name Data Querying
	 * Searching and browsing
	 */

	/**
	 * Query the remote node as a normal search.  Please note that this
	 * command will be forwarded throughout the virtual cluster defined by
	 * the parent node.  Currently there is no way of knowing when the search
	 * has indefinitely terminated as a result of this.  Hopefully, the next
	 * major revision of OpenFT will address this limitation.
	 */
	FT_SEARCH_REQUEST  = 200,
	FT_SEARCH_RESPONSE,

	/**
	 * Browse the directly connected remote peer.  If a direct connection
	 * cannot be established, browsing will not be supported.  This is done
	 * in an effort to reduce overall bandwidth usage from forwarded replies.
	 */
	FT_BROWSE_REQUEST,
	FT_BROWSE_RESPONSE,

	/*************************************************************************/
	/**
	 * @name Transfers
	 * Transfer assistance
	 */

	/**
	 * Special interface for delivering messages to firewalled users so that
	 * they may still upload to users who have directly exposed external
	 * Internet addresses.  The actual PUSH request will be performed by the
	 * parent node with a direct connection to the firewalled user.  The PUSH
	 * FORWARD will be delivered by the downloading user to the parent, which
	 * will in turn forward the request to the appropriate node.
	 *
	 * Upon receiving the PUSH request, the firewalled node will make an
	 * outgoing connection to the original forwarding node (the one who
	 * requested the download) and advertise that the request is being
	 * fulfilled through a special HTTP header.  It's dirty, but it's the
	 * only way.
	 */
	FT_PUSH_REQUEST    = 300,
	FT_PUSH_FORWARD
} FTCommand;

/**************************************************************************/

/**
 * Handle the supplied packet.
 *
 * @return Boolean success or failure.
 */
int protocol_handle (Protocol *p, TCPC *c, FTPacket *packet);

/**************************************************************************/

#endif /* __FT_PROTOCOL_H */
