/*
 * $Id: as_http_server.h,v 1.5 2005/11/26 13:34:05 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_HTTP_SERVER_H
#define __AS_HTTP_SERVER_H

/*****************************************************************************/

/* time after which we disconnect if we didn't get any data */
#define HTSV_REQUEST_TIMEOUT (20*SECONDS)

/* maximum length of http/binary request.
 * connection is dropped if we receive more data but header is still incomplete
 */
#define HTSV_MAX_REQUEST_LEN 4096

/*****************************************************************************/

typedef struct _ASHttpServer ASHttpServer;

/* The server uses three different callbacks depending on what it receives.
 * returning FALSE will close and free the new connection, if TRUE is returned
 * the callback will handle the connection from then on.
 */

/* called when a http request is received.
 * ownership of request analog to that of tcpcon
 */
typedef int (*ASHttpServerRequestCb)(ASHttpServer *server, TCPC *tcpcon,
                                     ASHttpHeader *request);

/* called when a push request is received */
typedef int (*ASHttpServerPushCb)(ASHttpServer *server, TCPC *tcpcon,
                                  String *buf);

/* called when a binary download request was received and successfully
 * decrypted
 */
typedef int (*ASHttpServerBinaryCb)(ASHttpServer *server, TCPC *tcpcon,
                                    ASPacket *request);


struct _ASHttpServer
{
	TCPC *tcpcon;		/* socket we are listening on */
	in_port_t port;		/* port we're listening on */
	input_id input;		/* event triggered on incomming connection */

	ASHttpServerRequestCb request_cb;
	ASHttpServerPushCb    push_cb;
	ASHttpServerBinaryCb  binary_cb;

	int banlist_filter;	/* cache for config key main/banlist_filter */

	List *list;         /* list of connection objects */

	void *udata;        /* user data */
};

/*****************************************************************************/

/* alloc and init server */
ASHttpServer *as_http_server_create (in_port_t port,
                                     ASHttpServerRequestCb request_cb,
                                     ASHttpServerPushCb push_cb,
                                     ASHttpServerBinaryCb binary_cb);

/* free server, close listening port */
void as_http_server_free (ASHttpServer *server);

/* Fake an incoming connection. Used for push replies and kept open upload
 * connections.
 */
void as_http_server_pushed (ASHttpServer *server, TCPC *c);

/*****************************************************************************/

#endif /* __AS_HTTP_SERVER_H */
