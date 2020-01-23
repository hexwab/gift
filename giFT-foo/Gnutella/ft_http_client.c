/*
 * $Id: ft_http_client.c,v 1.21 2003/06/06 04:06:32 jasta Exp $
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

#include "ft_xfer.h"

#include "ft_http_client.h"
#include "ft_http_server.h"

#include "gt_accept.h"

/*****************************************************************************/

/*
 * LOTS OF DOCUMENTATION BADLY NEEDED HERE
 */

/*****************************************************************************/

/* prototyping this function effectively provides the non-blocking
 * flow of the program and helps to self-document this file */
static void get_server_reply     (int fd, input_id id, TCPC *c);

/*****************************************************************************/
/* CLIENT HELPERS */

static int gt_http_client_send (TCPC *c, char *command, char *request, ...)
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
	GT->dbgsock (GT, c, "sending client request:\n%s", data);
#endif /* debug */

	return net_send (c->fd, data, data_len);
}

/* parse an HTTP server reply */
static int parse_server_reply (GtTransfer *xfer, TCPC *c, char *reply)
{
	char  *response; /* HTTP/1.1 200 OK */
	char  *version;
	int    code;     /* 200, 404, ... */

	if (!xfer || !reply)
		return FALSE;

#if 1 /* debug */
	GT->dbgsock (GT, c, "reply:\n%s", reply);
#endif

	response = string_sep_set (&reply, "\r\n");

	if (!response)
		return FALSE;

	version =       string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code    = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the rest of the key/value fields */
	http_headers_parse (reply, &xfer->header);

	xfer->code    = code;
	xfer->version = STRDUP (version);

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
	TCPC *c;

	if (!chunk || !xfer)
	{
		GT->DBGFN (GT, "uhm.");
		return;
	}

	xfer->command = STRDUP ("GET");

	if (!(c = gt_http_connection_open (http, xfer->ip, xfer->port,
	                                   GT_TRANSFER_DOWNLOAD)))
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
	TCPC  *c = NULL;
	char   host[128];
	char   range_hdr[64];
	int    ret;

	if (!xfer)
		return FALSE;

	gt_transfer_unref (&c, NULL, &xfer);

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%i-%i",
	          (int) xfer->start, (int) xfer->stop - 1);

	snprintf (host, sizeof (host) - 1, "%s:%hu", net_ip_str (xfer->ip),
	          xfer->port);

	/* always send the Range request just because we always know the full
	 * size we want */
	ret = gt_http_client_send (c, "GET",        xfer->request,
	                           "Range",         range_hdr,
	                           "Host",          host,
	                           "User-Agent",    gt_version(),
	                           NULL);

	return ret;
}

/*
 * Verify connection status and Send the GET request to the server
 */
