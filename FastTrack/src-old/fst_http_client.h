/*
 * $Id: fst_http_client.h,v 1.5 2003/11/13 17:48:31 mkern Exp $
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

#ifndef __FST_HTTP_CLIENT_H
#define __FST_HTTP_CLIENT_H

#include <libgift/strobj.h>
#include <libgift/tcpc.h>

#include "fst_http_header.h"

/*****************************************************************************/

/* some time outs in ms */
#define HTCL_CONNECT_TIMEOUT	(15*SECONDS)
#define HTCL_REQUEST_TIMEOUT	(20*SECONDS)
#define HTCL_DATA_TIMEOUT		(20*SECONDS)

/* size of download data buffer,
 * must be large enough to contain http reply header
 */
#define HTCL_DATA_BUFFER_SIZE	4096

/*****************************************************************************/

typedef enum
{
	HTCL_DISCONNECTED = 0,	/* connection closed, initial state */
	HTCL_CONNECTING,		/* connecting to ip */
	HTCL_CONNECTED,			/* tcp connection established */
	HTCL_REQUESTING,		/* sending http request */
	HTCL_RECEIVING,			/* receiving body data */
} FSTHttpClientState;

typedef enum
{
	HTCL_CB_CONNECT_FAILED = 0,	/* connecting failed */
	HTCL_CB_REQUESTING,			/* connected, will send request after cb */
	HTCL_CB_REQUEST_FAILED,		/* request failed */
	HTCL_CB_REPLIED,			/* received http reply */
	HTCL_CB_DATA,				/* called multiple times for body data */
	HTCL_CB_DATA_LAST,			/* called for the last chunk of data or if
								 * connection closed prematurely in which
								 * case client->data_len is 0
								 */
} FSTHttpClientCbCode;


/* called when something interesting happens,
 * return TRUE if you want to continue,
 * returning FALSE will cancel the current request and close
 * the connection without further callbacks
 */
typedef struct _FSTHttpClient FSTHttpClient;

typedef int (*FSTHttpClientCallback)(FSTHttpClient *client,
									 FSTHttpClientCbCode code);

struct _FSTHttpClient
{
	FSTHttpClientState state;

	char	   *host;
	in_addr_t	ip;
	in_port_t	port;
	TCPC	   *tcpcon;
	int			persistent;

	FSTHttpHeader *request;
	FSTHttpHeader *reply;

	unsigned int content_length;	/* http content length */
	unsigned int content_received;	/* total number of downloaded bytes */
	unsigned char *data;			/* buffer of size HTCL_DATA_BUFFER_SIZE */
	unsigned int  data_len;			/* length of content in data */

	FSTHttpClientCallback callback;
	enum { CB_NONE, CB_ACTIVE, CB_FREED } callback_state;

	void *udata;					/* user data */
};

/*****************************************************************************/

/* alloc and init client */
FSTHttpClient *fst_http_client_create (char *host, in_port_t port,
									   FSTHttpClientCallback callback);

/* alloc and init client, reuse tcpcon */
FSTHttpClient *fst_http_client_create_tcpc (TCPC *tcpcon,
											FSTHttpClientCallback callback);

/* free client, closes connection */
void fst_http_client_free (FSTHttpClient *client);

/*****************************************************************************/

/* request file, takes ownership of request,
 * reuses previous connection if possible,
 * tries to keep connection alive if persistent is TRUE
 */
int fst_http_client_request (FSTHttpClient *client, FSTHttpHeader *request,
							 int persistent);

/* cancel current request by closing connection */
int fst_http_client_cancel (FSTHttpClient *client);

/*****************************************************************************/

#endif /* __FST_HTTP_CLIENT_H */
