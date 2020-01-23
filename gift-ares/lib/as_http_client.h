/*
 * $Id: as_http_client.h,v 1.5 2005/09/15 21:13:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_HTTP_CLIENT_H
#define __AS_HTTP_CLIENT_H

/*****************************************************************************/

/* some time outs in ms */
#define HTCL_CONNECT_TIMEOUT	(30*SECONDS)
#define HTCL_REQUEST_TIMEOUT	(30*SECONDS)
#define HTCL_DATA_TIMEOUT		(30*SECONDS)

/* size of download data buffer,
 * must be large enough to contain http reply header
 */
#define HTCL_DATA_BUFFER_SIZE (1024 * 16)

/*****************************************************************************/

typedef enum
{
	HTCL_DISCONNECTED = 0,	/* connection closed, initial state */
	HTCL_CONNECTING,		/* connecting to ip */
	HTCL_CONNECTED,			/* tcp connection established */
	HTCL_REQUESTING,		/* sending http request */
	HTCL_RECEIVING,			/* receiving body data */
} ASHttpClientState;

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
} ASHttpClientCbCode;


/* called when something interesting happens,
 * return TRUE if you want to continue,
 * returning FALSE will cancel the current request and close
 * the connection without further callbacks
 */
typedef struct _ASHttpClient ASHttpClient;

typedef int (*ASHttpClientCallback)(ASHttpClient *client,
                                    ASHttpClientCbCode code);

struct _ASHttpClient
{
	ASHttpClientState state;

	char	   *host;
	in_addr_t	ip;
	in_port_t	port;
	TCPC	   *tcpcon;
	int			persistent;

	ASHttpHeader *request;
	ASHttpHeader *reply;

	unsigned int content_length;	/* http content length */
	unsigned int content_received;	/* total number of downloaded bytes */
	unsigned char *data;			/* buffer of size HTCL_DATA_BUFFER_SIZE */
	unsigned int  data_len;			/* length of content in data */

	ASHttpClientCallback callback;
	enum { CB_NONE, CB_ACTIVE, CB_FREED, CB_RESET } callback_state;

	void *udata;					/* user data */
};

/*****************************************************************************/

/* alloc and init client */
ASHttpClient *as_http_client_create (char *host, in_port_t port,
                                     ASHttpClientCallback callback);

/* alloc and init client, reuse tcpcon */
ASHttpClient *as_http_client_create_tcpc (TCPC *tcpcon,
                                          ASHttpClientCallback callback);

/* free client, closes connection */
void as_http_client_free (ASHttpClient *client);

/*****************************************************************************/

/* request file, takes ownership of request,
 * reuses previous connection if possible,
 * tries to keep connection alive if persistent is TRUE
 */
int as_http_client_request (ASHttpClient *client, ASHttpHeader *request,
                            int persistent);

/* cancel current request by closing connection */
int as_http_client_cancel (ASHttpClient *client);

/*****************************************************************************/

#endif /* __AS_HTTP_CLIENT_H */
