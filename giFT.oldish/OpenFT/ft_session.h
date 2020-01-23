/*
 * ft_session.h
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

/**
 * OpenFT session data.
 */
typedef struct
{
	unsigned char stage;               /**< current handshaking stage */
	unsigned long start_timer;         /**< when raised the handshaking
	                                    *   stage took too long to complete
	                                    *   and the remote node will be
	                                    *   dropped */
	ListQueue    *queue;               /**< packet write queue to prevent
	                                    *   our local node from writing data
	                                    *   "out of order" according to the
	                                    *   handshaking rules */

	FTStream     *submit;              /**< temporary stream used for share
	                                    *   submission (additions) */
	FTStream     *submit_del;          /**< separate stream for share removal
	                                    *   because FTStream's are not capable
	                                    *   of combining commands */

	time_t        start;               /**< current session start */
} FTSession;

/**
 * Helper macro for accessing session info from a Connection.
 */
#define FT_SESSION(c) (FT_NODE(c)->session)

/*****************************************************************************/

/**
 * Begin an OpenFT session.  This begins the handshaking at stage 1.
 *
 * @param c
 * @param incoming Did this session connect to us?
 *
 * @return Boolean success or failure.
 */
int ft_session_start (Connection *c, int incoming);

/**
 * Halt an OpenFT session.  This disconnects the node and flushes all
 * temporary data.
 */
void ft_session_stop (Connection *c);

/**
 * Calculate the current session's total uptime as of the calling time.
 */
time_t ft_session_uptime (Connection *c);

/**
 * Increment the session stage.
 *
 * @param c
 * @param current Current stage.  This is intended to be a constant value and
 *                will be used to increment the session stage if they match.
 */
void ft_session_stage (Connection *c, unsigned char current);
int  ft_session_queue    (Connection *c, FTPacket *packet);

int  ft_session_connect  (Connection *c);
void ft_session_incoming (Protocol *p, Connection *listen);

/*****************************************************************************/

#endif /* __FT_SESSION_H */
