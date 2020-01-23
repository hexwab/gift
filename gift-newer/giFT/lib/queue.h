/*
 * queue.h
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

#ifndef __QUEUE_H
#define __QUEUE_H

/*****************************************************************************/

/**
 * @file queue.h
 *
 * @brief Non-blocking socket write queue
 *
 * Queues up socket writes and waits for either a manual \ref queue_flush or
 * if you are using the event.h systems it will automatically write the data
 * when the supplied socket is able to be written to.
 *
 * Example:
 *
 * \code
 * static int write_func (Connection *c, char *packet, void *udata)
 * {
 *    printf ("sending '%s'...\n", packet);
 *    net_send (c->fd, packet, 0);
 *
 *    // returning TRUE indicates that you wish this queued block to be
 *    // handles once more.  very useful if you are trying to send a file
 *    // over a socket this way.
 *    return FALSE;
 * }
 *
 * static int destroy_func ()
 * {
 *    // we dont need a special deallocator
 *    return FALSE;
 * }
 *
 * List *list = NULL;
 *
 * list = list_append (list, "packet1");
 * list = list_append (list, "packet2");
 * list = list_append (list, "packet3");
 *
 * queue_add (c, (QueueWriteFunc) write_func, (QueueWriteFunc) destroy_func,
 *            list, NULL);
 * // queue_flush would cause all entries to be written right now, or
 * // normally the event system will takeover and write them when able to.
 * \endcode
 */

/*****************************************************************************/

/**
 * Called on each supplied node to the queue
 *
 * @param c     Connection that requests the queued writes
 * @param data
 *   The data supplied to the writing functions.  This data will be passed
 *   along as NULL when all entries have been written.  The sentinel is
 *   given to you so that you may do any extra cleanups that will be
 *   required within your writing functions.
 * @param udata Any arbitrary data that needs to be passed along the queue
 */
typedef int (*QueueWriteFunc) (Connection *c, void *data, void *udata);

/**
 * Write queue structure that contains all buffering for this connection
 */
typedef struct
{
	Connection    *c;                  /**< Connection that created this
										*   write queue instance */
	QueueWriteFunc func;               /**< Main writing function */
	QueueWriteFunc destroy_func;       /**< Assist in destroying data */
	List          *buffer;             /**< List of writes left to perform */
	void          *udata;              /**< Arbitrary data passed along */
} WriteQueue;

/*****************************************************************************/

/**
 * Adds a list of data to the write queue for this connection
 *
 * @param c    Connection that will be passed along to the writing functions
 * @param func
 *   Function called when a node in this list is ready to be written
 * @param destroy_func
 *   Function called when a node in this list needs to be cleaned up
 * @param data
 *   Arbitrary data that you wish to be passed along to the write queue
 *   function handlers
 *
 * @see QueueWriteFunc
 */
void queue_add (Connection *c,
                QueueWriteFunc func, QueueWriteFunc destroy_func,
                List *buffer, void *data);

/**
 * @brief Simplified interface to \ref queue_add
 *
 * This interface will be supplied a single write data explicitly without being
 * required to construct a list to hold it
 *
 * @see queue_add
 */
void queue_add_single (Connection *c, QueueWriteFunc func,
					   QueueWriteFunc destroy_func, void *write_data,
					   void *data);

/**
 * @brief Force all writes to occur
 *
 * You will need to use
 * this function manually if you are not using the event system libgiFT
 * provides.
 *
 * @note This function is called from connection_close in order to make sure
 * that all data queued up can be written before the closure.
 */
void queue_flush (Connection *c);

/**
 * Do not use this function
 */
void queue_free (Connection *c);

/*****************************************************************************/

#endif /* __QUEUE_H */
