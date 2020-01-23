/*
 * $Id: ft_http_server.c,v 1.42 2003/06/07 05:24:14 hipnod Exp $
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

#include "plugin/share.h"
#include "src/upload.h"

#include "ft_xfer.h"

#include "ft_http_server.h"
#include "ft_http_client.h"

#include "gt_accept.h"

/*****************************************************************************/

struct _indirect_source
{
	Chunk     *chunk;
	uint32_t   ip;
	char      *request;
};

static List      *push_requests = NULL;        /* firewalled downloads */

/*****************************************************************************/

static void server_handle_get    (GtTransfer *xfer);

/*****************************************************************************/
/* SERVER TEMPORARY INDIRECT TRANSFERS
 *
 * NOTE:
 * These are held so that we can make a temporary request for a file we do not
 * immediately have a connection to.  We will simply save the chunk that we
 * were given and wait for it to come back
 *
 * TODO -- timeout each element in this structure
 */

static struct _indirect_source *indirect_lookup (Chunk *chunk, in_addr_t ip,
                                                 char *request)
{
	List *ptr;

	if (!chunk && !request)
		return NULL;

 	if (chunk && (ip || request))
 	{
 		GIFT_WARN (("bad indirect lookup: passed chunk and either ip"));
 		return NULL;
 	}

	for (ptr = push_requests; ptr; ptr = list_next (ptr))
	{
		struct _indirect_source *src = ptr->data;

 		if (chunk && src->chunk == chunk)
 			return src;

 		if (ip != 0 && src->ip != 0 && src->ip != ip)
			continue;

		if (request && strcmp (src->request, request) != 0)
			continue;

		return src;
	}

	return NULL;
}

void gt_http_server_indirect_add (Chunk *chunk, in_addr_t ip, char *request)
{
	struct _indirect_source *src;

	if (!chunk || !request)
		return;

	if (indirect_lookup (NULL, ip, request))
		GIFT_WARN (("adding duplicate sources"));

	if (!(src = malloc (sizeof (struct _indirect_source))))
		return;

	src->chunk   = chunk;
	src->ip      = ip;
	src->request = STRDUP (request);

	push_requests = list_append (push_requests, src);
}

void gt_http_server_indirect_remove (Chunk *chunk, in_addr_t ip, char *request)
{
	struct _indirect_source *src;

	if (!(src = indirect_lookup (chunk, ip, request)))
		return;

	/* TODO -- this is seriously inefficient */
	push_requests = list_remove (push_requests, src);

	free (src->request);
	free (src);
}

Chunk *gt_http_server_indirect_lookup (in_addr_t ip, char *request)
{
	struct _indirect_source *src;

	if (!(src = indirect_lookup (NULL, ip, request)))
		return NULL;

	return src->chunk;
}

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

/*
 * Construct and send a server reply
 *
 * NOTE:
 * A similar function is available in http_client.c
 */
