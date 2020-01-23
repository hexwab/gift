/*
 * $Id: gt_http_client.c,v 1.57 2006/02/03 20:11:40 mkern Exp $
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
#include "gt_version.h"

#include "gt_xfer_obj.h"
#include "gt_xfer.h"

#include "gt_http_client.h"
#include "gt_http_server.h"

#include "gt_accept.h"

#include "transfer/source.h"

/*****************************************************************************/

/* prototyping this function effectively provides the non-blocking flow of the
 * program and helps to self-document this file */
static void get_server_reply     (int fd, input_id id, GtTransfer *xfer);

/*****************************************************************************/
/* CLIENT HELPERS */

static int gt_http_client_send (TCPC *c, char *command, char *request, ...)
{
	char        *key;
	char        *value;
	String      *s;
	int          ret;
	va_list      args;

	if (!command || !request)
		return -1;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return -1;

	string_appendf (s, "%s %s HTTP/1.1\r\n", command, request);

	va_start (args, request);

	for (;;)
	{
		/* if we receive the sentinel, bail out */
		if (!(key = va_arg (args, char *)))
			break;

		if (!(value = va_arg (args, char *)))
			continue;

		string_appendf (s, "%s: %s\r\n", key, value);
	}

	va_end (args);

	/* append final message terminator */
	string_append (s, "\r\n");

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "sending client request:\n%s", s->str);

	ret = tcp_send (c, s->str, s->len);
	string_free (s);

	return ret;
}

/* parse an HTTP server reply */
static BOOL parse_server_reply (GtTransfer *xfer, TCPC *c, char *reply)
{
	char  *response; /* HTTP/1.1 200 OK */
	char  *version;
	int    code;     /* 200, 404, ... */

	if (!xfer || !reply)
		return FALSE;

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "reply:\n%s", reply);

	response = string_sep_set (&reply, "\r\n");

	if (!response)
		return FALSE;

	version =       string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code    = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the rest of the key/value fields */
	gt_http_header_parse (reply, &xfer->header);

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
void gt_http_client_get (Chunk *chunk, GtTransfer *xfer)
{
	TCPC *c;

	if (!chunk || !xfer)
	{
		GT->DBGFN (GT, "uhm.");
		return;
	}

	xfer->command = STRDUP ("GET");

	if (!(c = gt_http_connection_open (GT_TRANSFER_DOWNLOAD, xfer->ip,
	                                   xfer->port)))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* pass along the connection with the xfer */
	gt_transfer_set_tcpc (xfer, c);
	assert (xfer->chunk == chunk);
	assert (chunk->udata == xfer);

	gt_transfer_status (xfer, SOURCE_WAITING, "Connecting");

	/* be a little more aggressive timing out HTTP connections (TIMEOUT_DEF /
	 * 2 + 5), so that useless sources don't occupy Chunks so often */
	input_add (c->fd, xfer, INPUT_WRITE,
	           (InputCallback)gt_http_client_start, TIMEOUT_DEF / 2 + 5);
}

static int client_get_request (GtTransfer *xfer)
{
	TCPC  *c;
	char   host[128];
	char   range_hdr[64];
	int    ret;

	if (!xfer)
		return FALSE;

	c = gt_transfer_get_tcpc (xfer);

	snprintf (range_hdr, sizeof (range_hdr) - 1, "bytes=%i-%i",
	          (int) xfer->start, (int) xfer->stop - 1);

	snprintf (host, sizeof (host) - 1, "%s:%hu", net_ip_str (xfer->ip),
	          xfer->port);

	/* always send the Range request just because we always know the full size
	 * we want */
	ret = gt_http_client_send (c, "GET",        xfer->request,
	                           "Range",         range_hdr,
	                           "Host",          host,
	                           "User-Agent",    gt_version(),
	                           "X-Queue",       "0.1",
	                           NULL);

	return ret;
}

/*
 * Verify connection status and Send the GET request to the server.
 */
