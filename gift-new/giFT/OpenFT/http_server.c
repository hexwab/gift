/*
 * http_server.c
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
#include "file.h"

#include "xfer.h"

/*****************************************************************************/

extern Config *openft_conf;

/* do I smell a lan party? */
#define OPENFT_LOCAL_MODE \
    config_get_int (openft_conf, "local/lan_mode=0")
#define OPENFT_LOCAL_ALLOW \
    config_get_str (openft_conf, "local/hosts_allow=LOCAL")

/*****************************************************************************/

struct _indirect_source
{
	Chunk     *chunk;
	ft_uint32  ip;
	char      *request;
};

static List      *push_requests = NULL;        /* firewalled downloads */

/*****************************************************************************/

static void get_client_request (Protocol *p, Connection *c);
static void server_handle_push (FT_Transfer *xfer);
static void server_handle_get  (FT_Transfer *xfer);

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

#if 0
static char *indirect_uniq (unsigned long ip, char *request)
{
	static char uniq[PATH_MAX + 32];

	if (!ip || !request)
		return NULL;

	/* request has a leading / */
	snprintf (uniq, sizeof (uniq) - 1, "%s%s",
	          net_ip_str (ip), request);

	return uniq;
}
#endif

static struct _indirect_source *indirect_lookup (unsigned long ip, char *request)
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

void http_server_indirect_add (Chunk *chunk, unsigned long ip, char *request)
{
	struct _indirect_source *src;

	if (!chunk || !ip || !request)
		return;

	if (indirect_lookup (ip, request))
		GIFT_WARN (("adding duplicate sources"));

	if (!(src = malloc (sizeof (struct _indirect_source))))
		return;

	src->chunk   = chunk;
	src->ip      = ip;
	src->request = STRDUP (request);

	push_requests = list_append (push_requests, src);
}

void http_server_indirect_remove (unsigned long ip, char *request)
{
	struct _indirect_source *src;

	if (!(src = indirect_lookup (ip, request)))
		return;

	/* TODO -- this is seriously inefficient */
	push_requests = list_remove (push_requests, src);

	free (src->request);
	free (src);
}

Chunk *http_server_indirect_lookup (unsigned long ip, char *request)
{
	struct _indirect_source *src;

	if (!(src = indirect_lookup (ip, request)))
		return NULL;

	return src->chunk;
}

/*****************************************************************************/
/* SERVER HELPERS */

static char *lookup_http_code (int code)
{
	char *text;

	switch (code)
	{
	 case 200:  text = "OK";                    break;
	 case 206:  text = "Partial Content";       break;
	 case 403:  text = "Forbidden";             break;
	 case 404:  text = "Not Found";             break;
	 case 500:  text = "Internal Server Error"; break;
	 case 503:  text = "Service Unavailable";   break;
	 default:   text = NULL;                    break;
	}

	return text;
}

/*
 * Construct and send a server reply
 *
 * NOTE:
 * A similar function is available in http_client.c
 */
static int http_server_send (Connection *c, int code, ...)
{
	char       *key;
	char       *value;
	char       *code_text;
	static char data[RW_BUFFER];
	size_t      data_len = 0;
	va_list     args;

	/* so that we can communicate both the numerical code and the human
	 * readable string */
	if (!(code_text = lookup_http_code (code)))
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

	return net_send (c->fd, data, data_len);
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

		dataset_insert (*dataset, key, STRDUP (line));
	}
}

/* parse the Range: bytes=0-10000 format */
static void parse_client_request_range (Dataset *dataset,
										off_t *r_start, off_t *r_stop)
{
	char         *range;
	unsigned long start;
	unsigned long stop;

	if (!r_start && !r_stop)
		return;

	if (r_start)
		*r_start = 0;
	if (r_stop)
		*r_stop = 0;

	/* leave stop as 0 if we can't figure anything out yet.  This is expected
	 * to be handled separately by GET and PUSH */
	if (!(range = dataset_lookup (dataset, "Range")))
		return;

	/* WARNING: this butchers the data in the dataset! */
	string_sep (&range, "bytes=");

	if (!range)
	{
		TRACE (("error parsing Range: header"));
		return;
	}

	start = ATOUL (string_sep (&range, "-"));
	stop  = ATOUL (string_sep (&range, " "));

	if (r_start)
		*r_start = (off_t) start;
	if (r_stop)
		*r_stop = (off_t) stop;
}

