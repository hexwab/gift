/*
 * $Id: fst_http_server.h,v 1.3 2003/11/28 14:50:15 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_HTTP_SERVER_H
#define __FST_HTTP_SERVER_H

#include <libgift/strobj.h>
#include <libgift/tcpc.h>

#include "fst_http_header.h"

/*****************************************************************************/

/* time after which we disconnect if we didn't get any data */
#define HTSV_REQUEST_TIMEOUT (20*SECONDS)

/* maximum length of http request.
 * connection is dropped if we receive more data but header is still incomplete
 */
#define HTSV_MAX_REQUEST_LEN 4096

/*****************************************************************************/

typedef struct _FSTHttpServer FSTHttpServer;

/* The server uses three different callbacks depending on what it receives.
 * returning FALSE will close and free the new connection, if TRUE is returned
 * the callback will handle the connection from then on.
 */

/* called when a http request is received.
 * ownership of request analog to that of tcpcon
 */
typedef int (*FSTHttpServerRequestCb)(FSTHttpServer *server, TCPC *tcpcon,
									  FSTHttpHeader *request);

/* called when a push request is received */
typedef int (*FSTHttpServerPushCb)(FSTHttpServer *server, TCPC *tcpcon,
								   unsigned int push_id);

/* called for anything else, most likely a fasttrack session attempt */
typedef int (*FSTHttpServerBinaryCb)(FSTHttpServer *server, TCPC *tcpcon);


struct _FSTHttpServer
{
	TCPC *tcpcon;		/* socket we are listening on */
	in_port_t port;		/* port we're listening on */
	input_id input;		/* event triggered on incomming connection */

	FSTHttpServerRequestCb request_cb;
	FSTHttpServerPushCb push_cb;
	FSTHttpServerBinaryCb binary_cb;

	int banlist_filter;	/* cache for config key main/banlist_filter */

	void *udata;		/* user data */
};

/*****************************************************************************/

/* alloc and init server */
FSTHttpServer *fst_http_server_create (in_port_t port,
									   FSTHttpServerRequestCb request_cb,
									   FSTHttpServerPushCb push_cb,
									   FSTHttpServerBinaryCb binary_cb);

/* free server, close listening port */
void fst_http_server_free (FSTHttpServer *server);

/*****************************************************************************/

#endif /* __FST_HTTP_SERVER_H */