void gt_http_client_start (int fd, input_id id, GtTransfer *xfer)
{
	Chunk    *chunk;
	TCPC     *c;

	c     = gt_transfer_get_tcpc  (xfer);
	chunk = gt_transfer_get_chunk (xfer);

	if (net_sock_error (c->fd))
	{
		GtSource *gt;

		gt = gt_transfer_get_source (xfer);

		/* set the connection as having failed, and retry w/ a push */
		gt->connect_failed = TRUE;

		gt_transfer_status (xfer, SOURCE_CANCELLED, (fd == -1 ?
		                                             "Connect timeout" :
		                                             "Connect failed"));
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * Update the length of the chunk in the GtTransfer. We do this
	 * because giftd may change the range of the chunk while we are
	 * playing with it, and this is the last point where we can update
	 * the range.
	 *
	 * If giftd changes the range after this point, we'll be forced to break
	 * this connection :(
	 */
	gt_transfer_set_length (xfer, chunk);

	/* send the GET / request to the server */
	if (client_get_request (xfer) <= 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "GET send failed");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	gt_transfer_status (xfer, SOURCE_WAITING, "Sent HTTP request");

	/* do not remove all fds associated with this socket until we destroy it */
	input_remove (id);

	/* wait for the server response */
	input_add (fd, xfer, INPUT_READ,
			   (InputCallback)get_server_reply, TIMEOUT_DEF);
}

/*
 * Read the response body, if any, so persistent HTTP will work.
 * Note that the GtTransfer can still timeout, in which case
 * the connection will get closed and so will the xfer.
 */
static void read_response_body (int fd, input_id id, GtTransfer *xfer)
{
	Chunk       *chunk;
	TCPC        *c;
	FDBuf       *buf;
	char        *response;
	int          n;
	int          len;

	c     = gt_transfer_get_tcpc  (xfer);
	chunk = gt_transfer_get_chunk (xfer);

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

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "body:\n%s", response);

	input_remove (id);

	/* perform an orderly close */
	gt_transfer_close (xfer, FALSE);
}

static time_t queue_interval (time_t interval)
{
	/*
	 * HACK: giftd will retry the queued download every 49 seconds,
	 * so round the next retry time to coincide with that interval.
	 */
	if (interval > 49)
		interval = (interval / 49 + 1) * 49;

	return interval;
}

/*****************************************************************************/

/* set the next time to retry from the Retry-After: header */
static void set_retry_after (GtTransfer *xfer)
{
	int          seconds;
	char        *retry_after;
	GtSource    *gt;
#if 0
	char        *msg;
	struct tm   *tm;
#endif

	if (!(retry_after = dataset_lookupstr (xfer->header, "retry-after")))
		return;

	/*
	 * This can be either a HTTP date or a number of seconds. We only
	 * support the number of seconds right now.
	 */
	seconds = ATOI (retry_after);

	if (seconds <= 0)
		return;

	if (!(gt = gt_transfer_get_source (xfer)))
		return;

	/* set the retry time for the next download */
	gt->retry_time = time (NULL) + queue_interval (seconds);

#if 0
	/* get the absolute time */
	tm = localtime (&gt->retry_time);

	/* let the user know when we are going to retry */
	msg = stringf_dup ("Queued (retry at %d:%02d:%02d)", tm->tm_hour,
	                   tm->tm_min, tm->tm_sec);

	gt_transfer_status (xfer, SOURCE_QUEUED_REMOTE, msg);
	free (msg);
#endif
}

/*
 * Check for both the active-queueing style "X-Queue:" and PARQ-style
 * "X-Queued:".
 *
 * We avoid having to parse the X-Queue: line in the PARQ case (which would
 * be "1.0") by allowing X-Queued to override X-Queue.
 */
static size_t find_queue_key (Dataset *header, const char *key)
{
	size_t  pos = 0;
	char   *val;
	char   *line0, *line;
	char   *active_queue_line;
	char   *parq_queue_line;
	char   *sep;

	active_queue_line = dataset_lookupstr (header, "x-queue");
	parq_queue_line   = dataset_lookupstr (header, "x-queued");

	if (!active_queue_line && !parq_queue_line)
		return 0;

	line = active_queue_line;
	sep  = ", ";

	if (parq_queue_line)
	{
		line = parq_queue_line;
		sep  = "; ";
	}

	line0 = line = STRDUP (line);

	while ((val = string_sep_set (&line, sep)))
	{
		char *str;

		str = string_sep_set (&val, "= ");

		if (!str || !val)
			continue;

		if (!strcasecmp (str, key))
			pos = ATOI (val);
	}

	free (line0);
	return pos;
}

/* Create a message describing our position in the remote node's
 * upload queue. */
static char *get_queue_status (GtTransfer *xfer, char *msg)
{
	size_t  len   = 0;
	size_t  pos   = 0;

	pos = find_queue_key (xfer->header, "position");
	len = find_queue_key (xfer->header, "length");

	msg = STRDUP (msg);

	if (pos != 0)
	{
		free (msg);

		if (len != 0)
			msg = stringf_dup ("Queued (%u/%u)", pos, len);
		else
			msg = stringf_dup ("Queued (position %u)", pos);
	}

	return msg;
}

/* set the next time to retry the download based on the X-Queue: field */
static void set_queue_status (GtTransfer *xfer)
{
	GtSource    *gt;
	char        *queue_line;
	int          poll_min;

	if (!(gt = gt_transfer_get_source (xfer)))
		return;

	if (!(queue_line = dataset_lookupstr (xfer->header, "x-queue")))
		return;

	if ((poll_min = find_queue_key (xfer->header, "pollmin")) <= 0)
		return;

	gt->retry_time = time (NULL) + queue_interval (poll_min);
}

/*
 * Try to read any content-body in the response, so that persistent
 * HTTP can work correctly, which is really important for push downloads.
 */
static void handle_http_error (GtTransfer *xfer, SourceStatus status,
                               char *status_txt)
{
	TCPC    *c;
	Chunk   *chunk;
	int      len         = 0;
	char    *content_len;
	char    *conn_hdr;

	/* update the interface protocol with the error status of this xfer */
	status_txt = get_queue_status (xfer, status_txt);
	gt_transfer_status (xfer, status, status_txt);

	free (status_txt);

	c     = gt_transfer_get_tcpc  (xfer);
	chunk = gt_transfer_get_chunk (xfer);

	/*
	 * Check for a Content-Length: field, and use that for the
	 * length of the response body, if any.
	 */
	content_len = dataset_lookupstr (xfer->header, "content-length");
	conn_hdr    = dataset_lookupstr (xfer->header, "connection");

	/* look at the Retry-After: header and set download retry time */
	set_retry_after (xfer);

	/* parse the X-Queue: and X-Queued: headers for the queue position/
	 * retry time */
	set_queue_status (xfer);

	/*
	 * Don't read the body if they supplied "Connection: Close".
	 * Close if the HTTP version is 1.0 (TODO: this should
	 * not close 1.0 connections if they supplied Connection:
	 * Keep-Alive, I guess)
	 */
	if (!STRCASECMP (xfer->version, "HTTP/1.0") ||
	    !STRCASECMP (xfer->version, "HTTP") ||
	    !STRCASECMP (conn_hdr, "close"))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (content_len)
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
	input_add (c->fd, xfer, INPUT_READ,
	           (InputCallback)read_response_body, TIMEOUT_DEF);
}

/*****************************************************************************/

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
	TCPC     *c;
	Chunk    *chunk;
	GtSource *gt;

	/* successful code, do nothing */
	if (code >= 200 && code <= 299)
		return TRUE;

	c     = gt_transfer_get_tcpc  (xfer);
	chunk = gt_transfer_get_chunk (xfer);

	gt = gt_transfer_get_source (xfer);
	assert (gt != NULL);

	/*
	 * We have to be careful not to access the transfer or any
	 * data related to it after calling p->source_abort, which
	 * will cancel the transfer.
	 */
	switch (code)
	{
	 default:
	 case 404:                     /* not found */
		{
			/*
			 * If this was a uri-res request, retry with an index request.
			 * Otherwise, remove the source.
			 *
			 * TODO: perhaps this should retry with url-encoded index
			 * request, before removing?
			 */
			if (gt->uri_res_failed)
			{
				GT->source_abort (GT, chunk->transfer, xfer->source);
			}
			else
			{
				handle_http_error (xfer, SOURCE_QUEUED_REMOTE,
				                   "File not found");
				gt->uri_res_failed = TRUE;
			}
		}
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
		GT->source_abort (GT, chunk->transfer, xfer->source);
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
#if 0
	off_t    file_size;
#endif
	off_t    xfer_size;
	int      error = FALSE;

#if 0
	file_size = chunk->transfer->total;
#endif
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
		if (HTTP_DEBUG)
		{
			GT->dbg (GT, "Content-Range: %s, start=%lu, stop=%lu",
			         content_range, chunk->start, chunk->stop);
		}

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
#if 0
			if (size != file_size)
			{
				GIFT_ERROR (("bad xfer size: %lu, expected %lu", file_size,
				             size));
				error = TRUE;
			}
#endif
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
		GT->source_abort (GT, chunk->transfer, chunk->source);
		return FALSE;
	}

	return TRUE;
}

