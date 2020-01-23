/*
 * $Id: gt_http_server.c,v 1.72 2004/06/02 07:13:56 hipnod Exp $
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

#include "gt_xfer_obj.h"
#include "gt_xfer.h"

#include "gt_http_server.h"
#include "gt_http_client.h"

#include "gt_accept.h"
#include "gt_ban.h"
#include "gt_version.h"

/*****************************************************************************/

/* convenient shorthand */
#define CONTENT_URN_FIELD       "X-Gnutella-Content-URN"

#define INCOMING_TIMEOUT        (1 * MINUTES)

/*****************************************************************************/

struct http_incoming
{
	TCPC     *c;
	timer_id  timer;
};

/*****************************************************************************/

static void server_handle_get    (GtTransfer *xfer);
static void get_client_request   (int fd, input_id id,
                                  struct http_incoming *http);
static void send_http_response   (int fd, input_id id, GtTransfer *xfer);

/*****************************************************************************/
/* SERVER HELPERS */

static char *lookup_http_code (int code, char **desc)
{
	char *err;
	char *txt;

	switch (code)
	{
	 case 200: err = "OK";
		txt = "Success";
		break;
	 case 206: err = "Partial Content";
		txt = "Resume successful";
		break;
	 case 403: err = "Forbidden";
		txt = "You do not have access to this file";
		break;
	 case 404: err = "Not Found";
		txt = "File is not available";
		break;
	 case 500: err = "Internal Server Error";
		txt = "Stale file entry, retry later";
		break;
	 case 501: err = "Not Implemented";
		txt = "???";
		break;
	 case 503: err = "Service Unavailable";
		txt = "Upload queue is currently full, please try again later";
		break;
	 default:  err = NULL;
		txt = NULL;
		break;
	}

	if (desc)
		*desc = txt;

	return err;
}

static String *alloc_header (int code)
{
	char   *code_text;
	String *s;

	/* so that we can communicate both the numerical code and the human
	 * readable string */
	if (!(code_text = lookup_http_code (code, NULL)))
		return FALSE;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	string_appendf (s, "HTTP/1.1 %i %s\r\n", code, code_text);

	return s;
}

static void construct_header_va (String *s, int code, va_list args)
{
	char *key;
	char *value;

	/* Add "Server: " header */
	string_appendf (s, "Server: %s\r\n", gt_version ());

	for (;;)
	{
		if (!(key = va_arg (args, char *)))
			break;

		if (!(value = va_arg (args, char *)))
			continue;

		string_appendf (s, "%s: %s\r\n", key, value);
	}

	/* append final message terminator */
	string_append (s, "\r\n");
}

static String *construct_header (int code, ...)
{
	String *s;
	va_list args;

	if (!(s = alloc_header (code)))
		return NULL;

	va_start (args, code);
	construct_header_va (s, code, args);
	va_end (args);

	return s;
}

/*
 * Construct and send a server reply.
 */
static BOOL gt_http_server_send (TCPC *c, int code, ...)
{
	String *s;
	int     ret;
	size_t  len;
	va_list args;

	if (!(s = alloc_header (code)))
		return FALSE;

	va_start (args, code);

	construct_header_va (s, code, args);

	va_end (args);

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "sending reply to client =\n%s", s->str);

	len = s->len;
	ret = tcp_send (c, s->str, s->len);

	string_free (s);

	return (ret == len);
}

static char *get_error_page (GtTransfer *xfer, int code)
{
	char *page;
	char *err;
	char *errtxt = NULL;

	if (!(err = lookup_http_code (code, &errtxt)))
		return 0;

	page = stringf ("<h1>%i %s</h1><br>%s.", code, err, errtxt);

	return page;
}

static BOOL supports_queue (GtTransfer *xfer)
{
	char *features;

	if (dataset_lookupstr (xfer->header, "x-queue"))
		return TRUE;

	if ((features = dataset_lookupstr (xfer->header, "x-features")))
	{
		/* XXX: case-sensitive */
		if (strstr (features, "queue"))
			return TRUE;
	}

	return FALSE;
}

static char *get_queue_line (GtTransfer *xfer)
{
	String *s;

	/* do nothing if not queued */
	if (xfer->queue_pos == 0)
		return NULL;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	string_appendf (s, "position=%d,length=%d,pollMin=%d,pollMax=%d",
	                xfer->queue_pos, xfer->queue_ttl, 45, 120);

	return string_free_keep (s);
}

