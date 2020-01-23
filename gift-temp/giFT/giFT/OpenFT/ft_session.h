/*
 * $Id: ft_session.h,v 1.16 2003/05/05 09:49:11 jasta Exp $
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

#ifndef __FT_SESSION_H
#define __FT_SESSION_H

/*****************************************************************************/

/**
 * @file ft_session.h
 *
 * @brief OpenFT session management.
 *
 * Handles low-level protocol details including socket data processing and
 * initial handshaking.
 */

/*****************************************************************************/

struct _ft_node;

/**
 * @brief OpenFT session data.
 *
 * Data that is associated with an FTNode connection during it's active
 * session life (post-handshaking) and not necessarily cached or desired once
 * the connection has closed.
 */
typedef struct
{
	/**
	 * @name Handshaking Data
	 */
	unsigned char stage;               /**< Current handshaking stage */
	timer_id      start_timer;         /**< When raised the handshaking
	                                    *   stage took too long to complete
	                                    *   and the remote node will be
	                                    *   dropped */
	Array        *pqueue;              /**< Packet write queue to prevent
	                                    *   our local node from writing data
	                                    *   "out of order" according to the
	                                    *   handshaking rules */

	/**
	 * @name Data Buffers
	 */
	Dataset      *streams_recv;        /**< Indexes all receive streams for
	                                    *   this particular session */
	Dataset      *streams_send;        /**< Indexes all outbound streams for
	                                    *   this particular session */
	FTStream     *submit;              /**< Temporary stream used for share
	                                    *   submission (additions) */
	FTStream     *submit_del;          /**< Separate stream for share removal
	                                    *   because FTStream's are not capable
	                                    *   of combining commands */
	Dataset      *cap;                 /**< List of supported node features
	                                    *   and their respective support
	                                    *   values.  This is used to negotiate
	                                    *   features with a remotely
	                                    *   connected node. */

	/**
	 * @name Connection Data
	 */
	TCPC         *c;                   /**< Actual socket connection to the
	                                    *   remote node */
	time_t        start;               /**< Current session start time */
	unsigned char heartbeat : 4;       /**< Used to measure ping responses
	                                    *   (over the OpenFT protocol) and
	                                    *   determine when a node should be
	                                    *   disconnected due to lag or
	                                    *   unresponsiveness */
	unsigned char incoming : 1;        /**< If TRUE, this session was
	                                    *   established by a node that
	                                    *   connected to us.  This value is
	                                    *   really only useful when
	                                    *   determining if a port verification
	                                    *   test is necessary. */
	unsigned char verified : 1;        /**< Determines whether or not the
	                                    *   port verification has already
	                                    *   occurred.  This will hopefully
	                                    *   go away soon as we can implement
	                                    *   this logic abstractly through
	                                    *   ft_node_set_port et al. */

	/**
	 * @name Classification Data
	 *
	 * Data that is specific to a particular node classification, and not
	 * utilized by all node types.  You should note that this does not
	 * necessarily indicate whether the class is based on 'self' or the node
	 * this session instance represents.
	 */
	FTStats         stats;             /**< If the remote node is a search
	                                    *   node and 'self' is an index node,
	                                    *   this structure will contain the
	                                    *   last pulled stats digest update
	                                    *   from the remote node */
	TCPC           *verify_openft;     /**< Connection made in an attempt
	                                    *   to verify the port supplied by
	                                    *   this node on handshaking */
	TCPC           *verify_http;       /**< Same as verify_openft, except that
	                                    *   the HTTP port is tested */

	/**
	 * @name Temporary
	 *
	 * Members that are meant to act as a sort of global scratch buffer for
	 * each node.
	 */
	Dataset        *nodelist;          /**< List of all ip:port combinations
	                                    *   that have been delivered to
	                                    *   the node associated with this
	                                    *   session.  Used to avoid sending
	                                    *   the same nodeinfo_response over
	                                    *   and over. */
} FTSession;

/**
 * Helper macro for accessing session info from a Connection.
 */
#define FT_SESSION(c) ((FT_NODE(c)) ? FT_NODE(c)->session : NULL)

/*****************************************************************************/

/**
 * Begin an OpenFT session.  This begins the handshaking at stage 1.
 *
 * @param c
 * @param incoming Did this session connect to us?
 *
 * @return Boolean success or failure.
 */
int ft_session_start (TCPC *c, int incoming);

/**
 * Halt an OpenFT session.  This disconnects the node and flushes all
 * temporary data.
 */
void ft_session_stop (TCPC *c);

/**
 * Calculate the current session's total uptime as of the calling time.
 */
time_t ft_session_uptime (TCPC *c);

/**
 * Increment the session stage.
 *
 * @param c
 * @param current Current stage.  This is intended to be a constant value and
 *                will be used to increment the session stage if they match.
 */
void ft_session_stage (TCPC *c, unsigned char current);
int ft_session_queue (TCPC *c, FTPacket *packet);

int ft_session_connect (struct _ft_node *node);
void ft_session_incoming (int fd, input_id id, TCPC *listen);

/*****************************************************************************/

#endif /* __FT_SESSION_H */
