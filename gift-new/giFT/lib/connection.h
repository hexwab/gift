/*
 * connection.h
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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

/*****************************************************************************/

/**
 * @file connection.h
 *
 * @brief High-level socket object
 *
 * This structure is passed along to pretty much every internal giFT function
 * to encompass all the network functionality that will be required.  Also
 * the only way to interface with the event loop.  There's really no point
 * in linking against libgiFT if you don't use these routines.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

struct _protocol;

/**
 * Basic building block for all connections spawned using libgiFT.  If you
 * wish to use libgiFT at all, use these.
 */
typedef struct _connection
{
	/**
	 * @name public
	 * Public Variables
	 */

	/* @{ */
	int               fd;              /**< The file discriptor of this
	                                    *   connection */
	void             *data;            /**< Any arbitrary data that you want
	                                    *   passed along */
	/* @} */

	/**
	 * @name private
	 * Private Variables
	 */

	/* @{ */
	int               ev_id;           /**< Event associated w/ this
	                                    *   connection (you should not write
	                                    *   to this) */
	int               closing;         /**< State set when this socket is
	                                    *   currently being cleaned up (used
	                                    *   to avoid a race condition).  You
	                                    *   probably shouldnt need to mess
	                                    *   with this */
	ListQueue        *write_queue;     /**< Holds the currently pending
	                                    *   write queue elements */
  	float             adjust;          /**< Ratio to adjust the send/recv
	                                    *   buffer */
	struct _protocol *protocol;        /**< Protocol this connection belongs
	                                    *   to */
	/* @} */
} Connection;

/*****************************************************************************/

/**
 * Creates a new connection structure and attempts an outgoing TCP
 * connection using \ref net_connect
 *
 * @param p Protocol that owns this connection.  Pass NULL unless you are
 * developing a protocol plugin
 *
 * @see net_connect
 *
 * @return dynamically allocated Connection structure
 */
Connection *connection_open (struct _protocol *p, char *host,
                             unsigned short port, int blocking);

/**
 * Accepts an incoming socket connection
 *
 * @see connection_open
 * @see net_accept
 */
Connection *connection_accept (struct _protocol *p, Connection *listening,
                               int blocking);

/**
 * Binds to the supplied port
 *
 * @see connection_open
 * @see net_accept
 * @see net_bind
 */
Connection *connection_bind (struct _protocol *p, unsigned short port,
                             int blocking);

/**
 * Close (and free) a connection pointer
 *
 * @param c location of the Connection structure
 */
void connection_close (Connection *c);

/*****************************************************************************/

/**
 * Allocate a new connection structure
 *
 * @see connection_open
 */
Connection   *connection_new        (struct _protocol *p);

/**
 * Free the memory associated with a connection
 *
 * @param c location of the Connection structure
 */
void          connection_free       (Connection *c);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __CONNECTION_H__ */