void gt_http_client_start (int fd, input_id id, TCPC *c)
{
	Chunk       *chunk = NULL;
	GtTransfer  *xfer  = NULL;

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
 * Read the response body, if any, so persistent HTTP will work.
 * Note that the GtTransfer can still timeout, in which case
 * the connection will get closed and so will the xfer.
 */
static void read_response_body (int fd, input_id id, TCPC *c)
{
	GtTransfer  *xfer  = NULL;
	Chunk       *chunk = NULL;
	FDBuf       *buf;
	char        *response;
	int          n;
	int          len;

	gt_transfer_unref (&c, &chunk, &xfer);
	assert (xfer != NULL);
	assert (chunk != NULL);

	len = xfer->stop - xfer->start;

	/* since the body isnt important, close if its too large */
	if (len >= 16384)
	{
		GT->DBGFN (GT, "[%s:%hu] response body too large (%d)",
		           net_ip_str (xfer->ip), xfer->port);
		gt_transfer_close (xfer, TRUE);
		return;
	}

	buf = tcp_readbuf (c);

	if ((n = fdbuf_fill (buf, len)) < 0)
	{
		GT->DBGFN (GT, "error [%s:%hu]: %s",
		           net_ip_str (xfer->ip), xfer->port, GIFT_NETERROR ());
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	/*
	 * Set the body as having been completely read.
	 * This allows the connection to be cached.
	 */
	xfer->remaining_len -= len;
	assert (xfer->remaining_len == 0);

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

#if 0
	GT->dbgsock (GT, c, "body:\n%s", response);
#endif

	input_remove (id);

	/* perform an orderly close */
	gt_transfer_close (xfer, FALSE);
}

/* Try to read any content-body in the response, so that persistent
 * HTTP can work correctly, which is really important for push downloads. */
static void handle_http_error (GtTransfer *xfer, SourceStatus status,
                               char *status_txt)
{
	TCPC    *c     = NULL;
	Chunk   *chunk = NULL;
	int      len;
	char    *content_len;
	char    *conn_hdr;

	/* update the interface protocol with the error status of this xfer */
	gt_transfer_status (xfer, status, status_txt);

	gt_transfer_unref (&c, &chunk, &xfer);
	assert (chunk != NULL);
	assert (c != NULL);

	/*
	 * Check for a Content-Length: field, and use that for the
	 * length of the response body, if any.
	 */
	content_len = dataset_lookupstr (xfer->header, "content-length");
	conn_hdr    = dataset_lookupstr (xfer->header, "connection");

	/*
	 * No Content-Length: header, so don't do persistent HTTP.
	 * Need to check if this is right.
	 *
	 * Also, don't read if they supplied "Connection: Close".
	 */
	if (!content_len || !STRCASECMP (xfer->version, "HTTP/1.0") ||
	    !STRCASECMP (conn_hdr, "close"))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	len = ATOUL (content_len);

	/* abuse xfer->{start,stop} fields for the length of the response body */
	xfer->start = 0;
	xfer->stop  = len;

	/* set this flag to let gt_transfer_close() know the headers
	 * have been parsed */
	xfer->transmitted_hdrs = TRUE;

	/* set the length so that if the connection times out, it will be
	 * force closed when we havent read the entire body */
	xfer->remaining_len = len;

	/* if there is no length to read, we are done */
	if (len == 0)
	{
		gt_transfer_close (xfer, FALSE);
		return;
	}

	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)read_response_body, TIMEOUT_DEF);
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
	TCPC    *c     = NULL;
	Chunk   *chunk = NULL;

	/* successful code, do nothing */
	if (code >= 200 && code <= 299)
		return TRUE;

	gt_transfer_unref (&c, &chunk, &xfer);

	/* likely a fatal error of some kind */
	switch (code)
	{
	 case 404:                     /* not found */
		handle_http_error (xfer, SOURCE_QUEUED_REMOTE, "File not found");
		break;
	 case 401:                     /* unauthorized */
		handle_http_error (xfer, SOURCE_CANCELLED, "Access denied");
		break;
	 case 503:                     /* remotely queued */
		handle_http_error (xfer, SOURCE_QUEUED_REMOTE, "Queued (Remotely)");
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
		GT->DBGFN (GT, "wtf? %i...", code);
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
		GT->dbg (GT, "Content-Range: %s, start=%lu, stop=%lu", content_range,
		         chunk->start, chunk->stop);

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
		GT->DBGFN (GT, "removing source %s", chunk->source->url);
		download_remove_source (chunk->transfer, chunk->source->url);
		chunk->source = NULL;
		return FALSE;
	}

	return TRUE;
}

/*
 * Receive and process the HTTP response
 */
static void get_server_reply (int fd, input_id id, TCPC *c)
{
	Chunk         *chunk    = NULL;
	GtTransfer    *xfer     = NULL;
	FDBuf         *buf;
	unsigned char *data;
	size_t         data_len = 0;
	int            n;

	gt_transfer_unref (&c, &chunk, &xfer);
	assert (chunk != NULL);
	assert (xfer != NULL);

	buf = tcp_readbuf (c);

	/* attempt to read the complete server response */
	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Malformed HTTP header");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);

	if (!http_headers_terminated (data, data_len))
		return;

	fdbuf_release (buf);

	/* parse the server response */
	if (!parse_server_reply (xfer, c, data))
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
	xfer->transmitted_hdrs = TRUE;

	/* wait for the file to be sent by the server */
	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) gt_get_read_file, 0);
}

/*
 * Receive the requested file from the server
 */
void gt_get_read_file (int fd, input_id id, TCPC *c)
{
	Chunk        *chunk = NULL;
	GtTransfer   *xfer  = NULL;
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

	if ((recv_len = tcp_recv (c, buf, size)) <= 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Connection closed");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * We are receiving a file here, so this will always be calling
	 * gt_download.
	 */
	gt_transfer_write (xfer, chunk, buf, recv_len);
}

/*****************************************************************************/

static void client_reset_timeout (int fd, input_id id, TCPC *c)
{
	/* normally we would call recv () here but there's no reason why the server
	 * should be sending stuff down to use...so, disconnect */
	GT->dbgsock (GT, c, "closing client HTTP connection");
	gt_http_connection_close (gt_http, c, TRUE, GT_TRANSFER_DOWNLOAD);
}

void gt_http_client_reset (TCPC *c)
{
	/* This can happen when the GtTransfer and TCPC have been decoupled,
	 * such as when we are waiting for a GIV response from a node. */
	if (!c)
		return;

	tcp_flush (c, TRUE);

	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback) client_reset_timeout, 2 * MINUTES);
}
