/*
 * $Id: ft_http_client.c,v 1.9 2003/05/05 07:29:20 hipnod Exp $
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

#include "gt_gnutella.h"

#include "network.h"
#include "event.h"
#include "transfer.h"

#include "ft_xfer.h"

#include "ft_http_client.h"
#include "ft_http_server.h"

#include "download.h"         /* download_remove_source */

/*****************************************************************************/

#define HTTP_USER_AGENT platform_version ()

/*****************************************************************************/

/*
 * LOTS OF DOCUMENTATION BADLY NEEDED HERE
 */

/*****************************************************************************/

/* prototyping these functions effectively provides the non-blocking
 * flow of the program and helps to self-document this file */
static void get_server_reply     (int fd, input_id id, Connection *c);

static void push_complete_connect (int fd, input_id id, Connection *c);
static void push_server_reply     (int fd, input_id id, Connection *c);

/*****************************************************************************/
/* CLIENT HELPERS */

static int gt_http_client_send (Connection *c, char *command, char *request,
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

#if 1 /* debug */
	TRACE (("%s", data));
#endif /* debug */

	return net_send (c->fd, data, data_len);
}

#if 0
/* generically parse the rest of the Key: Value\r\n sets */
static void client_header_parse (GtTransfer *xfer, char *reply)
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
#endif

static void client_header_parse (GtTransfer *xfer, char *reply)
{
	extern void http_headers_parse (char *headers, Dataset **dataset);

	http_headers_parse (reply, &xfer->header);
}

/* parse an HTTP server reply */
static int parse_server_reply (GtTransfer *xfer, char *reply)
{
	char *response; /* HTTP/1.1 200 OK */
	int   code;     /* 200, 404, ... */

	if (!xfer || !reply)
		return FALSE;

#if 1 /* debug */
	GIFT_DEBUG (("reply = %s", reply));
#endif

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
void gt_http_client_get (HTTP_Protocol *http, Chunk *chunk, GtTransfer *xfer)
{
	Connection *c;

	if (!chunk || !xfer)
	{
		TRACE (("uhm."));
		return;
	}

	xfer->command = STRDUP ("GET");

	if (!(c = gt_http_connection_open (http, xfer->ip, xfer->port)))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* pass along the chunk data with this connection.  the chunk data
	 * passes along the GtTransfer data */
	gt_transfer_ref (c, chunk, xfer);

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback) gt_http_client_start, TIMEOUT_DEF);
}

static int client_get_request (GtTransfer *xfer)
{
	Connection *c = NULL;
	char range_hdr[64];
	int  ret;
	char *host;

	if (!xfer)
		return FALSE;

	gt_transfer_unref (&c, NULL, &xfer);

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%i-%i",
	          (int) xfer->start, (int) xfer->stop - 1);

	host = stringf_dup ("%s:%hu", net_ip_str (xfer->ip), xfer->port);

	/* always send the Range request just because we always know the full
	 * size we want */
	ret = gt_http_client_send (c, "GET",        xfer->request,
	                           "Range",         range_hdr,
	                           "Host",          host,
	                           "User-Agent",    gt_version(),
	                           NULL);

	free (host);

	return ret;
}

/*
 * Verify connection status and Send the GET request to the server
 */
void gt_http_client_start (int fd, input_id id, Connection *c)
{
	Chunk       *chunk = NULL;
	GtTransfer *xfer  = NULL;

	gt_transfer_unref (&c, &chunk, &xfer);

	if (net_sock_error (c->fd))
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, GIFT_NETERROR ());
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* send the GET / request to the server */
	if (client_get_request (xfer) <= 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED,
		                    "Remote host had an aneurism");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	gt_transfer_status (xfer, SOURCE_WAITING, "Sent HTTP request");

	/* do not remove all fds associated with this socket until we destroy it */
	input_remove (id);

	/* wait for the server response */
	input_add (fd, c, INPUT_READ,
			   (InputCallback) get_server_reply, TIMEOUT_DEF);
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
int gt_http_handle_code (GtTransfer *xfer, int code)
{
	Connection *c     = NULL;
	Chunk      *chunk = NULL;

	/* successful code, do nothing */
	if (code >= 200 && code <= 299)
		return TRUE;

	gt_transfer_unref (&c, &chunk, &xfer);

	/* likely a fatal error of some kind */
	switch (code)
	{
	 case 404:                     /* not found */
		gt_transfer_status (xfer, SOURCE_QUEUED_REMOTE, "File not found");
		gt_transfer_close (xfer, TRUE);
		break;
	 case 503:                     /* remotely queued */
		gt_transfer_status (xfer, SOURCE_QUEUED_REMOTE, "Queued (Remotely)");
		gt_transfer_close (xfer, TRUE);
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
		TRACE (("wtf? %i...", code));
		gt_transfer_status (xfer, SOURCE_CANCELLED,
		                    stringf ("Unknown error %i", code));
		gt_transfer_close (xfer, TRUE);
		break;
	}

	return FALSE;
}

