/*
 * $Id: gt_xfer_obj.c,v 1.47 2004/06/05 03:35:53 hipnod Exp $
 *
 * acts as a gateway between giFT and the HTTP implementation
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
#include "gt_share.h"

#include "encoding/url.h"

#include "transfer/source.h"

/*****************************************************************************/

static List *upload_connections   = NULL;
static List *download_connections = NULL;

/*****************************************************************************/
/* HTTP COMMUNICATION */

static BOOL header_read_timeout (GtTransfer *xfer)
{
	gt_transfer_status (xfer, SOURCE_TIMEOUT, "Timed out");
	gt_transfer_close (xfer, TRUE);
	return FALSE;
}

GtTransfer *gt_transfer_new (GtTransferType type, Source *source,
                             in_addr_t ip, in_port_t port,
                             off_t start, off_t stop)
{
	GtTransfer    *xfer;
	GtTransferCB   cb;

	if (!(xfer = malloc (sizeof (GtTransfer))))
		return NULL;

	memset (xfer, 0, sizeof (GtTransfer));

	if (type == GT_TRANSFER_UPLOAD)
		cb = gt_upload;
	else if (type == GT_TRANSFER_DOWNLOAD)
		cb = gt_download;
	else
		abort ();

	xfer->type     = type;
	xfer->callback = cb;
	xfer->source   = source;

	/* parsed information about the source */
	xfer->ip       = ip;
	xfer->port     = port;

	xfer->start    = start;
	xfer->stop     = stop;

	xfer->shared   = TRUE;

	xfer->detach_timer  = TIMER_NONE;
	xfer->detach_msgtxt = NULL;

	/* set the size of this http request */
	xfer->remaining_len = xfer->stop - xfer->start;

	/* give this GtTransfer a maximum amount of time before cancelling */
	xfer->header_timer = timer_add (1 * MINUTES,
	                                (TimerCallback)header_read_timeout,
	                                xfer);

	return xfer;
}

static void gt_transfer_free (GtTransfer *xfer)
{
	if (!xfer)
		return;

	free (xfer->command);
	free (xfer->request);
	free (xfer->request_path);
	free (xfer->content_urns);
	free (xfer->detach_msgtxt);

	if (xfer->header)
		dataset_clear (xfer->header);

#if 0
	if (xfer->share_authd && xfer->shared_authd_free)
		share_free (xfer->share_authd);
#endif

	timer_remove (xfer->detach_timer);
	timer_remove (xfer->header_timer);

	/* uploads use these */
	free (xfer->open_path);
	free (xfer->hash);
	free (xfer->version);

	if (xfer->f)
		fclose (xfer->f);

	/* whee */
	free (xfer);
}

/*****************************************************************************/

void gt_transfer_set_tcpc (GtTransfer *xfer, TCPC *c)
{
	assert (c->udata == NULL);
	assert (xfer->c == NULL);

	c->udata = xfer;
	xfer->c = c;
}

void gt_transfer_set_chunk (GtTransfer *xfer, Chunk *chunk)
{
	assert (chunk->udata == NULL);
	assert (xfer->chunk == NULL);

	xfer->chunk = chunk;
	chunk->udata = xfer;
}

Chunk *gt_transfer_get_chunk (GtTransfer *xfer)
{
	assert (xfer->chunk != NULL);
	assert (xfer->chunk->udata == xfer);

	return xfer->chunk;
}

TCPC *gt_transfer_get_tcpc (GtTransfer *xfer)
{
	assert (xfer->c != NULL);
	assert (xfer->c->udata == xfer);

	return xfer->c;
}

/*****************************************************************************/

GtSource *gt_transfer_get_source (GtTransfer *xfer)
{
	Source *source = xfer->source;

	/* could be null for uploads */
	if (!source)
		return NULL;

	assert (source->udata != NULL);
	return source->udata;
}

/*****************************************************************************/

/*
 * giftd may change the Chunk size. This routine will reset the range of the
 * transfer we'll ask the remote end for. It should be called before we've
 * transmitted the HTTP headers that include the Range: request.
 */
