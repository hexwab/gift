/*
 * interface.h
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

#ifndef __INTERFACE_H
#define __INTERFACE_H

/*****************************************************************************/

/**
 * @file interface.h
 *
 * @brief Interface protocol helpers
 *
 * Functions used to both read and write giFT's XML-like interface protocol.
 * These routines provide adequate enough abstraction to arbitrarily change
 * the protocol and still allow giFT and it's user interfaces to communicate
 * effectively.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/**
 * Construct a new interface packet.  This should not be used directly unless
 * you wish to construct before you send out, for some reason
 *
 * @param len   Memory location where the length of the packet will be stored
 * @param event Head tag name
 * @param args  preconstructed argument list
 *
 * @return dynamically allocated string holding the packet ready to be sent
 * to the daemon
 */
char *interface_construct_packet (int *len, char *event, va_list args);

/**
 * Send an interface packet over the connection provided.  A quick example
 * would be:
 *
 * \code
 * interface_send (dc, "search", "query=s", "Paul Oakenfold", NULL);
 * \endcode
 *
 * @param event Head tag name
 * @param ...   See above
 */
void interface_send (Connection *c, char *event, ...);

/**
 * Send an error message over the socket.  This function serves no purpose
 */
void interface_send_err (Connection *c, char *error);

/**
 * Close an interface connection.  If you are writing a user interface, this
 * isn't the function you want.  See \ref connection_close
 */
void interface_close (Connection *c);

/**
 * @brief Parse a complete interface command
 *
 * The "head" of the tag will be retrieved using the reserved key with the same
 * name in the \a dataset.
 *
 * @param dataset Reference to the local table to fill
 * @param packet  Complete command
 *
 * @retval TRUE  Parsing succeeded
 * @retval FALSE Parsing failed
 */
int interface_parse_packet (Dataset **dataset, char *packet);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __INTERFACE_H */
