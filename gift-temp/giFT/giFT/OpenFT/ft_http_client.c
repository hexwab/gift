/*
 * $Id: ft_http_client.c,v 1.28 2003/05/05 09:49:09 jasta Exp $
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

#include "ft_openft.h"

#include "ft_xfer.h"

/*****************************************************************************/

#define HTTP_USER_AGENT platform_version ()

/*****************************************************************************/

/*
 * LOTS OF DOCUMENTATION BADLY NEEDED HERE
 */

/*****************************************************************************/

/* prototyping these functions effectively provides the non-blocking
 * flow of the program and helps to self-document this file */
static void get_complete_connect (int fd, input_id id, TCPC *c);
static void get_server_reply     (int fd, input_id id, TCPC *c);

static void push_complete_connect (int fd, input_id id, TCPC *c);
static void push_server_reply     (int fd, input_id id, TCPC *c);

/*****************************************************************************/
/* CLIENT HELPERS */

static int http_client_send (TCPC *c, char *command, char *request,
                             ...)
{
	char        *key;
	char        *value;
	static char  data[RW_BUFFER];
	size_t       data_len = 0;
	va_list      args;

	if (!command || !request)
		return -1;

	data_len += snprintf (data, sizeof (data) - 1, "%s %s HTTP/1.1\r\n",
	                      command, request);

	va_start (args, request);

	for (;;)
	{
		/* if we receive the sentinel, bail out */
		if (!(key = va_arg (args, char *)))
			break;

		if (!(value = va_arg (args, char *)))
			continue;

		data_len += snprintf (data + data_len, sizeof (data) - 1 - data_len,
		                      "%s: %s\r\n", key, value);
	}

	va_end (args);

	data_len += snprintf (data + data_len, sizeof (data) - 1 - data_len,
	                      "\r\n");

	return tcp_send (c, (unsigned char *)data, data_len);
}

/* generically parse the rest of the Key: Value\r\n sets */
static void client_header_parse (FTTransfer *xfer, char *reply)
{
	char *key;
	char *line;

	if (!xfer || !reply)
		return;

	while ((line = string_sep_set (&reply, "\r\n")))
	{
		key = string_sep (&line, ": ");

		if (!key || !line)
			continue;

		dataset_insertstr (&xfer->header, key, line);
	}
}

/* parse an HTTP server reply */
static int parse_server_reply (FTTransfer *xfer, char *reply)
{
	char *response; /* HTTP/1.1 200 OK */
	int   code;     /* 200, 404, ... */

	if (!xfer || !reply)
		return FALSE;

	response = string_sep_set (&reply, "\r\n");

	if (!response)
		return FALSE;

	/*    */     string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the rest of the key/value fields */
	client_header_parse (xfer, reply);

	xfer->code = code;

	return TRUE;
}

/*****************************************************************************/

/*
 * Complete interface to the standard HTTP GET / with a server.  These routines
 * require non of OpenFT's extensions and communicate perfectly valid
 * HTTP/1.1 (to my knowledge).  You could use this to transfer non-OpenFT,
 * hopefully. ;)
 *
 * NOTE:
 * I need to add more text here so this stands out better as one of the two
 * core subsystems within this file.  So here it is. :P
 */
void http_client_get (Chunk *chunk, FTTransfer *xfer)
{
	TCPC *c;

	if (!chunk || !xfer)
		return;

	xfer->command = STRDUP ("GET");

	if (!(c = http_connection_open (xfer->ip, xfer->port)))
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* pass along the chunk data with this connection.  the chunk data
	 * passes along the FTTransfer data */
	ft_transfer_ref (c, chunk, xfer);
	ft_transfer_status (xfer, SOURCE_WAITING, "Connecting");

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)get_complete_connect, TIMEOUT_DEF);
}

static int client_get_request (FTTransfer *xfer)
{
	TCPC *c = NULL;
	char range_hdr[64];
	int  ret;

	if (!xfer)
		return FALSE;

	ft_transfer_unref (&c, NULL, &xfer);

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%i-%i",
	          (int) xfer->start, (int) xfer->stop);

	/* always send the Range request just because we always know the full
	 * size we want */
	ret = http_client_send (c, "GET",        xfer->request,
	                        "Range",         range_hdr,
	                        "X-OpenftAlias", FT_SELF->alias,
	                        NULL);

	return ret;
}

/*
 * Verify connection status and Send the GET request to the server
 */