void gt_transfer_set_length (GtTransfer *xfer, Chunk *chunk)
{
	TCPC   *c;
	off_t   old_start;
	off_t   old_stop;
	off_t   old_len;

	c = gt_transfer_get_tcpc (xfer);

	/* don't call me if you've already transmitted headers */
	assert (!xfer->transmitted_hdrs);

	old_start = xfer->start;
	old_stop  = xfer->stop;
	old_len   = xfer->remaining_len;

	xfer->start = chunk->start + chunk->transmit;
	xfer->stop  = chunk->stop;

	xfer->remaining_len = xfer->stop - xfer->start;

	/* i believe this is true even for push downloads... */
	assert (xfer->start == old_start);

	if (xfer->stop != old_stop)
	{
		assert (xfer->remaining_len != old_len);

		GT->DBGSOCK (GT, c, "(%s) old chunk range: [%lu,%lu) "
		                    "new range: [%lu,%lu) old len: %lu new len: %lu",
		                    xfer->request, (long)old_start,(long)old_stop,
		                    (long)xfer->start, (long)xfer->stop,
		                    (long)old_len, (long)xfer->remaining_len);
	}
}

/*****************************************************************************/

/*
 * Cancels a possibly active HTTP transfer
 *
 * NOTE:
 * This function may be called recursively if you don't do what giFT expects
 * of you.  This is a feature, not a bug, trust me.
 */
static void gt_transfer_cancel (Chunk *chunk, TransferType type)
{
	GtTransfer   *xfer;
	BOOL          force_close = FALSE;

	if (!chunk)
		return;

	xfer = chunk->udata;

	/* each time this function is called we _MUST_ uncouple the transfer
	 * and the chunk or the code to follow will eventually call this func
	 * again!! */
	if (!xfer)
		return;

	/* do not emit a callback signal */
	xfer->callback = NULL;
	gt_transfer_close (xfer, force_close);
}

/*****************************************************************************/

static void close_http_connection (TCPC *c, BOOL force_close,
                                   GtTransferType type, GtSource *gt_src)
{
	if (!c)
		return;

	/*
	 * If this is an incoming indirect download that we sent a push out
	 * for, then don't store the connection in the HTTP connection cache,
	 * store it in the separate push connection cache that uses the client
	 * id as well as the IP address to identify the node.
	 */
	if (!force_close && type == GT_TRANSFER_DOWNLOAD && !c->outgoing)
	{
		if (gt_src != NULL)
		{
			if (HTTP_DEBUG)
				GT->DBGSOCK (GT, c, "Keeping push connection");

			/* nullify the previous data on this connection */
			c->udata = NULL;

			gt_push_source_add_conn (gt_src->guid, gt_src->user_ip, c);
			return;
		}

		/*
		 * This could happen if the chunk has no source url to parse
		 * at the moment. Argh, GtTransfer should always have a GtSource
		 * if xfer->type == GT_TRANSFER_DOWNLOAD.
		 */
		if (HTTP_DEBUG)
			GT->DBGSOCK (GT, c, "Closing pushed connection! ARGH!");

		force_close = TRUE;
	}

	gt_http_connection_close (type, c, force_close);
}

/*
 * This function is very critical to OpenFT's transfer system.  It is called
 * anytime either the client or server HTTP implementations need to "cleanup"
 * all data associated.  This includes disconnecting the socket, unlinking
 * itself from the chunk system and registering this status with giFT, just
 * in case this is premature.  If anything is leaking or fucking up, blame
 * this :)
 */
void gt_transfer_close (GtTransfer *xfer, BOOL force_close)
{
	TCPC          *c;
	Chunk         *chunk;
	GtSource      *gt_src = NULL;
	char          *conn_hdr;

	if (!xfer)
		return;

	c     = xfer->c;
	chunk = xfer->chunk;

	assert (xfer != NULL);

	/* remove the xfer from the indirect src list */
	gt_push_source_remove_xfer (xfer);

	/* get this source if this was a download */
	if (xfer->type == GT_TRANSFER_DOWNLOAD && chunk && chunk->source)
		gt_src = gt_source_unserialize (chunk->source->url);

	/* if we have associated a chunk with this transfer we need to make sure
	 * we remove cleanly detach */
	if (chunk)
	{
		chunk->udata = NULL;

		/*
		 * notify the transfer callback that we have terminated this
		 * connection.  let giFT handle the rest
		 *
		 * NOTE:
		 * see gt_transfer_cancel for some warnings about this code
		 */
		if (xfer->callback)
			(*xfer->callback) (chunk, NULL, 0);

		/* WARNING: chunk is free'd in the depths of xfer->callback! */
	}

	/* HTTP/1.0 does not support persist connections or something...i dunno */
	if (!STRCASECMP (xfer->version, "HTTP/1.0"))
		force_close = TRUE;

	/* older gnutella clients send a plain "HTTP" version, that is
	 * not persistent */
	if (!STRCASECMP (xfer->version, "HTTP"))
		force_close = TRUE;

	/*
	 * We must force a socket close if there is still data waiting to
	 * be read on this transfer.
	 */
	if (!xfer->transmitted_hdrs || xfer->remaining_len != 0)
		force_close = TRUE;

	/* close the connection if "Connection: close" was supplied */
	conn_hdr = dataset_lookupstr (xfer->header, "connection");
	if (!STRCASECMP (conn_hdr, "close"))
		force_close = TRUE;

	close_http_connection (c, force_close, xfer->type, gt_src);

	gt_source_free (gt_src);

	gt_transfer_free (xfer);
}

