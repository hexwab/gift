/*
 * $Id: ft_http_client.c,v 1.40 2003/12/23 17:56:12 jasta Exp $
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

#include "ft_transfer.h"
#include "ft_http.h"

#include "ft_http_client.h"
#include "ft_http_server.h"

/*****************************************************************************/

static void push_complete_connect (int fd, input_id id, TCPC *http);
static void get_server_reply (int fd, input_id id, FTTransfer *xfer);
static void handle_server_reply (FTTransfer *xfer, input_id id,
                                 FTHttpReply *reply);
static void get_read_file (int fd, input_id id, FTTransfer *xfer);

/*****************************************************************************/

BOOL ft_http_client_push (in_addr_t ip, in_port_t port, const char *request)
{
	TCPC *http;

	if (ip == 0 || port == 0 || !request)
	{
		FT->DBGFN (FT, "invalid push request");
		return FALSE;
	}

	if (!(http = tcp_open (ip, port, FALSE)))
		return FALSE;

	/* attach the request as udata to help us in push_complete_connect */
	http->udata = STRDUP (request);

	input_add (http->fd, http, INPUT_WRITE,
	           (InputCallback)push_complete_connect, TIMEOUT_DEF);

	return TRUE;
}

static void tidy_push (TCPC *http)
{
	assert (http != NULL);

	free (http->udata);
	http->udata = NULL;

	tcp_close (http);
}

static void push_complete_connect (int fd, input_id id, TCPC *http)
{
	FTHttpRequest *req;
	char          *err = NULL;

	if (fd == -1 || id == 0)
		err = "timed out";
	else if (net_sock_error (fd))
		err = GIFT_NETERROR();

	if (err)
	{
		FT->DBGFN (FT, "outgoing push connection to %s:%hu failed: %s",
		           net_ip_str (http->host), http->port, err);
		tidy_push (http);
		return;
	}

	/* sent the PUSH request so that the remote peer understands what this
	 * connection is all about */
	req = ft_http_request_new ("PUSH", http->udata);

	free (http->udata);
	http->udata = NULL;

	if (!req)
	{
		tidy_push (http);
		return;
	}

	ft_http_request_send (req, http);

	/*
	 * Switch over the connection state as though we just received this
	 * connection incoming and we are waiting for the GET request to come
	 * in.
	 */
	input_remove (id);
	input_add (http->fd, http, INPUT_READ,
	           (InputCallback)get_client_request, TIMEOUT_DEF);
}

/*****************************************************************************/

BOOL ft_http_client_get (FTTransfer *xfer)
{
	Source           *source;
	struct ft_source *src;

	if (!xfer)
		return FALSE;

	source = ft_transfer_get_source (xfer);
	assert (source != NULL);

	src = source->udata;
	assert (src != NULL);

	if (!(xfer->http = tcp_open (src->host, src->port, FALSE)))
		return FALSE;

	input_add (xfer->http->fd, xfer, INPUT_WRITE,
	           (InputCallback)get_complete_connect, TIMEOUT_DEF);

	return TRUE;
}

/* not to be confused with get_client_request... */
static int client_send_get_request (FTTransfer *xfer)
{
	FTHttpRequest    *req;
	Chunk            *chunk;
	Source           *source;
	struct ft_source *src;
	char             *range_hdr;

	/* access the giFT download parameters */
	chunk = ft_transfer_get_chunk (xfer);
	assert (chunk != NULL);

	source = ft_transfer_get_source (xfer);
	assert (source != NULL);

	/* access the data we parsed from openft_source_add */
	src = source->udata;
	assert (src != NULL);
	assert (src->request != NULL);

	if (!(req = ft_http_request_new ("GET", src->request)))
		return -1;

	/* inform the remote node of the range we will be trying to access, even
	 * if the range is the entire file *g* */
	range_hdr = stringf ("bytes=%lu-%lu",
	                     (unsigned long)(chunk->start + chunk->transmit),
	                     (unsigned long)(chunk->stop));

	dataset_insertstr (&req->keylist, "Range", range_hdr);

	/* add our own alias so they know who is requesting this file without
	 * having access to our OpenFT node */
	if (openft->ninfo.alias)
		dataset_insertstr (&req->keylist, "X-OpenftAlias", openft->ninfo.alias);

	return ft_http_request_send (req, xfer->http);
}

/*
 * Immediately after the connection has been opened.  Please note that this
 * function is also used by the server code to move a server connection to a
 * client connection after a PUSH request has been seen.
 */
void get_complete_connect (int fd, input_id id, FTTransfer *xfer)
{
	if (fd == -1 || id == 0)
	{
		ft_transfer_stop_status (xfer, SOURCE_TIMEOUT, "Connect timeout");
		return;
	}

	if (net_sock_error (fd))
	{
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED, GIFT_NETERROR());
		return;
	}

	/*
	 * Send the GET request to the server.  This sends the full header
	 * including our range request that it will retrieve from xfer->chunk
	 * (which was set by openft_download_start when it called
	 * ft_transfer_new).
	 */
	if (client_send_get_request (xfer) < 0)
	{
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED,
		                         "Remote host had an aneurism");
		return;
	}

	/* hooray, we got passed the first round of possible errors */
	ft_transfer_status (xfer, SOURCE_WAITING, "Sent HTTP request");

	/* now wait for the server's response */
	input_remove (id);
	input_add (xfer->http->fd, xfer, INPUT_READ,
	           (InputCallback)get_server_reply, TIMEOUT_DEF);
}

