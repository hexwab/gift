/*
 * http_client.c
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

#include "openft.h"

#include "nb.h"

#include "xfer.h"

/*****************************************************************************/

#define HTTP_USER_AGENT platform_version ()

/*****************************************************************************/

/*
 * LOTS OF DOCUMENTATION BADLY NEEDED HERE
 */

/*****************************************************************************/

/* prototyping these functions effectively provides the non-blocking
 * flow of the program and helps to self-document this file */
static void get_complete_connect (Protocol *p, Connection *c);
static void get_server_reply     (Protocol *p, Connection *c);

static void push_complete_connect (Protocol *p, Connection *c);
static void push_server_reply     (Protocol *p, Connection *c);

/*****************************************************************************/
/* CLIENT HELPERS */

static int http_client_send (Connection *c, char *command, char *request,
                             ...)
{
	char        *key;
	char        *value;
	static char  data[RW_BUFFER];
	size_t       data_len = 0;
	va_list      args;

	if (!command || !request)
		return -1;

	TRACE(("command='%s', request= '%s'",command,request));

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

	return net_send (c->fd, data, data_len);
}

/* generically parse the rest of the Key: Value\r\n sets */
static void client_header_parse (FT_Transfer *xfer, char *reply)
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

		dataset_insert (xfer->header, key, STRDUP (line));
	}
}

