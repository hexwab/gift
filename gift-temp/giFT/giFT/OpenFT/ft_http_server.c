/*
 * $Id: ft_http_server.c,v 1.32 2003/05/05 09:49:09 jasta Exp $
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

#include "file.h"

#include "ft_xfer.h"

/*****************************************************************************/

/* do I smell a lan party? */
#define OPENFT_LOCAL_MODE \
    config_get_int (OPENFT->conf, "local/lan_mode=0")
#define OPENFT_LOCAL_ALLOW \
    config_get_str (OPENFT->conf, "local/hosts_allow=LOCAL")

/*****************************************************************************/

struct _indirect_source
{
	Chunk     *chunk;
	uint32_t   ip;
	char      *request;
};

static List *push_requests = NULL;        /* firewalled downloads */

/*****************************************************************************/

static void get_client_request (int fd, input_id id, TCPC *c);
static void server_handle_push (FTTransfer *xfer);
static void server_handle_get  (FTTransfer *xfer);

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

static struct _indirect_source *indirect_lookup (in_addr_t ip, char *request)
{
	List *ptr;

	if (!ip || !request)
		return NULL;

	for (ptr = push_requests; ptr; ptr = list_next (ptr))
	{
		struct _indirect_source *src = ptr->data;

		if (src->ip == ip && !strcmp (src->request, request))
			return src;
	}

	return NULL;
}

static int find_chunk (struct _indirect_source *src, Chunk *chunk)
{
	return (src->chunk != chunk);
}

static struct _indirect_source *indirect_find (Chunk *chunk)
{
	List *ptr;

	if (!chunk)
		return NULL;

	ptr = list_find_custom (push_requests, chunk, (CompareFunc)find_chunk);
	return list_nth_data (ptr, 0);
}

void http_server_indirect_add (Chunk *chunk, in_addr_t ip, char *request)
{
	struct _indirect_source *src;

	if (!chunk || !ip || !request)
		return;

	if (indirect_lookup (ip, request))
		FT->warn (FT, "adding duplicate sources");

	if (!(src = malloc (sizeof (struct _indirect_source))))
		return;

	src->chunk   = chunk;
	src->ip      = ip;
	src->request = STRDUP (request);

	push_requests = list_append (push_requests, src);
}

void http_server_indirect_remove (in_addr_t ip, char *request)
{
	struct _indirect_source *src;

	if (!(src = indirect_lookup (ip, request)))
		return;

	/* TODO -- this is seriously inefficient */
	push_requests = list_remove (push_requests, src);

	free (src->request);
	free (src);
}

Chunk *http_server_indirect_lookup (in_addr_t ip, char *request)
{
	struct _indirect_source *src;

	if (!(src = indirect_lookup (ip, request)))
		return NULL;

	return src->chunk;
}

char *http_server_indirect_find (Chunk *chunk)
{
	struct _indirect_source *src;

	if (!(src = indirect_find (chunk)))
		return NULL;

	return src->request;
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
static int http_server_send (TCPC *c, int code, ...)
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

	return tcp_send (c, (unsigned char *)data, data_len);
}

static int send_error_page (TCPC *c, int code)
{
	char *page;
	char *err;
	char *errtxt = NULL;

	if (!(err = lookup_http_code (code, &errtxt)))
		return 0;

	page = stringf ("<h1>%i %s</h1><br>%s.", code, err, errtxt);

	return tcp_send (c, (unsigned char *)page, strlen (page));
}

/* constructs a friendly message describing the error for browsers */
static int http_server_send_error (TCPC *c, int code)
{
	http_server_send (c, code,
	                  "Content-Type",   "text/html",
	                  NULL);

	return send_error_page (c, code);
}

