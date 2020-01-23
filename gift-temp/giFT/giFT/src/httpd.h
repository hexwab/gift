/*
 * $Id: httpd.h,v 1.2 2003/03/30 19:26:33 jasta Exp $
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

#ifndef __HTTPD_H
#define __HTTPD_H

/*****************************************************************************/

/**
 * @file httpd.h
 *
 * @brief Very primitive HTTP server.
 *
 * This system is nowhere near compliant with the spec.  Lots of work needs
 * to be done here.
 */

/*****************************************************************************/

/**
 * Callback handler for incoming HTTP requests.
 *
 * @param request   Processed GET request.  An example would be "/index.html".
 */
typedef char (*HTTPHandler) (char *request);

/**
 * HTTP handle to use the API defined here.
 */
typedef struct
{
	TCPC       *c;                     /**< Listening socket */
	HTTPHandler handler;               /**< Incoming request hook */
} HTTPD;

/*****************************************************************************/

/**
 * Starts up the HTTP server on the supplied port.
 */
HTTPD *httpd_start (in_port_t port);

/**
 * Close all listening sockets.  This does not terminate any of the currently
 * active HTTP connections.
 */
void httpd_stop (HTTPD *http);

/*****************************************************************************/

/**
 * Registers a request handler.
 */
void httpd_handler (HTTPD *http, HTTPHandler handler);

/*****************************************************************************/

#endif /* __HTTPD_H */