/*
 * Receive and process the HTTP response
 */
static void get_server_reply (int fd, input_id id, GtTransfer *xfer)
{
	Chunk         *chunk;
	TCPC          *c;
	GtSource      *gt;
	FDBuf         *buf;
	unsigned char *data;
	char          *msg;
	size_t         data_len = 0;
	int            n;

	chunk = gt_transfer_get_chunk (xfer);
	c     = gt_transfer_get_tcpc  (xfer);

	gt = gt_transfer_get_source (xfer);

	buf = tcp_readbuf (c);

	/* attempt to read the complete server response */
	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		msg = "Timed out";

		/*
		 * If the peer abruptly closed the connection, its possible it
		 * didn't understand our request if it was a uri-res request.
		 * So, disable uri-res in that case.
		 */
		if (fd != -1)
		{
			gt->uri_res_failed = TRUE;
			msg = "Connection closed";
		}

		gt_transfer_status (xfer, SOURCE_CANCELLED, msg);
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (gt_fdbuf_full (buf))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);

	if (!gt_http_header_terminated (data, data_len))
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

	/* special case if the request size is 0 */
	if (xfer->remaining_len == 0)
	{
		gt_transfer_close (xfer, FALSE);
		return;
	}

	/* disable the header read timeout */
	timer_remove_zero (&xfer->header_timer);

	/* wait for the file to be sent by the server */
	input_remove (id);
	input_add (fd, xfer, INPUT_READ,
	           (InputCallback)gt_get_read_file, 0);
}

