/*
 * $Id: gt_http_server.h,v 1.11 2003/07/06 13:05:04 jasta Exp $
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

#ifndef __FT_HTTP_SERVER_H
#define __FT_HTTP_SERVER_H

/*****************************************************************************/

/**
 * @file ft_http_server.h
 *
 * @brief Server-side implementation of the HTTP-based OpenFT transfer
 *        system.
 *
 * See ft_http_client.h for better documentation on the quirks of this
 * system.
 */

/*****************************************************************************/

/**
 * Handle an incoming HTTP connection.  This is attached to from ft_openft.c
 * when it attempts to bind the listening ports.
 */
void ft_http_server_incoming (int fd, input_id id, TCPC *c);

/**
 * Public method used to switch over the clients connection state immediately
 * after delivering a PUSH request from ::ft_http_client_push.
 */
void get_client_request (int fd, input_id id, TCPC *c);

/*****************************************************************************/

#endif /* __FT_HTTP_SERVER_H */