static int parse_content_range (char *range, off_t *r_start, off_t *r_end,
                                off_t *r_size)
{
	char *start, *end, *size;

	string_sep (&range, "bytes");
	string_sep_set (&range, " ="); /* accept "bytes=" and "bytes " */

	if (r_end)
		*r_end   = -1;
	if (r_start)
		*r_start = -1;
	if (r_size)
		*r_size  = -1;

	if (!range)
		return FALSE;

	start = string_sep (&range, "-");
	end   = string_sep (&range, "/");
	size  = range;

	if (r_start && start)
		*r_start = ATOUL (start);
	if (r_end && end)
		*r_end   = ATOUL (end);
	if (r_size && size)
		*r_size  = ATOUL (size);

	if (start && end && size)
		return TRUE;

	return FALSE;
}

static int verify_range_response (GtTransfer *xfer, Chunk *chunk)
{
	char    *user_agent;
	char    *content_range;
	char    *content_len;
	off_t    start, stop, size;
	off_t    file_size;
	off_t    xfer_size;
	int      error = FALSE;

	file_size = chunk->transfer->total;
	xfer_size = xfer->stop - xfer->start;

	if ((content_len = dataset_lookupstr (xfer->header, "content-length")))
	{
		size = ATOUL (content_len);

		if (size != xfer_size)
		{
			GIFT_ERROR (("bad content len=%lu, expected %lu", size, xfer_size));
			error = TRUE;
			gt_transfer_status (xfer, SOURCE_CANCELLED, "Bad Content-Length");
		}
	}

	if ((content_range = dataset_lookupstr (xfer->header, "content-range")))
	{
		GIFT_DEBUG (("Content-Range: %s, start=%lu, stop=%lu", content_range,
		             chunk->start, chunk->stop));

		if (parse_content_range (content_range, &start, &stop, &size))
		{
			if (start != xfer->start)
			{
				GIFT_ERROR (("bad xfer start: %lu %lu", xfer->start, start));
				error = TRUE;
			}
			if (stop != xfer->stop - 1)
			{
				GIFT_ERROR (("bad xfer end: %lu %lu", xfer->stop, stop));
				error = TRUE;
			}
			if (size != file_size)
			{
				GIFT_ERROR (("bad xfer size: %lu, expected %lu", file_size,
				             size));
				error = TRUE;
			}
		}
		else
		{
			GIFT_ERROR (("error parsing content-range hdr"));
			error = TRUE;
		}
	}

	if (!content_len && !content_range)
	{
		if (!(user_agent = dataset_lookupstr (xfer->header, "Server")))
			user_agent = dataset_lookupstr (xfer->header, "User-Agent");

		GIFT_ERROR (("missing Content-Range/Length, start=%lu, stop=%lu, "
		             "culprit=%s", xfer->start, xfer->stop, user_agent));

		error = TRUE;
	}

	if (error)
	{
		TRACE (("removing source %s", chunk->source->url));
		download_remove_source (chunk->transfer, chunk->source->url);
		chunk->source = NULL;
		return FALSE;
	}

	return TRUE;
}

/*
 * Receive and process the HTTP response
 */
static void get_server_reply (int fd, input_id id, Connection *c)
{
	Chunk       *chunk = NULL;
	GtTransfer *xfer  = NULL;
	FDBuf         *buf;
	unsigned char *data;
	int            n;

	gt_transfer_unref (&c, &chunk, &xfer);

	buf = tcp_readbuf (c);

	/* attempt to read the complete server response */
	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Malformed HTTP header");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* parse the server response */
	if (!parse_server_reply (xfer, data))
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Malformed HTTP header");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * NOTE: if we wanted to do any further processing of the server reply
	 * after GET /, this is where it would be
	 */

	/* determine what to do with the HTTP code reply */
	if (!gt_http_handle_code (xfer, xfer->code))
		return;

	/* make sure the server understood our "Range:" request */
	if (!verify_range_response (xfer, chunk))
		return;

	/*
	 * Received HTTP headers, ...and now we are waiting for the file.  This
	 * should be a very short wait :)
	 */
	gt_transfer_status (xfer, SOURCE_WAITING, "Received HTTP headers");

	/* wait for the file to be sent by the server */
	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) gt_get_read_file, 0);
}

/*
 * Receive the requested file from the server
 */