static int gt_http_server_send (TCPC *c, int code, ...)
{
	char       *key;
	char       *value;
	char       *code_text;
	static char data[RW_BUFFER];
	size_t      data_len = 0;
	va_list     args;

	/* so that we can communicate both the numerical code and the human
	 * readable string */
	if (!(code_text = lookup_http_code (code, NULL)))
		return -1;

	data_len += snprintf (data, sizeof (data) - 1, "HTTP/1.1 %i %s\r\n",
	                      code, code_text);

	/* Add "Server: " header */
	data_len += snprintf (data + data_len, sizeof (data) - 1 - data_len,
	                      "Server: %s\r\n", gt_version ());

	va_start (args, code);

	for (;;)
	{
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

#if 1
	GT->dbgsock (GT, c, "sending reply to client =\n%s", data);
#endif

	return net_send (c->fd, data, data_len);
}

static char *get_error_page (TCPC *c, int code)
{
	char *page;
	char *err;
	char *errtxt = NULL;

	if (!(err = lookup_http_code (code, &errtxt)))
		return 0;

	page = stringf ("<h1>%i %s</h1><br>%s.", code, err, errtxt);

	return page;
}

static void get_content_urns (GtTransfer *xfer, char **key, char **val)
{
	*key = NULL;
	*val = NULL;

	if (xfer && xfer->content_urns)
	{
		*key = "X-Gnutella-Content-URN";
		*val = xfer->content_urns;
	}
}

static int gt_http_server_send_error (TCPC *c, int code)
{
	char        *error_page;
	int          len;
	int          sent_len;
	char         content_len[256];
	GtTransfer  *xfer = NULL;
	char        *content_urn_key;
	char        *content_urn_val;

	gt_transfer_unref (&c, NULL, &xfer);

	error_page = get_error_page (c, code);
	assert (error_page != NULL);

	len = strlen (error_page);
	snprintf (content_len, sizeof (content_len) - 1, "%u", len);

	/* Append any content urns */
	get_content_urns (xfer, &content_urn_key, &content_urn_val);

	gt_http_server_send (c, code,
	                     "Content-Type",    "text/html",
	                     "Content-Length",   content_len,
	                     content_urn_key,    content_urn_val,
	                     NULL);

	sent_len = tcp_send (c, error_page, len);

	/* if the whole page was sent, then its safe to use persistent
	 * http by setting the headers to transmitted and the reamining len to
	 * zero */
	if (sent_len == len)
	{
		xfer->transmitted_hdrs = TRUE;
		xfer->remaining_len    = 0;
	}

	return sent_len;
}

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
		GT->DBGFN (GT, "error parsing Range: header");
		return;
	}

	start = (off_t) ATOI (string_sep (&range, "-"));
	stop  = (off_t) ATOI (string_sep (&range, " "));

	/*
	 * The end of the range is optional (e.g. "Range: 0-"),
	 * and in that case stop == 0. In the case of a single
	 * byte file, stop == 1.
	 */
	if (stop > 0)
		stop = stop + 1;

	if (r_start)
		*r_start = start;
	if (r_stop)
		*r_stop = stop;
}

/*
 * Break down the clients HTTP request
 */
static int parse_client_request (Dataset **r_dataset, char **r_command,
								 char **r_request, char **r_version,
								 off_t *r_start, off_t *r_stop, char *data)
{
	Dataset *dataset = NULL;
	char    *command; /* GET */
	char    *request; /* /file.tar.gz */
	char    *version; /* HTTP/1.1 */

	if (!data)
		return FALSE;

	/*
	 * This tries to handle those requests that are URL-encoded
	 * as well as those that are not. This is very fragile right
	 * now.
	 */
	command = string_sep (&data, " ");
	request = string_sep (&data, " HTTP/"); /* argh, case sensitive */
	version = string_sep_set (&data, " \r\n");

	GT->DBGFN (GT, "command=%s version=%s request=%s",
	           command, version, request);

	if (!request)
		return FALSE;

	if (r_command)
		*r_command = command;
	if (r_request)
		*r_request = request;
	if (r_version)
		*r_version = version;

	http_headers_parse (data, &dataset);

	if (r_dataset)
		*r_dataset = dataset;

	/* handle Range: header */
	parse_client_request_range (dataset, r_start, r_stop);

	if (r_start && r_stop)
		GT->dbg (GT, "range: [%i, %i)", *r_start, *r_stop);

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
	TCPC       *c     = NULL;
	Chunk      *chunk = NULL;
	off_t       entity_size;
	char        range[128];
	char        length[32];
	char       *content_urn_key;
	char       *content_urn_val;

	if (!xfer)
		return;

	gt_transfer_unref (&c, &chunk, &xfer);

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
	snprintf (range, sizeof (range) - 1, "bytes=%i-%i/%i",
	          (int) xfer->start, (int) (xfer->stop - 1), (int) entity_size);

	snprintf (length, sizeof (length) - 1, "%i",
	          (int) (xfer->stop - xfer->start));

	get_content_urns (xfer, &content_urn_key, &content_urn_val);

	/* NOTE: X-OpenftAlias is currently not read by this implementation,
	 * it is provided for consistency
	 * TODO: get headers from protocol instead of Content-MD5 */
	gt_http_server_send (c, xfer->code,
	                     "Content-Range",  range,
	                     "Content-Length", length,
	                     "Content-Type",   xfer->content_type,
	                     content_urn_key,  content_urn_val,
	                     NULL);
}

