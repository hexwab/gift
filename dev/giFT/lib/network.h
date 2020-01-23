/*
 * network.h
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

#ifndef __NETWORK_H
#define __NETWORK_H

/*****************************************************************************/

/**
 * @file network.h
 *
 * @brief Abstracted socket routines.
 *
 * Contains high-level abstraction of the low-level socket functions.  Most of
 * these routines are meant for TCP socket manipulation only, but UDP will
 * likely come in the future.  UI developers should make sure they see
 * \ref connection_open before using any of these routines.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#include <fcntl.h>
#include <sys/types.h>

/* socket includes */
#ifndef WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif /* !WIN32 */

/*****************************************************************************/

/**
 * Sane size to attempt to read/write during each transmission call.
 */
#define RW_BUFFER       2048

/**
 * Socket layer read/write buffer.  Used for throttling of bandwidth usage
 * during large transfers.
 */
#define RW_SOCKBUFFER   65535  /* socket layer read/write buffer */

/**
 * Close a socket file descriptor.  Use this routine if portability is
 * important to you.
 *
 * @param fd File descriptor to close.
 */
void net_close (int fd);

/**
 * Make an outgoing TCP connection
 *
 * @param ip       IP address (in dotted notation) that you wish to connect to.
 * @param port
 * @param blocking
 *   Boolean value used to set the blocking (or non-blocking) state of the
 *   socket that will be created.  If you're unsure you probably want
 *   blocking set to TRUE.
 *
 * @return Newly created socket.
 *
 * @retval >=0
 *   Successfully created the socket and if blocking, successfully connected
 *   to the remote host.  You should be aware that if you do not set blocking
 *   this call will need to be later checked with \ref net_sock_error.
 * @retval -1   Error.
 */
int net_connect (char *ip, unsigned short port, int blocking);

/**
 * Accept an incoming connection.
 *
 * @note This function creates a new socket using the \em accept call.  For
 *       more information try looking up the man page.
 *
 * @param sock     Bind socket to accept from.
 * @param blocking Blocking state to use for the new socket.
 *
 * @return See accept's manual page.
 */
int net_accept (int sock, int blocking);

/**
 * Bind (and listen) on the specified port.
 *
 * @param port     Port to listen on.
 * @param blocking Blocking state for the new socket.
 *
 * @return Newly created socket.
 */
int net_bind (unsigned short port, int blocking);

/**
 * Manually change the blocking status of a socket that has already been
 * created.
 *
 * @param fd       File descriptor to manipulate.
 * @param blocking Blocking state for the original socket.
 */
int net_set_blocking (int fd, int blocking);

/**
 * @brief Check if the socket has any pending errors resulting from a previous
 *        socket operation.
 *
 * If you are using a non-blocking socket scheme you will need to use this
 * function when the selected socket gains the WRITE state after sending a
 * connect request.
 *
 * @param sock     File descriptor to check.
 *
 * @return If greater than zero, the socket has errors as reported by
 *         getsockopt and the return value reflects this error.  If
 *         less than zero, getsockopt failed.  If exactly 0, no errors
 *         present on this socket.
 */
int net_sock_error (int fd);

/**
 * Send data over the socket
 *
 * @param fd   File descriptor for writing.
 * @param data Linear character array to write.
 * @param len
 *   Length of non-terminated data stream.  Use 0 if you wish to simply call
 *   strlen from within this function.
 *
 * @return See send's manual page.
 */
int net_send (int fd, char *data, size_t len);

/**
 * Wrapper for inet_addr.
 */
in_addr_t net_ip (char *ip_str);

/**
 * Formats an IPv4 address for display.
 *
 * @param ip IPv4 address to format.
 *
 * @return Pointer to internally used memory that describes the dotted
 * quad address notation of the supplied long address.
 */
char *net_ip_str (in_addr_t ip);

/**
 * Determine the peer IP address.
 *
 * @param fd  File descriptor to examine.  This descriptor must be fully
 *            connected in order for this function do anything.
 */
in_addr_t net_peer (int fd);

/**
 * Wrapper for net_peer which passes the result to ::net_ip_str.
 */
char *net_peer_ip (int fd);

/**
 * Returns a network mask.
 *
 * @param bitwidth Width of the mask.
 *
 * @return Network order IPv4 address.
 */
in_addr_t net_mask (int bitwidth);

/**
 * Matches an IP address against a worded notation.
 *
 * @param ip    IPv4 address supplied for comparison.
 * @param match
 *   Worded notation.  Either ALL, LOCAL, or a dotted quad notation with an
 *   optionally appended "/" followed by the bitwidth of the mask to apply.
 *
 * @retval TRUE  Successful match.
 * @retval FALSE No match could be identified.
 */
int net_match_host (in_addr_t ip, char *match);

/*****************************************************************************/

ft_uint8 net_get8 (unsigned char *src);
ft_uint16 net_get16 (unsigned char *src, int tohost);
ft_uint32 net_get32 (unsigned char *src, int tohost);

void net_put8 (unsigned char *dst, ft_uint8 src);
void net_put16 (unsigned char *dst, ft_uint16 src);
void net_put32 (unsigned char *dst, ft_uint32 src);

/*****************************************************************************/

in_addr_t net_local_ip ();
int net_sock_adj_buf (int fd, int buf_name, float factor);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __NETWORK_H */