static String *get_error_header (GtTransfer *xfer, int code,
                                 const char *error_page)
{
	size_t  len;
	char    content_len[256];
	char   *queue_line      = NULL;
	char   *content_type    = "text/html";
	String *s;

	len = strlen (error_page);
	snprintf (content_len, sizeof (content_len), "%u", len);

	if (code == 503 && supports_queue (xfer))
		queue_line = get_queue_line (xfer);

	/* don't send a content-type header if there is no entity body */
	if (len == 0)
		content_type = NULL;

	s = construct_header (code,
	                      "Content-Type",    content_type,
	                      "Content-Length",  content_len,
	                      CONTENT_URN_FIELD, xfer->content_urns,
	                      "X-Queue",         queue_line,
	                      NULL);

	free (queue_line);
	return s;
}

static void send_error_reply (int fd, input_id id, GtTransfer *xfer)
{
	String     *s;
	const char *error_page;
	int         ret;
	TCPC       *c;

	c = gt_transfer_get_tcpc (xfer);

	if (!(error_page = get_error_page (xfer, xfer->code)))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * If the remote end supports queueing or supplied
	 * "X-Gnutella-Content-URN",, it's a Gnutella client and we don't want to
	 * keep sending the error page, because over many requests from this
	 * client the bandwidth could add up.
	 */
	if (supports_queue (xfer) ||
	    dataset_lookupstr (xfer->header, "x-gnutella-content-urn"))
	{
		error_page = ""; /* empty */
	}

	if (!(s = get_error_header (xfer, xfer->code, error_page)))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	string_append (s, error_page);

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "sending reply to client =\n%s", s->str);

	/* send the whole page at once */
	ret = tcp_send (c, s->str, s->len);

	/* if the whole thing was sent keep the connection open */
	if (ret == s->len)
	{
		xfer->transmitted_hdrs = TRUE;
		xfer->remaining_len    = 0;
	}

	string_free (s);

	gt_transfer_close (xfer, FALSE);
}

static void gt_http_server_send_error_and_close (GtTransfer *xfer, int code)
{
	TCPC *c;

	c = gt_transfer_get_tcpc (xfer);

	xfer->code = code;

	input_remove_all (c->fd);
	input_add (c->fd, xfer, INPUT_WRITE,
	           (InputCallback)send_error_reply, TIMEOUT_DEF);
}

/*****************************************************************************/

/* parse the Range: bytes=0-10000 format */
static void parse_client_request_range (Dataset *dataset,
                                        off_t *r_start, off_t *r_stop)
{
	char         *range;
	off_t         start;
	off_t         stop;

	if (!r_start && !r_stop)
		return;

	if (r_start)
		*r_start = 0;
	if (r_stop)
		*r_stop = 0;

	/* leave stop as 0 if we can't figure anything out yet.  This is expected
	 * to be handled separately by GET and PUSH */
	if (!(range = dataset_lookupstr (dataset, "range")))
		return;

	/* WARNING: this butchers the data in the dataset! */
	string_sep     (&range, "bytes");
	string_sep_set (&range, " =");

	if (!range)
	{
		if (HTTP_DEBUG)
			GT->DBGFN (GT, "error parsing Range: header");

		return;
	}

	start = (off_t) ATOI (string_sep (&range, "-"));
	stop  = (off_t) ATOI (string_sep (&range, " "));

	/*
	 * The end of the range is optional (e.g. "Range: 0-"), and in that case
	 * stop == 0. In the case of a single byte file, stop == 1.
	 *
	 * TODO: this is broken for one-byte requests at the start of the file.
	 */
	if (stop > 0)
		stop = stop + 1;

	if (r_start)
		*r_start = start;
	if (r_stop)
		*r_stop = stop;
}

/*
 * Parse requests of the forms:
 *
 *  /get/47/File maybe with spaces HTTP
 *  /get/47/files_%20_url_encoded HTTP/1.1
 *  /uri-res/N2R?urn:sha1:ABCD HTTP/1.0
 *
 * and combinations thereof. The "HTTP" trailer is mandatory.
 */
static void get_request_and_version (char *data, char **request,
                                     char **version)
{
	size_t  len;
	char   *next;
	char   *dup;
	char   *http_version = NULL;

	*request = NULL;
	*version = NULL;

	/* trim whitespace inbetween command and request */
	string_trim (data);

	if (!(dup = STRDUP (data)))
		return;

	string_upper (dup);

	next = dup;

	/* find the last instance of "HTTP" in the string */
	while ((next = strstr (next, "HTTP")))
	{
		http_version = next;
		next += sizeof ("HTTP") - 1;
	}

	/* the rest of the string must be the request */
	if (http_version != NULL && http_version != dup)
	{
		len = http_version - dup;
		data[len - 1] = 0;

		*request = data;
		*version = data + len;
	}

	free (dup);
}