/*****************************************************************************/

/*
 * Accepts an incoming connection from a client wishing to communicate
 * HTTP with this node.  You should keep in mind that this does NOT mean
 * that the connecting client wishes to download a file from us.  They may
 * deliver a PUSH request in which case all of the logic will jump into
 * the client code.
 */
void gt_http_server_incoming (int fd, input_id id, TCPC *c)
{
	TCPC *new_c;

	if (!(new_c = tcp_accept (c, FALSE)))
		return;

    /* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (GNUTELLA_LOCAL_MODE)
	{
		if (!net_match_host (net_ip (net_peer_ip (new_c->fd)),
		                     GNUTELLA_LOCAL_ALLOW))
		{
			tcp_close (new_c);
			return;
		}
	}

	input_add (new_c->fd, new_c, INPUT_READ,
			   (InputCallback) gt_get_client_request, TIMEOUT_DEF);
}

/*
 * Handle the client's GET or PUSH commands
 */
void gt_get_client_request (int fd, input_id id, TCPC *c)
{
	HTTP_Protocol *http    = NULL;
	GtTransfer    *xfer;
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

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		gt_http_connection_close (http, c, TRUE, GT_TRANSFER_UPLOAD);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);

	/* TODO: this should be timed out */
	if (!http_headers_terminated (data, data_len))
		return;

	fdbuf_release (buf);

#if 1
	GT->dbgsock (GT, c, "client request:\n%s", data);
#endif

	/* parse the client's reply and determine how we should proceed */
	if (!parse_client_request (&dataset, &command, &request, &version,
	                           &start, &stop, data))
	{
		GT->dbgsock (GT, c, "invalid http header");
		gt_http_connection_close (http, c, TRUE, GT_TRANSFER_UPLOAD);
		return;
	}

	/*
	 * We have enough information now to actually allocate the transfer
	 * structure and pass it along to all logic that follows this
	 *
	 * NOTE:
	 * Each individual handler can determine when it wants to let giFT
	 * in on this
	 */
	xfer = gt_transfer_new (gt_http, GT_TRANSFER_UPLOAD,
	                        net_ip (net_peer_ip (c->fd)), 0, start, stop);

	gt_transfer_ref (c, NULL, xfer);

	/* assign all our own memory */
	xfer->command = STRDUP (command);
	xfer->header  = dataset;
	xfer->version = STRDUP (version);

	if (!gt_transfer_set_request (xfer, request))
	{
		GT->dbgsock (GT, c, "invalid request '%s'", request);
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

	gt_http_server_send_error (c, 501);
	gt_transfer_close (xfer, FALSE);
}

static Transfer *start_upload (GtTransfer *xfer)
{
	Transfer *transfer;
	char     *user;

	user = net_ip_str (xfer->ip);

	/* TODO: hipnod, xfer->hash should no longer contain the giFT-specific
	 * hash hack... */
	transfer = upload_new (xfer->http->p, user, "SHA1", hashstr_data (xfer->hash),
	                       file_basename (xfer->open_path),
	                       xfer->open_path, xfer->start, xfer->stop,
	                       TRUE, xfer->shared);

	return transfer;
}

/* setup the structure for uploading.  this will be called from within
 * client space for PUSH requests as well */
int gt_server_setup_upload (GtTransfer *xfer)
{
	Transfer   *transfer;              /* giFT structure */
	TCPC       *c = NULL;

	if (!xfer)
		return FALSE;

	gt_transfer_unref (&c, NULL, &xfer);

	/* we have received all the headers, so set this now, regardless
	 * of whether we accept the clients request or not */
	xfer->transmitted_hdrs = TRUE;

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
			GT->dbgsock (GT, c, "cannot satisfy %s: invalid share",
			             xfer->open_path);
			return FALSE;
		}

		xfer->stop = st.st_size;
		xfer->remaining_len = xfer->stop - xfer->start;
	}

	/* we can now be certain that we are handling a download request from
	 * the client.  allocate the appropriate structures to hook into giFT */
	if (!(transfer = start_upload (xfer)))
	{
		GT->DBGFN (GT, "unable to register upload with the daemon");
		return FALSE;
	}

	/* assign the circular references for passing this data along */
	gt_transfer_ref (c, transfer->chunks->data, xfer);

	/* finally, seek the file descriptor where it needs to be */
	fseek (xfer->f, xfer->start, SEEK_SET);

	return TRUE;
}