/* parse an HTTP server reply */
static int parse_server_reply (FT_Transfer *xfer, char *reply)
{
	char *response; /* HTTP/1.1 200 OK */
	int   code;     /* 200, 404, ... */

	if (!xfer || !reply)
		return FALSE;

	response = string_sep_set (&reply, "\r\n");
	TRACE (("%s", response));

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
void http_client_get (Chunk *chunk, FT_Transfer *xfer)
{
	Connection *c;

	if (!chunk || !xfer)
	{
		TRACE (("uhm."));
		return;
	}

	xfer->command = STRDUP ("GET");

	if (!(c = http_connection_open (xfer->ip, xfer->port)))
	{
		TRACE (("connection_open failed"));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* pass along the chunk data with this connection.  the chunk data
	 * passes along the FT_Transfer data */
	ft_transfer_ref (c, chunk, xfer);

	input_add (openft_proto, c, INPUT_WRITE,
	           (InputCallback) get_complete_connect, TRUE);
}

static int client_get_request (FT_Transfer *xfer)
{
	Connection *c = NULL;
	char range_hdr[64];
	int  ret;

	if (!xfer)
		return FALSE;

	ft_transfer_unref (&c, NULL, &xfer);

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%lu-%lu",
	          xfer->start, xfer->stop);

	/* always send the Range request just because we always know the full
	 * size we want */
	ret = http_client_send (c, "GET", xfer->request,
	                        "Range", range_hdr,
	                        NULL);

	return ret;
}

/*
 * Verify connection status and Send the GET request to the server
 */
static void get_complete_connect (Protocol *p, Connection *c)
{
	Chunk       *chunk = NULL;
	FT_Transfer *xfer  = NULL;

	ft_transfer_unref (&c, &chunk, &xfer);

	if (net_sock_error (c->fd))
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* send the GET / request to the server */
	if (client_get_request (xfer) <= 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* wait for the reply from the server */
	input_remove (c);
	input_add (p, c, INPUT_READ,
			   (InputCallback) get_server_reply, TRUE);
}

/*
 * Receive and process the HTTP response
 */
static void get_server_reply (Protocol *p, Connection *c)
{
	Chunk       *chunk = NULL;
	FT_Transfer *xfer  = NULL;
	NBRead      *nb;

	ft_transfer_unref (&c, &chunk, &xfer);

	nb = nb_active (c->fd);

	/* attempt to read the complete server response */
	if (nb_read (nb, 0, "\r\n\r\n") <= 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	if (!nb->term)
		return;

	/* parse the server response */
	if (!parse_server_reply (xfer, nb->data))
	{
		nb_finish (nb);

		TRACE_SOCK (("invalid http header"));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * NOTE: if we wanted to do any further processing of the server reply
	 * after GET /, this is where it would be
	 */

	/* determine what to do with the HTTP code reply */
	if (xfer->code >= 200 && xfer->code < 299)
		chunk->source->status = SOURCE_ACTIVE;
	else
	{
		/* error of some kind, figure out what */
		switch (xfer->code)
		{
		 case 404:                     /* remotely queued */
		 case 503:                     /* ...             */
			{
				char *queue_pos;

				queue_pos = dataset_lookup (xfer->header, "X-QueuePosition");

				chunk->source->status      = SOURCE_QUEUED_REMOTE;
				chunk->source->status_data = STRDUP (queue_pos);

				ft_transfer_close (xfer, TRUE);
			}
			break;
		 case 500:                     /* source may not match, check later */
			{
				/* we need to tell giFT that this source can't be trusted
				 * WARNING: this is a bit low-level, giFT should provide
				 * abstraction for this shit
				 * NOTE: if we dont set chunk->source to NULL here,
				 * ft_transfer_close will eventually call download_write,
				 * which will attempt to read chunk->source
				 * NOTE2: this function calls a lot of crazy shit i really
				 * dont remember...sigh */
				download_remove_source (chunk->transfer, chunk->source->url);
				chunk->source = NULL;
			}
			break;
		 default:
			GIFT_WARN (("unsupported error code %i", xfer->code));
			ft_transfer_close (xfer, TRUE);
			break;
		}

		return;
	}

	/* wait for the file to be sent by the server */
	input_remove (c);
	input_add (p, c, INPUT_READ,
	           (InputCallback) get_read_file, FALSE);
}

/*
 * Receive the requested file from the server
 */
void get_read_file (Protocol *p, Connection *c)
{
	Chunk        *chunk = NULL;
	FT_Transfer  *xfer  = NULL;
	char          buf[RW_BUFFER];
	int           recv_len;

	ft_transfer_unref (&c, &chunk, &xfer);

	if ((recv_len = recv (c->fd, buf, sizeof (buf), 0)) <= 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* We are receiving a file here, so this will always be calling
	 * ft_download */
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
void http_client_push (unsigned long ip, unsigned short port, char *request,
                       unsigned long start, unsigned long stop)
{
	Connection  *c;
	FT_Transfer *xfer;

	if (!ip || !port || !request)
	{
		TRACE (("uhm."));
		return;
	}

	if (!(c = http_connection_open (ip, port)))
	{
		TRACE (("connection_open failed"));
		return;
	}

	/* create the FT_Transfer object */
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
		TRACE_SOCK (("invalid request '%s'", request));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	input_add (openft_proto, c, INPUT_WRITE,
	           (InputCallback) push_complete_connect, TRUE);
}

/*
 * Send the PUSH / request
 *
 * NOTE:
 * It's possible for stop to be 0 here
 */
static int client_push_request (FT_Transfer *xfer)
{
	Connection *c = NULL;
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
				                        NULL);

				free (queue_pos);
				free (queue_retry);
			}
			break;
		 default:
			ret = http_client_send (c, "PUSH", xfer->request,
			                        "X-HttpCode", xfer_code,
			                        NULL);
			break;
		}

		free (xfer_code);

		return ret;
	}

	xfer_code = STRDUP (ITOA (xfer->code));

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%lu-%lu",
	          xfer->start, xfer->stop);

	ret = http_client_send (c, "PUSH", xfer->request,
	                        "Range", (xfer->stop != 0) ? range_hdr : NULL,
	                        "X-HttpCode", xfer_code,
	                        NULL);

	free (xfer_code);

	return ret;
}

/*
 * Verifies the connection and delivers PUSH / to the server
 */
static void push_complete_connect (Protocol *p, Connection *c)
{
	Chunk       *chunk = NULL;
	FT_Transfer *xfer  = NULL;

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
	input_remove (c);
	input_add (p, c, INPUT_READ,
	           (InputCallback) push_server_reply, TRUE);
}

/*
 * Handle the HTTP server reply to our push request
 *
 * NOTE:
 * This response is pretty meaningless.  Just a courtesy to maintain HTTP
 * sanity
 */
static void push_server_reply (Protocol *p, Connection *c)
{
	Chunk       *chunk = NULL;         /* unavialable */
	FT_Transfer *xfer  = NULL;
	NBRead      *nb;

	ft_transfer_unref (&c, &chunk, &xfer);

	nb = nb_active (c->fd);

	/* attempt to read the complete server response
	 * NOTE: this is duplicated in get_server_reply and should not be!!!! */
	if (nb_read (nb, 0, "\r\n\r\n") <= 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	if (!nb->term)
		return;

	/* parse the server response */
	if (!parse_server_reply (xfer, nb->data))
	{
		TRACE_SOCK (("invalid http header"));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* some kinda funky HTTP code, fail */
	if (xfer->code >= 300)
	{
		TRACE (("received code %i", xfer->code));
		ft_transfer_close (xfer, TRUE);
		return;
	}

#if 0
	/*
	 * We've verified enough.  It's time to move into server space to complete
	 * the rest of this upload
	 */
	if (!(server_setup_upload (xfer)))
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}
#endif

	input_remove (c);
	input_add (p, c, INPUT_WRITE, (InputCallback) server_upload_file, FALSE);
}

/*****************************************************************************/

static void client_reset_timeout (Protocol *p, Connection *c)
{
	/* normally we would call recv () here but there's no reason why the server
	 * should be sending stuff down to use...so, disconnect */
	http_connection_close (c, TRUE);
}

void http_client_reset (Connection *c)
{
	TRACE_SOCK ((""));

	input_remove (c);
	input_add (c->protocol, c, INPUT_READ,
	           (InputCallback) client_reset_timeout, TRUE);
}