void gt_get_read_file (int fd, input_id id, Connection *c)
{
	Chunk        *chunk = NULL;
	GtTransfer  *xfer  = NULL;
	char          buf[RW_BUFFER];
	size_t        size;
	int           recv_len;

	gt_transfer_unref (&c, &chunk, &xfer);

	/*
	 * Ask giFT for the max size we should read.  If this returns 0, the
	 * download was suspended.
	 */
	if ((size = download_throttle (chunk, sizeof (buf))) == 0)
		return;

	if ((recv_len = recv (c->fd, buf, size, 0)) <= 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Connection closed");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * We are receiving a file here, so this will always be calling
	 * gt_download.
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
void gt_http_client_push (HTTP_Protocol *http, in_addr_t ip, unsigned short port,
                       char *request, off_t start, off_t stop)
{
	Connection  *c;
	GtTransfer *xfer;

	if (!ip || !port || !request)
	{
		TRACE (("uhm."));
		return;
	}

	if (!(c = gt_http_connection_open (http, ip, port)))
	{
		TRACE (("connection_open failed"));
		return;
	}

	/* create the GtTransfer object */
	if (!(xfer = gt_transfer_new (http, gt_upload, ip, port, start, stop)))
	{
		gt_http_connection_close (http, c, TRUE);
		return;
	}

	/* fill in the rest of the data */
	xfer->command = STRDUP ("PUSH");

	gt_transfer_ref (c, NULL, xfer);

	if (!gt_transfer_set_request (xfer, request))
	{
		TRACE_SOCK (("invalid request '%s'", request));
		gt_transfer_close (xfer, TRUE);
		return;
	}

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback) push_complete_connect, TIMEOUT_DEF);
}

/*
 * Send the PUSH / request
 *
 * NOTE:
 * It's possible for stop to be 0 here
 */
static int client_push_request (GtTransfer *xfer)
{
	Connection *c = NULL;
	char       *xfer_code;
	char        range_hdr[64];
	int         ret;

	if (!xfer)
		return FALSE;

	gt_transfer_unref (&c, NULL, &xfer);

	/* WARNING: this block is duplicated in http_server.c:server_handle_get */
	if (!gt_server_setup_upload (xfer))
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

				ret = gt_http_client_send (c, "PUSH", xfer->request,
				                        "X-HttpCode",      xfer_code,
				                        "X-QueuePosition", queue_pos,
				                        "X-QueueRetry",    queue_retry,
#if 0 /* FIXME */
				                        "X-OpenftAlias",   FT_SELF->alias,
#endif
				                        NULL);

				free (queue_pos);
				free (queue_retry);
			}
			break;
		 default:
			ret = gt_http_client_send (c, "PUSH",       xfer->request,
			                        "X-HttpCode",    xfer_code,
#if 0 /* FIXME */
			                        "X-OpenftAlias", FT_SELF->alias,
#endif
			                        NULL);
			break;
		}

		free (xfer_code);

		return ret;
	}

	xfer_code = STRDUP (ITOA (xfer->code));

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%i-%i",
	          (int) xfer->start, (int) xfer->stop - 1);

	ret = gt_http_client_send (c, "PUSH",       xfer->request,
	                        "Range",        (xfer->stop != 0) ? range_hdr : NULL,
	                        "X-HttpCode",    xfer_code,
#if 0 /* FIXME */
							"X-OpenftAlias", FT_SELF->alias,
#endif
	                        NULL);

	free (xfer_code);

	return ret;
}

/*
 * Verifies the connection and delivers PUSH / to the server
 */
static void push_complete_connect (int fd, input_id id, Connection *c)
{
	Chunk       *chunk = NULL;
	GtTransfer *xfer  = NULL;

	gt_transfer_unref (&c, &chunk, &xfer);

	if (net_sock_error (c->fd))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* send the PUSH / request */
	if (client_push_request (xfer) <= 0)
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * Here comes the tricky part.  We have now delivered the PUSH / request,
	 * which is going to get a standard server response accepting our
	 * request to upload the file the server asked for.  We will stay in
	 * client space for this
	 */
	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) push_server_reply, TIMEOUT_DEF);
}

/*
 * Handle the HTTP server reply to our push request
 *
 * NOTE:
 * This response is pretty meaningless.  Just a courtesy to maintain HTTP
 * sanity
 */
static void push_server_reply (int fd, input_id id, Connection *c)
{
	Chunk       *chunk = NULL;         /* unavialable */
	GtTransfer  *xfer  = NULL;
	FDBuf         *buf;
	unsigned char *data;
	int            n;

	gt_transfer_unref (&c, &chunk, &xfer);

	buf = tcp_readbuf (c);

	/* attempt to read the complete server response
	 * NOTE: this is duplicated in get_server_reply and should not be!!!! */
	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* parse the server response */
	if (!parse_server_reply (xfer, data))
	{
		TRACE_SOCK (("invalid http header"));
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* some kinda funky HTTP code, fail */
	if (xfer->code >= 300)
	{
		TRACE (("received code %i", xfer->code));
		gt_transfer_close (xfer, TRUE);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_WRITE, (InputCallback) gt_server_upload_file, 0);
}

/*****************************************************************************/

static void client_reset_timeout (int fd, input_id id, Connection *c)
{
	/* normally we would call recv () here but there's no reason why the server
	 * should be sending stuff down to use...so, disconnect */
	gt_http_connection_close (gt_http, c, TRUE);
}

void gt_http_client_reset (Connection *c)
{
	tcp_flush (c, TRUE);

	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback) client_reset_timeout, TIMEOUT_DEF);
}