static void get_server_reply (int fd, input_id id, FTTransfer *xfer)
{
	FDBuf       *buf;
	char        *data;
	size_t       data_len;
	int          n;
	FTHttpReply *reply;

	if (fd == -1 || id == 0)
	{
		ft_transfer_stop_status (xfer, SOURCE_TIMEOUT,
		                         "GET response timeout");
		return;
	}

	buf = tcp_readbuf (xfer->http);
	assert (buf != NULL);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED, "Invalid reply");
		return;
	}

	if (n > 0)
		return;

	data = (char *)fdbuf_data (buf, &data_len);

	/* look for the two trailing \n's optionally preceeded by \r characters */
	if (!(http_check_sentinel (data, data_len)))
		return;

	fdbuf_release (buf);

	if (!(reply = ft_http_reply_unserialize (data)))
	{
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED, "Malformed header");
		return;
	}

	ft_transfer_status (xfer, SOURCE_WAITING, "Received HTTP reply");

	input_remove (id);
	handle_server_reply (xfer, id, reply);
	ft_http_reply_free (reply);
}

/*
 * Process an HTTP reply from the server after our initial GET request.
 */
static void handle_server_reply (FTTransfer *xfer, input_id id,
                                 FTHttpReply *reply)
{
	Chunk   *chunk;
	Dataset *keylist;
	int      code;

	/* setup our shorthand variables */
	chunk = ft_transfer_get_chunk (xfer);
	assert (chunk != NULL);

	keylist = reply->keylist;
	code    = reply->code;

	/* successful codes all do the same thing */
	if (code >= 200 && code <= 299)
	{
		input_add (xfer->http->fd, xfer, INPUT_READ,
		           (InputCallback)get_read_file, 0);
	}
	else
	{
		/*
		 * Important note about this block:
		 *
		 * FT->source_abort (FT, ...) will call download_remove_source in
		 * giFT space which holds a different meaning there than in plugin
		 * space.  In other words, both FT->source_remove (FT, ...) and
		 * FT->download_stop (FT, ...) will be called inside of the
		 * FT->source_abort (FT, ...) call stack.  This means that any access
		 * to xfer or any variables associated within will be illegal after
		 * the call to FT->source_abort (FT, ...), as they will have been
		 * freed by our plugin upon receiving the aforementioned callbacks.
		 */
		switch (code)
		{
		 case 404:                     /* Not Found */
			/* they were once sharing this file, but no more... */
			FT->source_abort (FT, chunk->transfer, chunk->source);
			break;
		 case 503:                     /* Remotely Queued */
			{
				char *queue_pos;
				char *queue_msg = NULL;

				/* hack hack hack */
				if ((queue_pos = dataset_lookupstr (keylist, "X-ShareStatus")))
					queue_msg = stringf ("Queued (%s)", queue_pos);

				/* get our position in this users transfer queue */
				if ((queue_pos = dataset_lookupstr (keylist, "X-QueuePosition")))
					queue_msg = stringf ("Queued (position %s)", queue_pos);

				ft_transfer_stop_status (xfer, SOURCE_QUEUED_REMOTE, queue_msg);
			}
			break;
		 case 500:                     /* Hash mismatch, abort source */
			/*
			 * The remote node has reported that this file has changed since
			 * the last time we received results for it.  This more than
			 * likely indicates a hash change, in which case we should not
			 * keep this associated with this transfer.
			 */
			FT->source_abort (FT, chunk->transfer, chunk->source);
			break;
		 default:                      /* ??? */
			ft_transfer_stop_status (xfer, SOURCE_CANCELLED,
			                         stringf ("Unknown error %i", code));
			break;
		}
	}
}

static void get_read_file (int fd, input_id id, FTTransfer *xfer)
{
	char      buf[RW_BUFFER];
	size_t    size;
	int       recvret;
	Transfer *t;
	Chunk    *c;
	Source   *s;

	t = ft_transfer_get_transfer (xfer);
	c = ft_transfer_get_chunk (xfer);
	s = ft_transfer_get_source (xfer);

	assert (t != NULL);
	assert (c != NULL);
	assert (s != NULL);

	/*
	 * Ask giFT for the max size we should read.  If this returns 0, the
	 * download has been (or will) be suspended, so we should return without
	 * calling recv.
	 */
	if (!(size = download_throttle (c, sizeof (buf))))
		return;

	if ((recvret = tcp_recv (xfer->http, buf, size)) <= 0)
	{
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED,
		                         stringf ("recv error: %s", GIFT_NETERROR()));
		return;
	}

	/* give giFT the data we just read */
	FT->chunk_write (FT, t, c, s, buf, (size_t)recvret);
}