/*
 * Serve client GET / requests
 */
static void server_handle_get (GtTransfer *xfer)
{
	TCPC *c = NULL;

	if (!xfer)
		return;

	gt_transfer_unref (&c, NULL, &xfer);

	/* WARNING: this block is duplicated in http_client:client_push_request */
	if (!gt_server_setup_upload (xfer))
	{
		/* gracefully reject */
		switch (xfer->code)
		{
		 case 503:
			gt_http_server_send_error (c, xfer->code);
			break;
		 case 200:                     /* general failure */
			xfer->code = 404;
			/* fall through */
		 default:
			gt_http_server_send_error (c, xfer->code);
			break;
		}

		/* perform an orderly close: keep the connection alive if possible */
		gt_transfer_close (xfer, FALSE);
		return;
	}

	/* ok, give the client the accepted header */
	reply_to_client_request (xfer);

	if (!strcmp (xfer->command, "HEAD"))
	{
		gt_transfer_close (xfer, TRUE);
		return;
	}

	input_add (c->fd, c, INPUT_WRITE,
			   (InputCallback) gt_server_upload_file, 0);
}

/*
 * Uploads the file requests
 */
void gt_server_upload_file (int fd, input_id id, TCPC *c)
{
	Chunk       *chunk = NULL;
	GtTransfer *xfer  = NULL;
	char         buf[RW_BUFFER];
	size_t       read_len;
	size_t       size;
	int          sent_len = 0;
	off_t        remainder;

	gt_transfer_unref (&c, &chunk, &xfer);

	assert (xfer->f != NULL);

	/* number of bytes left to be uploaded by this chunk */
	if ((remainder = chunk->stop - (chunk->start + chunk->transmit)) <= 0)
	{
		/* for whatever reason this function may have been called when we have
		 * already overrun the transfer...in that case we will simply fall
		 * through to the end-of-transfer condition */
		gt_transfer_write (xfer, chunk, NULL, 0);
		return;
	}

	/*
	 * Ask giFT for the size we should send.  If this returns 0, the upload
	 * was suspended.
	 */
	if ((size = upload_throttle (chunk, sizeof (buf))) == 0)
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

	/* cap the size we will send just in case there is more available
	 * in the local file than was requested by the remote node */
	if (read_len > remainder)
		read_len = remainder;

	if ((sent_len = tcp_send (c, buf, MIN (read_len, remainder))) <= 0)
	{
		gt_transfer_status (xfer, SOURCE_CANCELLED,
		                    "Unable to send data block");
		gt_transfer_close (xfer, TRUE);
		return;
	}

	/* call gt_upload to report back to giFT */
	gt_transfer_write (xfer, chunk, buf, sent_len);
}

/*****************************************************************************/

void gt_http_server_reset (TCPC *c)
{
	/* This can happen because the GtTransfer and TCPC can be decoupled.
	 * as in the case of a push request sent. */
	if (!c)
		return;

	/* finish all queued writes before we reset */
	tcp_flush (c, TRUE);

	/* reset the input state */
	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
			   (InputCallback) gt_get_client_request, 2 * MINUTES);
}