static void get_complete_connect (int fd, input_id id, TCPC *c)
{
	Chunk       *chunk = NULL;
	FTTransfer *xfer  = NULL;

	ft_transfer_unref (&c, &chunk, &xfer);

	if (net_sock_error (c->fd))
	{
		ft_transfer_status (xfer, SOURCE_CANCELLED, GIFT_NETERROR ());
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* send the GET / request to the server */
	if (client_get_request (xfer) <= 0)
	{
		ft_transfer_status (xfer, SOURCE_CANCELLED,
		                    "Remote host had an aneurism");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	ft_transfer_status (xfer, SOURCE_WAITING, "Sent HTTP request");

	/* do not remove all fds associated with this socket until we destroy it */
	input_remove (id);

	/* wait for the server response */
	input_add (c->fd, c, INPUT_READ,
			   (InputCallback)get_server_reply, TIMEOUT_DEF);
}

/*
 * Process an HTTP return code (either client or server [push]) and attempt
 * to appropriately handle/expunge the transfer structure accordingly.  The
 * return value indicates whether or not we may continue after the code
 * has been processed.  Some error codes (404) are considered fatal here
 * and you should abort after this call.
 *
 * NOTE:
 * If this function returns FALSE, the calling argument is free'd and you
 * should not access it further.
 */
int http_handle_code (FTTransfer *xfer, int code)
{
	TCPC *c     = NULL;
	Chunk      *chunk = NULL;

	/* successful code, do nothing */
	if (code >= 200 && code <= 299)
		return TRUE;

	ft_transfer_unref (&c, &chunk, &xfer);

	/* likely a fatal error of some kind */
	switch (code)
	{
	 case 404:                     /* not found */
		ft_transfer_status (xfer, SOURCE_QUEUED_REMOTE, "File not found");
		ft_transfer_close (xfer, TRUE);
		break;
	 case 503:                     /* remotely queued */
		{
			char *queue_pos;

			queue_pos =
				dataset_lookupstr (xfer->header, "X-QueuePosition");

			ft_transfer_status (xfer, SOURCE_QUEUED_REMOTE,
			                    stringf ("Queued (position %s)", queue_pos));
			ft_transfer_close (xfer, TRUE);
		}
		break;
	 case 500:                     /* source may not match, check later */
		/*
		 * The remote node has reported that this file has changed
		 * since the last time we received results for it.  This more
		 * than likely indicates a hash change, in which case we should
		 * not keep this associated with this transfer.  Unfortunately,
		 * giFT fails to provide any sort of abstraction for us
		 * to remove sources ourselves, so we need this hopefully
		 * temporary hack to remove the source.
		 */
		download_remove_source (chunk->transfer, chunk->source->url);
		chunk->source = NULL;
		break;
	 default:
		ft_transfer_status (xfer, SOURCE_CANCELLED,
		                    stringf ("Unknown error %i", code));
		ft_transfer_close (xfer, TRUE);
		break;
	}

	return FALSE;
}

/*
 * Receive and process the HTTP response
 */
static void get_server_reply (int fd, input_id id, TCPC *c)
{
	Chunk       *chunk = NULL;
	FTTransfer *xfer  = NULL;
	FDBuf         *buf;
	unsigned char *data;
	int            n;

	ft_transfer_unref (&c, &chunk, &xfer);

	buf = tcp_readbuf (c);

	/* attempt to read the complete server response */
	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		ft_transfer_status (xfer, SOURCE_CANCELLED, "Malformed HTTP header");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* parse the server response */
	if (!parse_server_reply (xfer, data))
	{
		ft_transfer_status (xfer, SOURCE_CANCELLED, "Malformed HTTP header");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * NOTE: if we wanted to do any further processing of the server reply
	 * after GET /, this is where it would be
	 */

	/* determine what to do with the HTTP code reply */
	if (!http_handle_code (xfer, xfer->code))
		return;

	/*
	 * Received HTTP headers, ...and now we are waiting for the file.  This
	 * should be a very short wait :)
	 */
	ft_transfer_status (xfer, SOURCE_WAITING, "Received HTTP headers");

	/* wait for the file to be sent by the server */
	input_remove (id);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback) get_read_file, 0);
}

/*
 * Receive the requested file from the server
 */
void get_read_file (int fd, input_id id, TCPC *c)
{
	Chunk        *chunk = NULL;
	FTTransfer  *xfer  = NULL;
	char          buf[RW_BUFFER];
	size_t        size;
	int           recv_len;

	ft_transfer_unref (&c, &chunk, &xfer);

	/*
	 * Ask giFT for the max size we should read.  If this returns 0, the
	 * download was suspended.
	 */
	if ((size = download_throttle (chunk, sizeof (buf))) == 0)
		return;

	if ((recv_len = tcp_recv (c, buf, size)) <= 0)
	{
		ft_transfer_status (xfer, SOURCE_CANCELLED,
		                    stringf ("Error recving: %s", GIFT_NETERROR()));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * We are receiving a file here, so this will always be calling
	 * ft_download.
	 */
	(*xfer->callback) (chunk, buf, recv_len);
}

/*****************************************************************************/

/*
 * This function is very misleading.  It is a client operation only because
 * it is required to make an outgoing connect, once the initial PUSH
 * request is given to the actual server, this code shifts everything over
 * to the server code.
 *
 * NOTE:
 * The server implementation of this operation does the exact opposite, upon
 * receiving PUSH, moves everything over to this file
 */
void http_client_push (in_addr_t ip, in_port_t port, char *request,
                       off_t start, off_t stop)
{
	TCPC  *c;
	FTTransfer *xfer;

	if (!ip || !port || !request)
		return;

	if (!(c = http_connection_open (ip, port)))
		return;

	/* create the FTTransfer object */
	if (!(xfer = ft_transfer_new (ft_upload, ip, port, start, stop)))
	{
		http_connection_close (c, TRUE);
		return;
	}

	/* fill in the rest of the data */
	xfer->command = STRDUP ("PUSH");

	ft_transfer_ref (c, NULL, xfer);

	if (!ft_transfer_set_request (xfer, request))
	{
		FT->DBGSOCK (FT, c, "invalid request '%s'", request);
		ft_transfer_close (xfer, TRUE);
		return;
	}

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)push_complete_connect, TIMEOUT_DEF);
}