/*
 * Break down the clients HTTP request
 */
static int parse_client_request (Dataset **r_dataset, char **r_command,
								 char **r_request,
								 off_t *r_start, off_t *r_stop, char *data)
{
	Dataset *dataset = NULL;
	char    *command; /* GET */
	char    *request; /* /file.tar.gz */

	if (!data)
		return FALSE;

	command = string_sep     (&data, " ");
	request = string_sep_set (&data, " \r\n");

	if (!request)
		return FALSE;

	if (r_command)
		*r_command = command;
	if (r_request)
		*r_request = request;

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
static void reply_to_client_request (FT_Transfer *xfer)
{
	Connection *c     = NULL;
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

	http_server_send (c, xfer->code,
	                  "Content-Range",  range,
	                  "Content-Length", length,
	                  "Content-Type",   xfer->content_type,
	                  "Content-MD5",    xfer->hash,
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
void http_server_incoming (Protocol *p, Connection *c)
{
	Connection *new_c;

	if (!(new_c = connection_accept (p, c, FALSE)))
		return;

    /* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (OPENFT_LOCAL_MODE)
	{
		if (!net_match_host (inet_addr (net_peer_ip (new_c->fd)),
		                     OPENFT_LOCAL_ALLOW))
		{
			connection_close (new_c);
			return;
		}
	}

	input_add (p, new_c, INPUT_READ,
			   (InputCallback) get_client_request, TRUE);
}

/*
 * Handle the client's GET or PUSH commands
 */
static void get_client_request (Protocol *p, Connection *c)
{
	FT_Transfer  *xfer;
	FT_TransferCB cb;
	Dataset      *dataset = NULL;
	char         *command = NULL;
	char         *request = NULL;
	off_t        start   = 0;
	off_t        stop    = 0;
	NBRead       *nb;

	nb = nb_active (c->fd);

	if (nb_read (nb, 0, "\r\n\r\n") <= 0)
	{
		http_connection_close (c, TRUE);
		return;
	}

	if (!nb->term)
		return;

	/* parse the client's reply and determine how we should proceed */
	if (!parse_client_request (&dataset, &command, &request,
	                           &start, &stop, nb->data))
	{
		TRACE_SOCK (("invalid http header"));
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
	xfer = ft_transfer_new (cb, inet_addr (net_peer_ip (c->fd)), 0,
	                        start, stop);

	ft_transfer_ref (c, NULL, xfer);

	/* assign all our own memory */
	xfer->command = STRDUP (command);
	xfer->header  = dataset;

	if (!ft_transfer_set_request (xfer, request))
	{
		TRACE_SOCK (("invalid request '%s'", request));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* no need for this function again */
	input_remove (c);

	if (!strcmp (xfer->command, "PUSH")) /* handle PUSH /file.tar.gz */
		server_handle_push (xfer);
	else                                 /* handle GET /file.tar.gz */
		server_handle_get (xfer);
}

/*
 * Serve client PUSH / requests
 *
 * NOTE:
 * This code moves everything over into http_client.c's space
 */
static void server_handle_push (FT_Transfer *xfer)
{
	Connection *c = NULL;
	Chunk      *chunk;
	char       *code_str;
	int         code;

	if (!xfer)
		return;

	ft_transfer_unref (&c, NULL, &xfer);

	assert (xfer != NULL);

	if (!(chunk = http_server_indirect_lookup (xfer->ip, xfer->request)))
	{
		TRACE_SOCK (("unable to locate temporary buffer for %s",
		             xfer->request));
		http_server_send (c, 403, NULL);
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* attempt to minimize the chance of a future race condition by removing
	 * it from the temporary buffer now */
	http_server_indirect_remove (xfer->ip, xfer->request);

	ft_transfer_ref (c, chunk, xfer);

	/* look at the PUSH status code to see if this wasn't actually remotely
	 * queued or something like that
	 * NOTE: we assume > 300 is queued */
	if ((code_str = dataset_lookup (xfer->header, "X-HttpCode")) &&
	    (code = ATOI (code_str)) >= 300)
	{
		/* WARNING: this code is also duplicated in
		 * http_client.c:get_server_reply */
		char *queue_pos;

		queue_pos = dataset_lookup (xfer->header, "X-QueuePosition");

		chunk->source->status      = SOURCE_QUEUED_REMOTE;
		chunk->source->status_data = STRDUP (queue_pos);

		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* check if this node is going to send a size we did not request */
	if (chunk->start + chunk->transmit != xfer->start)
	{
		TRACE_SOCK (("race condition detected (%lu/%i), aborting transfer",
		             chunk->start + chunk->transmit, (int) xfer->start));
		http_server_send (c, 403, NULL);
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* send the HTTP ack back to the client */
	xfer->code = 206;
	reply_to_client_request (xfer);

	/*
	 * Move the rest of the file reading over to the client
	 */
	input_add (openft_proto, c, INPUT_READ,
	           (InputCallback) get_read_file, FALSE);
}

/* setup the structure for uploading.  this will be called from within
 * client space for PUSH requests as well */
int server_setup_upload (FT_Transfer *xfer)
{
	Transfer   *transfer;              /* giFT structure */
	Connection *c = NULL;

	if (!xfer)
		return FALSE;

	ft_transfer_unref (&c, NULL, &xfer);

	/* open the file that was requested before we go bringing giFT into
	 * this */
	if (!(xfer->f = ft_transfer_open_request (xfer, &xfer->code)))
	{
#if 0
		TRACE_SOCK (("cannot satisfy %s (%i)", xfer->request, xfer->code));
#endif
		return FALSE;
	}

	/* assign stop a value before we proceed */
	if (xfer->stop == 0)
	{
		if (!file_exists (xfer->open_path, &xfer->stop, NULL) ||
		    xfer->stop == 0)
		{
			/* stupid bastards have a 0 length file */
			TRACE_SOCK (("cannot satisfy %s: invalid share",
			             xfer->open_path));
			return FALSE;
		}
	}

	/* we can now be certain that we are handling a download request from
	 * the client.  allocate the appropriate structures to hook into giFT */
	transfer = upload_new (openft_proto, net_ip_str (xfer->ip),
	                       xfer->hash, file_basename (xfer->request_path),
	                       xfer->request_path,
	                       (unsigned long) xfer->start,
	                       (unsigned long) xfer->stop,
	                       TRUE);

	if (!transfer)
	{
		TRACE (("unable to register upload with the daemon"));
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
static void server_handle_get (FT_Transfer *xfer)
{
	Connection *c = NULL;

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
				                  NULL);

				free (queue_pos);
				free (queue_retry);
			}
			break;
		 case 200:                     /* general failure */
			xfer->code = 404;
			/* fall through */
		 default:
			http_server_send (c, xfer->code, NULL);
			break;
		}

		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* ok, give the client the accepted header */
	reply_to_client_request (xfer);

	input_add (openft_proto, c, INPUT_WRITE,
			   (InputCallback) server_upload_file, FALSE);
}

/*
 * Uploads the file requests
 */
void server_upload_file (Protocol *p, Connection *c)
{
	Chunk       *chunk = NULL;
	FT_Transfer *xfer  = NULL;
	char         buf[RW_BUFFER];
	size_t       read_len;
	int          sent_len = 0;
	off_t        remainder;

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

	/* read as much as we can from the local file */
	read_len = fread (buf, sizeof (char), sizeof (buf), xfer->f);
	if (read_len == 0)
	{
		GIFT_ERROR (("unable to read from %s: %s", xfer->open_path,
		             GIFT_STRERROR ()));
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* cap the size we will send just in case there is more available
	 * in the local file than was requested by the remote node */
	if (read_len > remainder)
		read_len = remainder;

	if ((sent_len = net_send (c->fd, buf, MIN (read_len, remainder))) <= 0)
	{
		ft_transfer_close (xfer, TRUE);
		return;
	}

	/* call ft_upload to report back to giFT */
	(*xfer->callback) (chunk, buf, sent_len);
}

/*****************************************************************************/

void http_server_reset (Connection *c)
{
	TRACE_SOCK ((""));

	input_remove (c);
	input_add (c->protocol, c, INPUT_READ,
			   (InputCallback) get_client_request, TRUE);
}
