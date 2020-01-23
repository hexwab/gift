/*
 * $Id: ft_session.h,v 1.28 2004/10/31 22:19:23 hexwab Exp $
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
 * handshaking stage logic.
 */

/*****************************************************************************/

struct ft_search_db;                   /* ft_search_db.h:FTSearchDB */
struct ft_node;

/*****************************************************************************/

/**
 * Defines the various connection goals to accomplish.  This is used so that
 * nodes will gracefully disconnect themselves when they have completed
 * whichever mission inspired the session in the first place.
 */
typedef enum
{
	FT_PURPOSE_DRIFTER      = 0x00,    /**< No purpose */
	FT_PURPOSE_UNDEFINED    = 0x01,    /**< Backwards compat until all
	                                    *   systems support ft_purpose_t */
	FT_PURPOSE_PARENT_TRY   = 0x02,    /**< Get a new parent search node */
	FT_PURPOSE_PARENT_KEEP  = 0x04,    /**< Hang on to the newly acquired
	                                    *   parent search node */
	FT_PURPOSE_GET_NODES    = 0x08,    /**< Request additional nodelists */
	FT_PURPOSE_DELIVERY     = 0x10,    /**< General packet delivery, as
	                                    *   with ::ft_packet_sendto */
	FT_PURPOSE_PUSH_FWD     = 0x20,    /**< Deliver a PUSH forward request */
	FT_PURPOSE_PEER_KEEP    = 0x40
} ft_purpose_t;

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
	FTStream     *autoflushed;         /**< Stream for not-easily-encapsulated
					        data; flushed periodically. */
	unsigned int  af_pkts;             /**< Number of (uncompressed)
					    *  packets sent via autoflush last time
					    *  we checked */
	Dataset      *cap;                 /**< List of supported node features
	                                    *   and their respective support
	                                    *   values.  This is used to negotiate
	                                    *   features with a remotely
	                                    *   connected node */

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
	unsigned char keep     : 1;        /**< Determines whether or not we
	                                    *   wish to maintain this connection.
	                                    *   If no, it will be dropped next
	                                    *   heartbeat tick */
	unsigned char incoming : 1;        /**< If TRUE, this session was
	                                    *   established by a node that
	                                    *   connected to us.  This value is
	                                    *   really only useful when
	                                    *   determining if a port verification
	                                    *   test is necessary */
	unsigned char verified : 1;        /**< Determines whether or not the
	                                    *   port verification has already
	                                    *   occurred.  This will hopefully
	                                    *   go away soon as we can implement
	                                    *   this logic abstractly through
	                                    *   ft_node_set_port et al */
	ft_purpose_t  purpose;             /**< Connection purpose/reason */

	/**
	 * @name Classification Data
	 *
	 * Data that is specific to a particular node classification, and not
	 * utilized by all node types.  You should note that this does not
	 * necessarily indicate whether the class is based on 'self' or the node
	 * this session instance represents.
	 */
	ft_stats_t           stats;        /**< Simply holds the last stats
	                                    *   reply from this user.  Generally
	                                    *   useful for search nodes to manage
	                                    *   multiple index node replies */

	struct ft_search_db  *search_db;   /**< Encapsulates all the libdb cruft
	                                    *   for ft_search_db.c */
	unsigned int          avail;       /**< Number of open upload positions */

	TCPC                 *verify_ft;   /**< Connection made in an attempt
	                                    *   to verify the port supplied by
	                                    *   this node on handshaking */
	TCPC                 *verify_http; /**< Same as verify_ft, except that
	                                    *   the HTTP port is tested */

	/* temporary hack, pay no attention to me */
	BOOL                  child_eligibility;
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
BOOL ft_session_start (TCPC *c, int incoming);

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

int ft_session_connect (struct ft_node *node, ft_purpose_t goal);
void ft_session_incoming (int fd, input_id id, TCPC *listen);

/*****************************************************************************/

void ft_session_tidy_streams (void);

/*****************************************************************************/
/**
 * Override the previous purpose completely.  This will not handle
 * ::ft_session_drop_purpose, although will warn if setting goal to 0.
 */
void ft_session_set_purpose (struct ft_node *node, ft_purpose_t goal);

/**
 * Add an additional purpose.  You should avoid adding multiple OR'd goals at
 * once.
 */
void ft_session_add_purpose (struct ft_node *node, ft_purpose_t goal);

/**
 * Remove a potentially added purpose from the node.  This will not handle
 * automatic session removal as ::ft_session_drop_purpose will.
 *
 * @return  New purpose.  If 0, no purpose is defined for the session.
 */
ft_purpose_t ft_session_remove_purpose (struct ft_node *node,
                                        ft_purpose_t goal);

/**
 * Identical to ::ft_session_remove_purpose, except that the connection will
 * be [gracefully] terminated if there is no longer any purpose for this
 * connection.
 *
 * @return Boolean which determines whether or not the connection was indeed
 *         dropped.
 */
BOOL ft_session_drop_purpose (struct ft_node *node, ft_purpose_t goal);

/*****************************************************************************/

#endif /* __FT_SESSION_H */