/*
 * Send the PUSH / request
 *
 * NOTE:
 * It's possible for stop to be 0 here
 */
static int client_push_request (FTTransfer *xfer)
{
	TCPC *c = NULL;
	char       *xfer_code;
	char        range_hdr[64];
	int         ret;

	if (!xfer)
		return FALSE;

	ft_transfer_unref (&c, NULL, &xfer);

	/* WARNING: this block is duplicated in http_server.c:server_handle_get */
	if (!server_setup_upload (xfer))
	{
		if (xfer->code == 200)
			xfer->code = 404;

		xfer_code = STRDUP (ITOA (xfer->code));

		switch (xfer->code)
		{
		 case 503:
			{
				char *queue_pos   = STRDUP (ITOA (1));
				char *queue_retry = STRDUP (ITOA (30 * SECONDS));

				ret = http_client_send (c, "PUSH", xfer->request,
				                        "X-HttpCode",      xfer_code,
				                        "X-QueuePosition", queue_pos,
				                        "X-QueueRetry",    queue_retry,
				                        "X-OpenftAlias",   FT_SELF->alias,
				                        NULL);

				free (queue_pos);
				free (queue_retry);
			}
			break;
		 default:
			ret = http_client_send (c, "PUSH",       xfer->request,
			                        "X-HttpCode",    xfer_code,
			                        "X-OpenftAlias", FT_SELF->alias,
			                        NULL);
			break;
		}

		free (xfer_code);

		return ret;
	}

	xfer_code = STRDUP (ITOA (xfer->code));

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%i-%i",
	          (int) xfer->start, (int) xfer->stop);

	ret = http_client_send (c, "PUSH",       xfer->request,
	                        "Range",        (xfer->stop != 0) ? range_hdr : NULL,
	                        "X-HttpCode",    xfer_code,
							"X-OpenftAlias", FT_SELF->alias,
	                        NULL);

	free (xfer_code);

	return ret;
}

/*
 * Verifies the connection and delivers PUSH / to the server
 */
static void push_complete_connect (int fd, input_id id, TCPC *c)
{
	Chunk       *chunk = NULL;
	FTTransfer *xfer  = NULL;

	ft_transfer_unref (&c, &chunk, &xfer);

	if (net_sock_error (c->fd))
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* send the PUSH / request */
	if (client_push_request (xfer) <= 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * Here comes the tricky part.  We have now delivered the PUSH / request,
	 * which is going to get a standard server response accepting our
	 * request to upload the file the server asked for.  We will stay in
	 * client space for this
	 */
	input_remove (id);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)push_server_reply, TIMEOUT_DEF);
}

/*
 * Handle the HTTP server reply to our push request
 *
 * NOTE:
 * This response is pretty meaningless.  Just a courtesy to maintain HTTP
 * sanity
 */
static void push_server_reply (int fd, input_id id, TCPC *c)
{
	Chunk         *chunk = NULL;       /* unavialable */
	FTTransfer    *xfer  = NULL;
	FDBuf         *buf;
	unsigned char *data;
	int            n;

	ft_transfer_unref (&c, &chunk, &xfer);

	buf = tcp_readbuf (c);

	/* attempt to read the complete server response
	 * NOTE: this is duplicated in get_server_reply and should not be!!!! */
	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* parse the server response */
	if (!parse_server_reply (xfer, data))
	{
		FT->DBGSOCK (FT, c, "invalid http header");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* some kinda funky HTTP code, fail */
	if (xfer->code >= 300)
	{
		FT->DBGFN (FT, "received code %i", xfer->code);
		ft_transfer_close (xfer, TRUE);
		return;
	}

	input_remove (id);
	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)server_upload_file, 0);
}

/*****************************************************************************/

static void client_reset_timeout (int fd, input_id id, TCPC *c)
{
	/* normally we would call recv () here but there's no reason why the server
	 * should be sending stuff down to use...so, disconnect */
	http_connection_close (c, TRUE);
}

void http_client_reset (TCPC *c)
{
	tcp_flush (c, TRUE);

	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)client_reset_timeout, TIMEOUT_DEF);
}
