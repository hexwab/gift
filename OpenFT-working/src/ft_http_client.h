/*
 * $Id: gt_http_client.h,v 1.12 2003/07/06 13:05:04 jasta Exp $
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

#ifndef __FT_HTTP_CLIENT_H
#define __FT_HTTP_CLIENT_H

/*****************************************************************************/

/**
 * @file ft_http_client.h
 *
 * @brief Client-side HTTP methods.
 *
 * @note The meaning of client is a bit blurred by this design.  For
 *       ft_http_client_.* it simply means the TCP client (the one making the
 *       outgoing TCP connection), however after the initial interface calls it
 *       will come to mean the downloading party.
 */

/*****************************************************************************/

/**
 * Connect to a remote node to deliver a PUSH request.  This is done by the
 * uploading party when the node performing this action can not accept
 * incoming connections.  After the PUSH request is delivered, the state will
 * be transferred into ft_http_server.c:get_client_request to finish the rest
 * of the transfer.
 */
BOOL ft_http_client_push (in_addr_t ip, in_port_t port, const char *request);

/**
 * Perform an outgoing client connection to the host described by the parsed
 * source attached to the transfer object supplied.  After the connection has
 * completed, a GET request will be delivered (based on the Chunk parameters)
 * and the transfer will begin after the servers approval.
 */
BOOL ft_http_client_get (FTTransfer *xfer);

/**
 * Publicly provided input function that will be used by the server
 * implementation to switch states of an incoming PUSH request.  The PUSH
 * will be delivered by a remote node using ::ft_http_client_push.
 */
void get_complete_connect (int fd, input_id id, FTTransfer *xfer);

/*****************************************************************************/

#endif /* __FT_HTTP_CLIENT_H */