/* generically parse the rest of the Key: Value\r\n sets */
static void server_header_parse (Dataset **dataset, char *request)
{
	char *key;
	char *line;

	if (!dataset || !request)
		return;

	while ((line = string_sep_set (&request, "\r\n")))
	{
		key = string_sep (&line, ": ");

		if (!key || !line)
			continue;

		dataset_insertstr (dataset, key, line);
	}
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
	if (!(range = dataset_lookupstr (dataset, "Range")))
		return;

	/* WARNING: this butchers the data in the dataset! */
	string_sep (&range, "bytes=");

	if (!range)
	{
		FT->DBGFN (FT, "error parsing Range: header");
		return;
	}

	start = (off_t) ATOI (string_sep (&range, "-"));
	stop  = (off_t) ATOI (string_sep (&range, " "));

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

	command = string_sep     (&data, " ");
	request = string_sep_set (&data, " \r\n");
	version = string_sep_set (&data, " \r\n");

	if (!request)
		return FALSE;

	if (r_command)
		*r_command = command;
	if (r_request)
		*r_request = request;
	if (r_version)
		*r_version = version;

	server_header_parse (&dataset, data);

	if (r_dataset)
		*r_dataset = dataset;

	/* handle Range: header */
	parse_client_request_range (dataset, r_start, r_stop);

	return TRUE;
}

/*****************************************************************************/

/*
 * Send the request reply back to the client
 *
 * NOTE:
 * This is used by both GET / and PUSH /
 */