void gt_transfer_status (GtTransfer *xfer, SourceStatus status, char *text)
{
	Chunk      *chunk;
	GtSource   *gt_src;
	char       *next_status;

	if (!xfer || !text)
		return;

	chunk = gt_transfer_get_chunk (xfer);

	GT->source_status (GT, chunk->source, status, text);

	/*
	 * HACK: Store the status message on the GtSource,
	 *       so we can reuse it sometimes.
	 */
	if (!xfer->source || !(gt_src = xfer->source->udata))
		return;

	/* allocate first so it's ok to call this function with an old value of
	 * gt_src->status_txt */
	next_status = STRDUP (text);
	free (gt_src->status_txt);
	gt_src->status_txt = next_status;
}

/*****************************************************************************/
/* PERSISTENT HTTP HANDLING */

struct conn_info
{
	in_addr_t  ip;
	in_port_t  port;
	size_t     count;
};

static int conn_cmp (TCPC *c, struct conn_info *info)
{
 	if (info->port != c->port)
 		return -1;

	if (net_peer (c->fd) != info->ip)
		return 1;

	return 0;
}

TCPC *gt_http_connection_lookup (GtTransferType type, in_addr_t ip,
                                 in_port_t port)
{
	List            *link;
	List           **connlist_ptr;
	TCPC            *c           = NULL;
	struct conn_info info;

	info.ip   = ip;
	info.port = port;

	if (type == GT_TRANSFER_DOWNLOAD)
		connlist_ptr = &download_connections;
	else
		connlist_ptr = &upload_connections;

	link = list_find_custom (*connlist_ptr, &info, (CompareFunc)conn_cmp);

	if (link)
	{
		c = link->data;

		GT->DBGFN (GT, "using previous connection to %s:%hu",
		           net_ip_str (ip), port);

		/* remove from the open list */
		*connlist_ptr = list_remove_link (*connlist_ptr, link);
		input_remove_all (c->fd);
	}

	return c;
}

/*
 * Handles outgoing HTTP connections.  This function is capable of
 * retrieving an already connected socket that was left over from a previous
 * transfer.
 */
TCPC *gt_http_connection_open (GtTransferType type, in_addr_t ip,
                               in_port_t port)
{
	TCPC *c;

	if (!(c = gt_http_connection_lookup (type, ip, port)))
		c = tcp_open (ip, port, FALSE);

	return c;
}

static BOOL count_open (TCPC *c, struct conn_info *info)
{
	if (info->ip == net_peer (c->fd))
		info->count++;

	return FALSE;
}

size_t gt_http_connection_length (GtTransferType type, in_addr_t ip)
{
	struct conn_info info;
	List            *list;

	info.ip    = ip;
	info.port  = 0;
	info.count = 0;

	assert (type == GT_TRANSFER_DOWNLOAD || type == GT_TRANSFER_UPLOAD);
	list = (type == GT_TRANSFER_DOWNLOAD ? download_connections :
	                                       upload_connections);

	list_foreach (list, (ListForeachFunc)count_open, &info);

	return info.count;
}

