/*
 * nb.h
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

#ifndef __NB_H
#define __NB_H

/*****************************************************************************/

/**
 * @file nb.h
 *
 * @brief Simplified non-blocking socket input
 *
 * NB is a system designed to assist in reading complete packets individually
 * when operating on a non-blocking socket.  These routines are designed to
 * be called from within a select () callback from the event system.  These
 * routines can also be used on blocking sockets by simply wrapping the
 * \ref nb_read function within a while loop.  See below for more info.
 *
 * Sample code:
 *
 * \code
 * void read_function (int fd)
 * {
 *     NBRead *nb;
 *
 *     // create a new nb structure or access a previous one using fd as the
 *     // unique key.
 *     nb = nb_active (fd);
 *
 *     // do not terminate until we see "\r\n"...
 *     if (nb_read (nb, fd, 0, "\r\n") < 0)
 *     {
 *         // handle disconnect
 *         nb_finish (nb);
 *         return;
 *     }
 *
 *     // the data will be terminated when nb_read is completely finished
 *     // reading.  do not process until then.
 *     if (!nb->term)
 *         return;
 *
 *     printf ("read '%s'...\n", nb->data);
 * }
 * \endcode
 */

/*****************************************************************************/

/**
 * Holds state information for the socket input
 */
typedef struct
{
	/**
	 * @name public
	 * Public Variables
	 */

	/* @{ */
	int   flag;                        /**< Arbitrary state incrementation.
	                                    *   Doesn't affect NBRead internally */
	/* @} */

	/**
	 * @name protected
	 * Protected Variables
	 */

	/* @{ */
	char *data;                        /**< Currently read data */
	int   datalen;                     /**< Allocated length.  Used internally
	                                    *   for the realloc logic */
	int   len;                         /**< Currently read length  */
	char  term;                        /**< Boolean flag indicating whether
	                                    *   or not data is terminated.  This
	                                    *   occurs only when the \ref nb_read
	                                    *   conditions are met and the data
	                                    *   is ready to be processed */
	/* @} */

	/**
	 * @name private
	 * Private Variables
	 */

	/* @{ */
	int   type;                        /**< I honestly can't remember */
	int   nb_type;                     /**< Hmm */
	/* @} */
} NBRead;

/*****************************************************************************/

/**
 * @brief Activate an NBRead structure with a unique id
 *
 * This function will return the previously allocated structure if one was
 * already found.  If it cannot be found, it will allocate a new structure
 * and add it to the list.
 *
 * @param uniq_id
 *   Any unique identifier that will be used each call.  Socket
 *   file descriptor is recommended.
 *
 * @return State structure for later manipulation
 */
NBRead *nb_active (int uniq_id);

/**
 * @brief Manually terminate an NB packet
 *
 * Under normal circumstances, you should leave this up to nb_read
 */
void nb_terminate (NBRead *nb);

/**
 * Add a character to the data stream
 *
 * @param c character to append
 */
void nb_add (NBRead *nb, char c);

/**
 * @brief Read data from the socket
 *
 * This function reads data into the state structure pointed to by \a nb
 * attempting to meet the conditions provided by count \strong OR term.
 * Two modes of operation exist with this function, size-specific reading or
 * a crude sentinel string
 *
 * @param source File descriptor to use for reading
 * @param count
 *   Maximum number of bytes to read until determining that this packet is
 *   complete.  If 0, switches to sentinel reading.
 * @param term
 *   Terminates the packet when the supplied sentinel is found.  This has
 *   no effect if count is non-zero.
 *
 * @return Same as the recv call
 */
int nb_read (NBRead *nb, int source, size_t count, char *term);

/**
 * Do not use this function
 */
void nb_free (NBRead *nb);

/**
 * @brief Terminate usage of the state buffer
 *
 * This function frees the data supplied as well as removing it from the
 * state list.  This causes the following nb_active call with this same id
 * to allocate a fresh structure.  Should be called anytime the socket
 * closes
 */
void nb_finish (NBRead *nb);

/**
 * \deprecated This function is replaced by nb_finish
 */
void nb_destroy (int type);

/**
 * \deprecated This function is just stupid
 */
void nb_destroy_all ();

/*****************************************************************************/

#endif /* __NB_H */