static void reply_to_client_request (FTTransfer *xfer)
{
	TCPC *c     = NULL;
	Chunk      *chunk = NULL;
	off_t       entity_size;
	char        range[128];
	char        length[32];

	if (!xfer)
		return;

	ft_transfer_unref (&c, &chunk, &xfer);

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

	/* NOTE: X-OpenftAlias is currently not read by this implementation,
	 * it is provided for consistency */
	http_server_send (c, xfer->code,
	                  "Content-Range",  range,
	                  "Content-Length", length,
	                  "Content-Type",   xfer->content_type,
	                  "Content-MD5",    hashstr_data (xfer->hash),
	                  "X-OpenftAlias",  FT_SELF->alias,
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
void http_server_incoming (int fd, input_id id, TCPC *c)
{
	TCPC *new_c;

	if (!(new_c = tcp_accept (c, FALSE)))
		return;

    /* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (OPENFT_LOCAL_MODE)
	{
		if (!net_match_host (net_ip (net_peer_ip (new_c->fd)),
		                     OPENFT_LOCAL_ALLOW))
		{
			tcp_close (new_c);
			return;
		}
	}

	input_add (new_c->fd, new_c, INPUT_READ,
			   (InputCallback)get_client_request, TIMEOUT_DEF);
}

/*
 * Handle the client's GET or PUSH commands
 */
static void get_client_request (int fd, input_id id, TCPC *c)
{
	FTTransfer    *xfer;
	FTTransferCB   cb;
	Dataset       *dataset = NULL;
	char          *command = NULL;
	char          *request = NULL;
	char          *version = NULL;
	off_t          start   = 0;
	off_t          stop    = 0;
	FDBuf         *buf;
	unsigned char *data;
	int            n;

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		http_connection_close (c, TRUE);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* parse the client's reply and determine how we should proceed */
	if (!parse_client_request (&dataset, &command, &request, &version,
	                           &start, &stop, data))
	{
		FT->DBGSOCK (FT, c, "invalid http header");
		http_connection_close (c, TRUE);
		return;
	}

	/* determine the transfer callback */
	cb = (!strcmp (command, "PUSH") ? ft_download : ft_upload);

	/*
	 * We have enough information now to actually allocate the transfer
	 * structure and pass it along to all logic that follows this
	 *
	 * NOTE:
	 * Each individual handler can determine when it wants to let giFT
	 * in on this
	 */
	xfer = ft_transfer_new (cb, net_ip (net_peer_ip (c->fd)), 0,
	                        start, stop);

	ft_transfer_ref (c, NULL, xfer);

	/* assign all our own memory */
	xfer->command = STRDUP (command);
	xfer->header  = dataset;
	xfer->version = STRDUP (version);

	if (!ft_transfer_set_request (xfer, request))
	{
		FT->DBGSOCK (FT, c, "invalid request '%s'", request);
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* no need for this function again */
	input_remove (id);

	/* figure out how to handle this request */
	if (!strcmp (xfer->command, "PUSH"))
		server_handle_push (xfer);
	else if (!strcmp (xfer->command, "GET") || !strcmp (xfer->command, "HEAD"))
		server_handle_get (xfer);
	else
		http_server_send_error (c, 501);
}

/*
 * Serve client PUSH / requests
 *
 * NOTE:
 * This code moves everything over into http_client.c's space
 */
static void server_handle_push (FTTransfer *xfer)
{
	TCPC *c = NULL;
	Chunk      *chunk;
	char       *code_str;

	if (!xfer)
		return;

	ft_transfer_unref (&c, NULL, &xfer);

	assert (xfer != NULL);

	if (!(chunk = http_server_indirect_lookup (xfer->ip, xfer->request)))
	{
		FT->DBGSOCK (FT, c, "unable to locate temporary buffer for %s",
		             xfer->request);
		http_server_send_error (c, 403);
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* attempt to minimize the chance of a future race condition by removing
	 * it from the temporary buffer now */
	http_server_indirect_remove (xfer->ip, xfer->request);

	ft_transfer_ref (c, chunk, xfer);

	/*
	 * Look at the PUSH status code and handle it naturally (that is, with
	 * the same method that the client uses).
	 */
	if (!(code_str = dataset_lookupstr (xfer->header, "X-HttpCode")))
	{
		http_server_send_error (c, 403);
		ft_transfer_status (xfer, SOURCE_CANCELLED, "No HTTP PUSH code");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/*
	 * This function handles status changes, transfer closure, etc.  If it
	 * returns FALSE, everything is taken care of for us and we can safely
	 * leave this function.
	 */
	if (!http_handle_code (xfer, ATOI (code_str)))
		return;

	/*
	 * Check if this node is going to send a size we did not request.  This
	 * is not merely a sanity check as it may be possible we have requested
	 * a size that was later fulfilled by another source.
	 */
	if (chunk->start + chunk->transmit != xfer->start)
	{
		FT->DBGSOCK (FT, c, "race condition detected (%i/%i), aborting",
		             (int)(chunk->start + chunk->transmit),(int)xfer->start);
		http_server_send_error (c, 403);
		ft_transfer_status (xfer, SOURCE_CANCELLED, "Incorrect chunk size");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* send the HTTP ack back to the client */
	xfer->code = 206;
	reply_to_client_request (xfer);
	ft_transfer_status (xfer, SOURCE_WAITING, "Received HTTP PUSH");

	/*
	 * Move the rest of the file reading over to the client
	 */
	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)get_read_file, 0);
}

static Transfer *start_upload (FTTransfer *xfer)
{
	Transfer *transfer;
	char     *alias;
	char     *user;

	/* construct the alias@ipaddress username if one has been supplied */
	alias = dataset_lookupstr (xfer->header, "X-OpenftAlias");
	user = stringf_dup ("%s%s", alias ? stringf ("%s@", alias) : "",
	                    net_ip_str (xfer->ip));

	transfer = upload_new (FT, user, xfer->hash,
	                       file_basename (xfer->open_path),
	                       xfer->open_path, xfer->start, xfer->stop,
	                       TRUE, xfer->shared);

	free (user);

	return transfer;
}

/* setup the structure for uploading.  this will be called from within
 * client space for PUSH requests as well */
int server_setup_upload (FTTransfer *xfer)
{
	Transfer   *transfer;              /* giFT structure */
	TCPC *c = NULL;

	if (!xfer)
		return FALSE;

	ft_transfer_unref (&c, NULL, &xfer);

	/* open the file that was requested before we go bringing giFT into
	 * this */
	if (!(xfer->f = ft_transfer_open_request (xfer, &xfer->code)))
		return FALSE;

	/* assign stop a value before we proceed */
	if (xfer->stop == 0)
	{
		struct stat st;
		int         ret;

		ret = file_stat (xfer->open_path, &st);

		if (!ret || st.st_size == 0)
		{
			/* 0 length files are not ok, this decision was made for reasons
			 * now beyond me */
			FT->DBGSOCK (FT, c, "cannot satisfy %s: invalid share",
			             xfer->open_path);
			return FALSE;
		}

		xfer->stop = st.st_size;
	}

	/* we can now be certain that we are handling a download request from
	 * the client.  allocate the appropriate structures to hook into giFT */
	if (!(transfer = start_upload (xfer)))
	{
		FT->DBGFN (FT, "unable to register upload with the daemon");
		return FALSE;
	}

	/* assign the circular references for passing this data along */
	ft_transfer_ref (c, transfer->chunks->data, xfer);

	/* finally, seek the file descriptor where it needs to be */
	fseek (xfer->f, xfer->start, SEEK_SET);

	return TRUE;
}

/*
 * Serve client GET / requests
 */
static void server_handle_get (FTTransfer *xfer)
{
	TCPC *c = NULL;

	if (!xfer)
		return;

	ft_transfer_unref (&c, NULL, &xfer);

	/* WARNING: this block is duplicated in http_client:client_push_request */
	if (!server_setup_upload (xfer))
	{
		/* gracefully reject */
		switch (xfer->code)
		{
		 case 503:                     /* send queue position information */
			{
				char *queue_pos   = STRDUP (ITOA (1));
				char *queue_retry = STRDUP (ITOA (30 * SECONDS));

				http_server_send (c, xfer->code,
				                  "X-QueuePosition", queue_pos,
				                  "X-QueueRetry",    queue_retry,
				                  "Content-Type",    "text/html",
				                  NULL);
				send_error_page (c, xfer->code);

				free (queue_pos);
				free (queue_retry);
			}
			break;
		 case 200:                     /* general failure */
			xfer->code = 404;
			/* fall through */
		 default:
			http_server_send_error (c, xfer->code);
			break;
		}

		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* ok, give the client the accepted header */
	reply_to_client_request (xfer);

	if (!strcmp (xfer->command, "HEAD"))
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	input_add (c->fd, c, INPUT_WRITE,
			   (InputCallback)server_upload_file, 0);
}

/*
 * Uploads the file requests
 */
void server_upload_file (int fd, input_id id, TCPC *c)
{
	Chunk        *chunk = NULL;
	FTTransfer   *xfer  = NULL;
	unsigned char buf[RW_BUFFER];
	size_t        read_len;
	size_t        size;
	int           sent_len = 0;
	off_t         remainder;

	ft_transfer_unref (&c, &chunk, &xfer);

	assert (xfer->f != NULL);

	/* number of bytes left to be uploaded by this chunk */
	if ((remainder = chunk->stop - (chunk->start + chunk->transmit)) <= 0)
	{
		/* for whatever reason this function may have been called when we have
		 * already overrun the transfer...in that case we will simply fall
		 * through to the end-of-transfer condition */
		(*xfer->callback) (chunk, NULL, 0);
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
		FT->err (FT, "unable to read from %s: %s", xfer->open_path,
		         GIFT_STRERROR ());
		ft_transfer_status (xfer, SOURCE_CANCELLED, "Local read error");
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* cap the size we will send just in case there is more available
	 * in the local file than was requested by the remote node */
	if (read_len > remainder)
		read_len = remainder;

	if ((sent_len = tcp_send (c, buf, (size_t)(MIN (read_len, remainder)))) <= 0)
	{
		ft_transfer_status (xfer, SOURCE_CANCELLED,
		                    stringf ("Error sending: %s", GIFT_NETERROR()));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* call ft_upload to report back to giFT */
	(*xfer->callback) (chunk, buf, sent_len);
}

/*****************************************************************************/

void http_server_reset (TCPC *c)
{
	/* finish all queued writes before we reset */
	tcp_flush (c, TRUE);

	/* reset the input state */
	input_remove_all (c->fd);
	input_add (c->fd, c, INPUT_READ,
			   (InputCallback)get_client_request, TIMEOUT_DEF);
}