void gt_http_connection_close (GtTransferType type, TCPC *c, BOOL force_close)
{
	List **connlist_ptr;

	if (!c)
		return;

	switch (type)
	{
	 case GT_TRANSFER_DOWNLOAD:
		{
			gt_http_client_reset (c);
			connlist_ptr = &download_connections;
		}
		break;

	 case GT_TRANSFER_UPLOAD:
		{
			gt_http_server_reset (c);
			connlist_ptr = &upload_connections;
		}
		break;

	 default:
		abort ();
	}

	if (force_close)
	{
		*connlist_ptr = list_remove (*connlist_ptr, c);

		if (HTTP_DEBUG)
			GT->DBGSOCK (GT, c, "force closing");

		tcp_close (c);
		return;
	}

	/* remove the data associated with this connection */
	c->udata = NULL;

	/*
	 * This condition will happen because the server doesn't remove the
	 * connection from the persistent list until the connection fails.
	 */
	if (list_find (*connlist_ptr, c))
	{
		assert (type == GT_TRANSFER_UPLOAD);
		return;
	}

	/* track it */
	*connlist_ptr = list_prepend (*connlist_ptr, c);
}

/*****************************************************************************/

static char *localize_request (GtTransfer *xfer, BOOL *authorized)
{
	char          *open_path;
	char          *s_path;
	int            auth      = FALSE;
	int            need_free = FALSE;

	if (!xfer || !xfer->request)
		return NULL;

	/* dont call secure_path if they dont even care if it's a secure
	 * lookup */
	s_path =
	    (authorized ? file_secure_path (xfer->request) : xfer->request);

	if (authorized)
		need_free = TRUE;

	open_path = gt_localize_request (xfer, s_path, &auth);

	if (need_free || !open_path)
		free (s_path);

	if (authorized)
		*authorized = auth;

	/* we need a unix style path for authorization */
	return open_path;
}

/*
 * request is expected in the form:
 *   /shared/Fuck%20Me%20Hard.mpg
 */
BOOL gt_transfer_set_request (GtTransfer *xfer, char *request)
{
#if 0
	FileShare *file;
	char      *shared_path;
#endif

	free (xfer->request);
	xfer->request = NULL;

	/* lets keep this sane shall we */
	if (!request || *request != '/')
		return FALSE;

	xfer->request      = STRDUP (request);
	xfer->request_path = gt_url_decode (request);   /* storing here for opt */

	return TRUE;
}

/* attempts to open the requested file locally.
 * NOTE: this handles verification */
FILE *gt_transfer_open_request (GtTransfer *xfer, int *code)
{
	FILE       *f;
	char       *shared_path;
	char       *full_path = NULL;
	char       *host_path;
	int         auth = FALSE;
	int         code_l = 200;

	if (code)
		*code = 404; /* Not Found */

	if (!xfer || !xfer->request)
		return NULL;

	if (!(shared_path = localize_request (xfer, &auth)))
	{
		/*
		 * If we havent finished syncing shares, that may be why the
		 * request failed. If so, return 503 here.
		 */
		if (!gt_share_local_sync_is_done () && code != NULL)
			*code = 503;

		return NULL;
	}

	/* needs more work for virtual requests */
#if 0
	/* check to see if we matched a special OpenFT condition.  If we did, the
	 * localized path is in the wrong order, so convert it (just to be
	 * converted back again...it's easier to maintain this way :) */
	if (auth)
	{
		xfer->shared = FALSE;
		full_path = file_unix_path (shared_path);
	}
	else
#endif
	{
		Share        *share;
		int           reason = UPLOAD_AUTH_NOTSHARED;
		upload_auth_t cond;

		/*
		 * NOTE: the user string is not consistent with the one
		 * echoed through search results, which prefixes the IP
		 * with the GUID, if this node is firewalled.
		 */
		if ((share = GT->share_lookup (GT, SHARE_LOOKUP_HPATH, shared_path)))
		    reason = GT->upload_auth (GT, net_ip_str (xfer->ip), share, &cond);

		xfer->share_authd = share;

		switch (reason)
		{
		 case UPLOAD_AUTH_STALE:
			code_l = 500;              /* Internal Server Error */
			break;
		 case UPLOAD_AUTH_NOTSHARED:
			code_l = 404;              /* Not Found */
			break;
		 case UPLOAD_AUTH_ALLOW:
			code_l = 200;              /* OK */
			xfer->open_path_size = share->size;
			xfer->content_type = share->mime;
			full_path = STRDUP (share->path);
			break;
		 case UPLOAD_AUTH_MAX:
		 case UPLOAD_AUTH_MAX_PERUSER:
		 case UPLOAD_AUTH_HIDDEN:
		 default:
			code_l = 503;              /* Service Unavailable */
			xfer->queue_pos = cond.queue_pos;
			xfer->queue_ttl = cond.queue_ttl;
			break;
		}
	}

	if (code)
		*code = code_l;

	/* error of some kind, get out of here */
	if (code_l != 200)
		return NULL;

	/* figure out the actual filename that we should be opening */
	host_path = file_host_path (full_path);
	free (full_path);

	/* needs more work for virtual requests */
#if 0
	/* complete the rest of the data required */
	if (auth)
	{
		struct stat st;

		if (!file_stat (host_path, &st))
		{
			free (host_path);
			return NULL;
		}

		xfer->open_path_size = st.st_size;
		xfer->content_type = mime_type (host_path);
	}
#endif

	if (!(f = fopen (host_path, "rb")))
	{
		*code = 500;
		return NULL;
	}

	/* NOTE: gt_transfer_close will be responsible for freeing this now */
	xfer->open_path = host_path;

	if (code)
		*code = 200; /* OK */

	return f;
}