/*
 * Break down the clients HTTP request
 */
static int parse_client_request (Dataset **r_dataset, char **r_command,
								 char **r_request, char **r_version,
								 off_t *r_start, off_t *r_stop, char *hdr)
{
	Dataset *dataset = NULL;
	char    *command; /* GET */
	char    *request; /* /file.tar.gz */
	char    *version; /* HTTP/1.1 */
	char    *req_line;

	if (!hdr)
		return FALSE;

	/*
	 * Get the first line of the request
	 */
	req_line = string_sep_set (&hdr, "\r\n");

	/* get the command (GET, HEAD, etc.) */
	command = string_sep (&req_line, " ");

	/*
	 * Handle non-url-encoded requests as well as encoded
	 * ones and get the request and version from this HTTP line.
	 */
	get_request_and_version (req_line, &request, &version);

	if (HTTP_DEBUG)
	{
		GT->DBGFN (GT, "command=%s version=%s request=%s",
		           command, version, request);
	}

	if (!request || string_isempty (request))
		return FALSE;

	if (r_command)
		*r_command = command;
	if (r_request)
		*r_request = request;
	if (r_version)
		*r_version = version;

	gt_http_header_parse (hdr, &dataset);

	if (r_dataset)
		*r_dataset = dataset;

	/* handle Range: header */
	parse_client_request_range (dataset, r_start, r_stop);

	if (r_start && r_stop)
	{
		if (HTTP_DEBUG)
			GT->dbg (GT, "range: [%i, %i)", *r_start, *r_stop);
	}

	return TRUE;
}

/*****************************************************************************/

/*
 * Send the request reply back to the client
 *
 * NOTE:
 * This is used by both GET / and PUSH /
 */
static void reply_to_client_request (GtTransfer *xfer)
{
	TCPC       *c;
	Chunk      *chunk;
	off_t       entity_size;
	char        range[128];
	char        length[32];
	BOOL        ret;

	if (!xfer)
		return;

	c     = gt_transfer_get_tcpc  (xfer);
	chunk = gt_transfer_get_chunk (xfer);

	/*
	 * Determine the "total" entity body that we have locally, not necessarily
	 * the data that we are uploading.  HTTP demands this, but OpenFT really
	 * doesn't give a shit.
	 *
	 * NOTE:
	 * This only works to standard when operating on a GET / request, PUSH's
	 * merely use the range!
	 */
	if (xfer->open_path_size)
		entity_size = xfer->open_path_size;
	else
		entity_size = xfer->stop - xfer->start;

	/* NOTE: we are "working" around the fact that HTTP's Content-Range
	 * reply is inclusive for the last byte, whereas giFT's is not. */
	snprintf (range, sizeof (range) - 1, "bytes %i-%i/%i",
	          (int) xfer->start, (int) (xfer->stop - 1), (int) entity_size);

	snprintf (length, sizeof (length) - 1, "%i",
	          (int) (xfer->stop - xfer->start));

	ret = gt_http_server_send (c, xfer->code,
	                           "Content-Range",   range,
	                           "Content-Length",  length,
	                           "Content-Type",    xfer->content_type,
	                           CONTENT_URN_FIELD, xfer->content_urns,
	                           NULL);

	/* if we transmitted all headers successfully, set transmitted_hdrs
	 * to keep the connection alive, possibly */
	if (ret)
		xfer->transmitted_hdrs = TRUE;
}

/*****************************************************************************/

static void http_incoming_free (struct http_incoming *incoming)
{
	timer_remove (incoming->timer);
	free (incoming);
}

static void http_incoming_close (struct http_incoming *incoming)
{
	gt_http_connection_close (GT_TRANSFER_UPLOAD, incoming->c, TRUE);
	http_incoming_free (incoming);
}

static BOOL http_incoming_timeout (struct http_incoming *incoming)
{
	http_incoming_close (incoming);
	return FALSE;
}

static struct http_incoming *http_incoming_alloc (TCPC *c)
{
	struct http_incoming *incoming;

	incoming = malloc (sizeof (struct http_incoming));
	if (!incoming)
		return NULL;

	incoming->c = c;
	incoming->timer = timer_add (INCOMING_TIMEOUT,
	                             (TimerCallback)http_incoming_timeout,
	                             incoming);