/*
 * Receive the requested file from the server.
 */
void gt_get_read_file (int fd, input_id id, GtTransfer *xfer)
{
	TCPC         *c;
	Chunk        *chunk;
	char          buf[RW_BUFFER];
	size_t        size;
	int           recv_len;

	c     = gt_transfer_get_tcpc  (xfer);
	chunk = gt_transfer_get_chunk (xfer);

	/* set the max initial size of the request */
	size = sizeof (buf);

	/*
	 * Cap the size of the received length at the length
	 * of the request.
	 *
	 * This way we can't receive part of the next request
	 * header with persistent HTTP.
	 */
	if (size > xfer->remaining_len)
		size = xfer->remaining_len;

	/*
	 * Ask giFT for the max size we should read.  If this returns 0, the
	 * download was suspended.
	 */
	if ((size = download_throttle (chunk, size)) == 0)
		return;

	if ((recv_len = tcp_recv (c, buf, size)) <= 0)
	{
		GT->DBGFN (GT, "tcp_recv error (%d) from %s:%hu: %s", recv_len,
		           net_ip_str (c->host), c->port,  GIFT_NETERROR());

		gt_transfer_status (xfer, SOURCE_CANCELLED, "Cancelled remotely");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * We are receiving a file here, so this will always be calling
	 * gt_download.
	 */
	gt_transfer_write (xfer, chunk, buf, (size_t)recv_len);
}

/*****************************************************************************/

static void client_reset_timeout (int fd, input_id id, TCPC *c)
{
	/* normally we would call recv () here but there's no reason why the server
	 * should be sending stuff down to use...so, disconnect */
	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "closing client HTTP connection");

	gt_http_connection_close (GT_TRANSFER_DOWNLOAD, c, TRUE);
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
	           (InputCallback)client_reset_timeout, 2 * MINUTES);
}