/*****************************************************************************/

/*
 * The callbacks here are from within the HTTP system, centralized for
 * maintainability.
 */

/* report back the progress of this download chunk */
void gt_download (Chunk *chunk, unsigned char *segment, size_t len)
{
	GT->chunk_write (GT, chunk->transfer, chunk, chunk->source,
	                 segment, len);
}

/* report back the progress of this upload chunk */
void gt_upload (Chunk *chunk, unsigned char *segment, size_t len)
{
	GT->chunk_write (GT, chunk->transfer, chunk, chunk->source,
	                 segment, len);
}

void gt_transfer_write (GtTransfer *xfer, Chunk *chunk,
                        unsigned char *segment, size_t len)
{
	/*
	 * Cap the data at the remaining size of the xfer.  Note that this is
	 * the size of the HTTP request issued, _NOT_ the chunk size, which may
	 * have been altered by giFT in splitting up chunks. I think giFT
	 * handles that case properly, but we also have to guard against
	 * remaining_len becoming less than 0. Note that p->chunk_write
	 * will cancel the transfer if remaining_len goes to 0.
	 *
	 * TODO: remaining_len is off_t, make sure this is handled right wrt
	 * negative file sizes (do big files become negative sizes?)
	 */
	if (len > xfer->remaining_len)
		len = xfer->remaining_len;

	xfer->remaining_len -= len;
	(*xfer->callback) (chunk, segment, len);
}

/*****************************************************************************/

static BOOL throttle_suspend (Chunk *chunk, int s_opt, float factor)
{
	GtTransfer  *xfer;

	if (!chunk)
		return FALSE;

	xfer = chunk->udata;

	if (!xfer || !xfer->c)
	{
		GT->DBGFN (GT, "no connection found to suspend");
		return FALSE;
	}

	input_suspend_all (xfer->c->fd);

	if (factor)
		net_sock_adj_buf (xfer->c->fd, s_opt, factor);

	return TRUE;
}

static BOOL throttle_resume (Chunk *chunk, int s_opt, float factor)
{
	GtTransfer *xfer = NULL;

	if (!chunk)
		return FALSE;

	xfer = chunk->udata;

	if (!xfer || !xfer->c)
	{
		GT->DBGFN (GT, "no connection found to resume");
		return FALSE;
	}

	input_resume_all (xfer->c->fd);

#if 0
	if (factor)
		net_sock_adj_buf (xfer->c->fd, s_opt, factor);
#endif

	return TRUE;
}

/*****************************************************************************/

void gt_download_cancel (Chunk *chunk, void *data)
{
	gt_transfer_cancel (chunk, TRANSFER_DOWNLOAD);
}

/* cancel the transfer associate with the chunk giFT gave us */
void gt_upload_cancel (Chunk *chunk, void *data)
{
	gt_transfer_cancel (chunk, TRANSFER_UPLOAD);
}

static int throttle_sopt (Transfer *transfer)
{
	int sopt = 0;

	switch (transfer_direction (transfer))
	{
	 case TRANSFER_DOWNLOAD:
		sopt = SO_RCVBUF;
		break;
	 case TRANSFER_UPLOAD:
		sopt = SO_SNDBUF;
		break;
	}

	return sopt;
}

BOOL gt_chunk_suspend (Chunk *chunk, Transfer *transfer, void *data)
{
	return throttle_suspend (chunk, throttle_sopt (transfer), 0.0);
}

BOOL gt_chunk_resume (Chunk *chunk, Transfer *transfer, void *data)
{
	return throttle_resume (chunk, throttle_sopt (transfer), 0.0);
}