	return incoming;
}

void gt_http_server_dispatch (int fd, input_id id, TCPC *c)
{
	struct http_incoming *incoming;

	if (net_sock_error (c->fd))
	{
		gt_http_connection_close (GT_TRANSFER_UPLOAD, c, TRUE);
		return;
	}

	if (!(incoming = http_incoming_alloc (c)))
	{
		gt_http_connection_close (GT_TRANSFER_UPLOAD, c, TRUE);
		return;
	}

	/* keep track of this incoming connection */
	/* gt_http_connection_insert (GT_TRANSFER_UPLOAD, c); */

	input_remove (id);
	input_add (c->fd, incoming, INPUT_READ,
	           (InputCallback)get_client_request, 0);
}

/*
 * Handle the client's GET commands.
 */
static void get_client_request (int fd, input_id id, struct http_incoming *http)
{
	GtTransfer    *xfer;
	TCPC          *c;
	Dataset       *dataset = NULL;
	char          *command = NULL;
	char          *request = NULL;
	char          *version = NULL;
	off_t          start   = 0;
	off_t          stop    = 0;
	FDBuf         *buf;
	unsigned char *data;
	size_t         data_len = 0;
	int            n;

	c = http->c;
	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		http_incoming_close (http);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);

	if (!gt_http_header_terminated (data, data_len))
		return;

	fdbuf_release (buf);

	if (HTTP_DEBUG)
		GT->DBGSOCK (GT, c, "client request:\n%s", data);

	/* parse the client's request and determine how we should proceed */
	if (!parse_client_request (&dataset, &command, &request, &version,
	                           &start, &stop, data))
	{
		GT->DBGSOCK (GT, c, "invalid http header");
		http_incoming_close (http);
		return;
	}

	/* discard incoming connection timeout maintainance structure */
	http_incoming_free (http);

	/*
	 * We have enough information now to actually allocate the transfer
	 * structure and pass it along to all logic that follows this
	 *
	 * NOTE:
	 * Each individual handler can determine when it wants to let giFT
	 * in on this
	 */
	xfer = gt_transfer_new (GT_TRANSFER_UPLOAD, NULL,
	                        net_peer (c->fd), 0, start, stop);

	/* connect the connection and the xfer in unholy matrimony */
	gt_transfer_set_tcpc (xfer, c);

	/* assign all our own memory */
	xfer->command = STRDUP (command);
	xfer->header  = dataset;
	xfer->version = STRDUP (version);

	if (!gt_transfer_set_request (xfer, request))
	{
		if (HTTP_DEBUG)
			GT->DBGSOCK (GT, c, "invalid request \"s\"", request);

		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* no need for this function again */
	input_remove (id);

	/* figure out how to handle this request */
	if (!strcasecmp (xfer->command, "GET") ||
	    !strcasecmp (xfer->command, "HEAD"))
	{
		server_handle_get (xfer);
		return;
	}

	gt_http_server_send_error_and_close (xfer, 501);
}

/*****************************************************************************/

static Transfer *start_upload (GtTransfer *xfer, Chunk **chunk)
{
	Transfer *transfer;
	char     *user;

	user = net_ip_str (xfer->ip);

	transfer = GT->upload_start (GT, chunk, user, xfer->share_authd,
	                             xfer->start, xfer->stop);

	assert (transfer != NULL);

	return transfer;
}

/* setup the structure for uploading.  this will be called from within
 * client space for PUSH requests as well */
int gt_server_setup_upload (GtTransfer *xfer)
{
	Transfer   *transfer;              /* giFT structure */
	Chunk      *chunk;
	TCPC       *c;

	if (!xfer)
		return FALSE;

	c = gt_transfer_get_tcpc (xfer);
	assert (xfer->chunk == NULL);

	/*
	 * Ban the host if they don't have access -- this gives no information
	 * about whether we have the file or not, and i think this is in violation
	 * of the HTTP spec (supposed to return "404 not found" before 403, but
	 * i'm not sure.
	 */
	if (gt_ban_ipv4_is_banned (c->host))
	{
		xfer->code = 403;
		return FALSE;
	}

	/* open the file that was requested before we go bringing giFT into
	 * this */
	if (!(xfer->f = gt_transfer_open_request (xfer, &xfer->code)))
		return FALSE;

	/* assign stop a value before we proceed */
	if (xfer->stop == 0)
	{
		struct stat st;

		if (!file_stat (xfer->open_path, &st) || st.st_size == 0)
		{
			/* stupid bastards have a 0 length file */
			GT->DBGSOCK (GT, c, "cannot satisfy %s: invalid share",
			             xfer->open_path);
			return FALSE;
		}

		xfer->stop = st.st_size;
		xfer->remaining_len = xfer->stop - xfer->start;
	}

	/* we can now be certain that we are handling a download request from
	 * the client.  allocate the appropriate structures to hook into giFT */
	if (!(transfer = start_upload (xfer, &chunk)))
	{
		GT->DBGFN (GT, "unable to register upload with the daemon");
		return FALSE;
	}

	/* override 200 w/ 206 if the request is not the whole file size */
	if (xfer->remaining_len != xfer->share_authd->size)
		xfer->code = 206;

	/* assign the circular references for passing the chunk along */
	gt_transfer_set_chunk (xfer, chunk);

	/* finally, seek the file descriptor where it needs to be */
	fseek (xfer->f, xfer->start, SEEK_SET);

	return TRUE;
}

static void server_handle_get (GtTransfer *xfer)
{
	TCPC *c;

	c = gt_transfer_get_tcpc (xfer);
	assert (xfer->chunk == NULL);

	/* WARNING: this block is duplicated in http_client:client_push_request */
	if (!gt_server_setup_upload (xfer))
	{
		if (xfer->code == 200)
			xfer->code = 404;

		gt_http_server_send_error_and_close (xfer, xfer->code);
		return;
	}

	input_add (c->fd, xfer, INPUT_WRITE,
	           (InputCallback)send_http_response, TIMEOUT_DEF);
}

static void send_http_response (int fd, input_id id, GtTransfer *xfer)
{
	TCPC *c;

	c = gt_transfer_get_tcpc (xfer);

	if (net_sock_error (c->fd))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* ok, send client the header */
	reply_to_client_request (xfer);

	if (!strcasecmp (xfer->command, "HEAD"))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* disable header read timer */
	timer_remove_zero (&xfer->header_timer);

	input_remove (id);
	input_add (c->fd, xfer, INPUT_WRITE,
			   (InputCallback)gt_server_upload_file, 0);
}

/*
 * Uploads the file requests
 */
void gt_server_upload_file (int fd, input_id id, GtTransfer *xfer)
{
	TCPC        *c;
	Chunk       *chunk;
	char         buf[RW_BUFFER];
	size_t       read_len;
	size_t       size;
	int          sent_len = 0;
	off_t        remainder;

	c     = gt_transfer_get_tcpc (xfer);
	chunk = gt_transfer_get_chunk (xfer);

	assert (xfer->f != NULL);

	/* number of bytes left to be uploaded by this chunk */
	if ((remainder = xfer->remaining_len) <= 0)
	{
		/* for whatever reason this function may have been called when we have
		 * already overrun the transfer...in that case we will simply fall
		 * through to the end-of-transfer condition */
		gt_transfer_write (xfer, chunk, NULL, 0);
		return;
	}

	size = sizeof (buf);

	if (size > remainder)
		size = remainder;

	/*
	 * Ask giFT for the size we should send.  If this returns 0, the upload
	 * was suspended.
	 */
	if ((size = upload_throttle (chunk, size)) == 0)
		return;

	/* read as much as we can from the local file */
	if (!(read_len = fread (buf, sizeof (char), size, xfer->f)))
	{
		GT->DBGFN (GT, "unable to read from %s: %s", xfer->open_path,
		           GIFT_STRERROR ());
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Local read error");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	if ((sent_len = tcp_send (c, buf, MIN (read_len, remainder))) <= 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED,
		                    "Unable to send data block");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* check if the file was too small for the transfer TODO: this isn't
	 * checked earlier, but should be */
	if (read_len != size)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Unexpected end of file");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * Check for short send(). This could use fseek(), but I have this
	 * growing feeling that using stdio everywhere is a bad idea.
	 */
	if (read_len != sent_len)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED, "Short send()");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * Call gt_upload to report back to giFT. This will also cancel
	 * the transfer if the upload has completed.
	 */
	gt_transfer_write (xfer, chunk, buf, sent_len);
}

/*****************************************************************************/

void gt_http_server_reset (TCPC *c)
{
	/*
	 * This can happen because the GtTransfer and TCPC can be decoupled, as in
	 * the case of a push request sent.
	 */
	if (!c)
		return;

	/* finish all queued writes before we reset */
	tcp_flush (c, TRUE);

	/* reset the input state */
	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
			   (InputCallback)gt_http_server_dispatch, 2 * MINUTES);
}
