/*
 * $Id: gt_xfer_obj.c,v 1.25 2003/07/06 13:05:03 jasta Exp $
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

#include "plugin/share.h"

#include "src/mime.h"

#include "ft_xfer.h"

#include "ft_http_server.h"
#include "ft_http_client.h"

#include "gt_xfer.h"
#include "gt_share.h"

#include "html.h"

/*****************************************************************************/

#define PERSISTENT_HTTP

#ifdef PERSISTENT_HTTP
/* this list is very misleading...it actually means open connections which
 * do not have an immediate "use"...they in waiting for some action to need
 * to be performed but are remaining open anyway */
static List *open_connections = NULL;
#endif /* PERSISTENT_HTTP */

/*****************************************************************************/

static Dataset *ref_table = NULL;

/* log_format_time */
static const char *month_tab =
	"Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec ";

/*****************************************************************************/
/* HTTP COMMUNICATION */

/* apparently this creates the log time that goes into access.log's.  i
 * find it hard to believe ;) */
static char *log_format_time (time_t log_time)
{
	static char buf[30];
	char *p;
	struct tm   *t;
	unsigned int a;
	int time_offset = 0;

	t = gmtime (&log_time);

	p = buf + 29;

	*p-- = '\0';
	*p-- = ' ';
	*p-- = ']';
	a = abs(time_offset / 60);
	*p-- = '0' + a % 10;
	a /= 10;
	*p-- = '0' + a % 6;
	a /= 6;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = (time_offset > 0) ? '-' : '+';
	*p-- = ' ';

	a = t->tm_sec;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_min;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = t->tm_hour;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p-- = ':';
	a = 1900 + t->tm_year;
	while (a)
	{
		*p-- = '0' + a % 10;
		a /= 10;
	}
	/* p points to an unused spot */
	*p-- = '/';
	p -= 2;
	memcpy(p--, month_tab + 4 * (t->tm_mon), 3);
	*p-- = '/';
	a = t->tm_mday;
	*p-- = '0' + a % 10;
	*p-- = '0' + a / 10;
	*p = '[';

	/* should be same as returning buf */
	return p;
}

/* apache-style access.log */
static void access_log (GtTransfer *xfer)
{
	Chunk *chunk = NULL;
	FILE  *f;
	char  *referer;
	char  *user_agent;

	if (!xfer || !xfer->request)
		return;

	gt_transfer_unref (NULL, &chunk, &xfer);

	/* lookup useful HTTP headers */
	referer    = dataset_lookup (xfer->header, "Referer",    8);
	user_agent = dataset_lookup (xfer->header, "User-Agent", 11);

	if (!referer)
		referer = "-";

	if (!user_agent)
		user_agent = "-";

	/* open the log file */
	if (!(f = fopen (gift_conf_path ("access.log"), "a")))
	{
		GIFT_ERROR (("failed to open %s", gift_conf_path ("access.log")));
		return;
	}

	/* write this entry */
	fprintf (f, "%s ", net_ip_str (xfer->ip));
	fprintf (f, "- - ");
	fprintf (f, "%s", log_format_time (time (NULL)));   /* trailing \s */
	fprintf (f, "\"%s %s HTTP/1.1\" ", xfer->command, xfer->request);
	fprintf (f, "%i %lu ", xfer->code,
	         (chunk ? chunk->transmit : xfer->stop - xfer->start));
	fprintf (f, "\"%s\" ", referer);
	fprintf (f, "\"%s\"", user_agent);
	fprintf (f, "\n");

	fclose (f);
}

/*****************************************************************************/

void gt_transfer_log (GtTransfer *xfer)
{
	TCPC     *c     = NULL;
	Chunk    *chunk = NULL;

	if (!xfer || !xfer->request)
		return;

	gt_transfer_unref (&c, &chunk, &xfer);

	/* check to make sure if this is an upload only if we have enough
	 * information to do so */
	if (chunk && chunk->transfer->type != TRANSFER_UPLOAD)
		return;

	/* dont log stupid requests */
	if (!strncmp (xfer->request, "/OpenFT/", 8))
		return;

	/* uhh :) */
	if (xfer->code == 0)
		return;

	/* TODO */
	access_log (xfer);
}

/*****************************************************************************/

GtTransfer *gt_transfer_new (HTTP_Protocol *http, GtTransferType type,
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
		assert (0);

	xfer->type     = type;
	xfer->callback = cb;
	xfer->http     = http;

	/* parsed information about the source */
	xfer->ip       = ip;
	xfer->port     = port;

	xfer->start    = start;
	xfer->stop     = stop;

	xfer->shared   = TRUE;

	/* set the size of this http request */
	xfer->remaining_len = xfer->stop - xfer->start;

	return xfer;
}

/*
 * Please see below if you think you should be using this function, because
 * you should not be :)
 */
static void gt_transfer_free (GtTransfer *xfer)
{
	if (!xfer)
		return;

	free (xfer->command);
	free (xfer->request);
	free (xfer->request_path);

	if (xfer->header)
		dataset_clear (xfer->header);

#if 0
	if (xfer->share_authd && xfer->shared_authd_free)
		share_free (xfer->share_authd);
#endif

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

/*
 * Cancels a possibly active HTTP transfer
 *
 * NOTE:
 * This function may be called recursively if you don't do what giFT expects
 * of you.  This is a feature, not a bug, trust me.
 */
static void gt_transfer_cancel (Chunk *chunk, TransferType type)
{
	TCPC         *c           = NULL;
	GtTransfer   *xfer        = NULL;
	int           force_close = FALSE;

	if (!chunk)
		return;

	gt_transfer_unref (&c, &chunk, &xfer);

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

static void close_http_connection (HTTP_Protocol *http, TCPC *c,
                                   int force_close, GtTransferType type,
                                   GtSource *gt_src)
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
			GT->DBGFN (GT, "Keeping push connection to [%s]", net_peer_ip (c->fd));
			gt_push_source_add_conn (gt_src->guid, gt_src->user_ip, c);
			return;
		}

		/*
		 * This could happen if the chunk has no source url to parse
		 * at the moment. Argh, GtTransfer should always have a GtSource
		 * if xfer->type == GT_TRANSFER_DOWNLOAD.
		 */
		GT->DBGFN (GT, "Closing pushed connection to [%s]!",
		           net_peer_ip (c->fd));
		force_close = TRUE;
	}

	gt_http_connection_close (http, c, force_close, type);
}

/*
 * This function is very critical to OpenFT's transfer system.  It is called
 * anytime either the client or server HTTP implementations need to "cleanup"
 * all data associated.  This includes disconnecting the socket, unlinking
 * itself from the chunk system and registering this status with giFT, just
 * in case this is premature.  If anything is leaking or fucking up, blame
 * this :)
 */
void gt_transfer_close (GtTransfer *xfer, int force_close)
{
	TCPC          *c      = NULL;
	Chunk         *chunk  = NULL;
	GtSource      *gt_src = NULL;
	char          *conn_hdr;

	if (!xfer)
		return;

	gt_transfer_unref (&c, &chunk, &xfer);

	assert (xfer != NULL);

	/* get rid of the hacks described in gt_transfer_ref */
	if (c && c->fd >= 0)
		dataset_remove (ref_table, &c->fd, sizeof (c->fd));

	/* write to the access.log */
	gt_transfer_log (xfer);

	/* remove the xfer from the indirect src list */
	gt_push_source_remove_xfer (xfer);

	/* get this source if this was a download */
	if (xfer->type == GT_TRANSFER_DOWNLOAD && chunk && chunk->source)
		gt_src = gt_source_new (chunk->source->url);

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

	close_http_connection (xfer->http, c, force_close, xfer->type, gt_src);

	gt_source_free (gt_src);

	gt_transfer_free (xfer);
}

void gt_transfer_status (GtTransfer *xfer, SourceStatus status, char *text)
{
	TCPC       *c     = NULL;
	Chunk      *chunk = NULL;
	Protocol   *p;

	if (!xfer)
		return;

	p = xfer->http->p;

	gt_transfer_unref (&c, &chunk, &xfer);
	p->source_status (p, chunk->source, status, text);
}

/*****************************************************************************/

struct _conn_info
{
	in_addr_t  ip;
	in_port_t  port;
	int        outgoing;
};

#ifdef PERSISTENT_HTTP
static int find_open (TCPC *c, struct _conn_info *info)
{
 	if (info->port != c->port)
 		return -1;

	if (net_peer (c->fd) != info->ip)
		return 1;

	/* dont want to return server connections for clients and vice-versa */
	if (c->outgoing != info->outgoing)
		return -1;

	return 0;
}
#endif /* PERSISTENT_HTTP */

TCPC *gt_http_connection_lookup (HTTP_Protocol *http, in_addr_t ip,
                                 in_port_t port, GtTransferType type)
{
#ifdef PERSISTENT_HTTP
	List               *link;
	TCPC               *c = NULL;
	struct _conn_info   info;

	info.ip       = ip;
	info.port     = port;

	/* XXX: This wont work properly for outgoing pushed uploads, which are
	 *      outgoing. Hackily enough, though, the server code never calls
	 *      this function, so this may not matter. */
	if (type == GT_TRANSFER_DOWNLOAD)
		info.outgoing = TRUE;
	else
		info.outgoing = FALSE;

	link = list_find_custom (open_connections, &info, (CompareFunc) find_open);

	if (link)
	{
		c = link->data;

		GT->DBGFN (GT, "using previous connection to %s:%hu",
		           net_ip_str (ip), port);

		/* remove from the open list */
		open_connections = list_remove_link (open_connections, link);
		input_remove_all (c->fd);
	}

	return c;
#else /* PERSISTENT_HTTP */

	return NULL;

#endif /* PERSISTENT_HTTP */
}

/*
 * Handles outgoing HTTP connections.  This function is capable of
 * retrieving an already connected socket that was left over from a previous
 * transfer (only with PERSISTENT_HTTP).
 */
TCPC *gt_http_connection_open (HTTP_Protocol *http, in_addr_t ip,
                               in_port_t port, GtTransferType type)
{
	TCPC *c;

	if (!(c = gt_http_connection_lookup (http, ip, port, type)))
		c = tcp_open (ip, port, FALSE);

	return c;
}

/*****************************************************************************/

void gt_http_connection_close (HTTP_Protocol *http, TCPC *c, int force_close,
                               GtTransferType type)
{
	if (!c)
		return;

	/*
	 * These reset functions used to only be called if persistent http was
	 * defined, but they should be fine otherwise.
	 */
	if (type == GT_TRANSFER_DOWNLOAD)
		gt_http_client_reset (c);
	else if (type == GT_TRANSFER_UPLOAD)
		gt_http_server_reset (c);
	else
		assert (0);

#ifdef PERSISTENT_HTTP
	if (force_close)
	{
		open_connections = list_remove (open_connections, c);
		GT->dbgsock (GT, c, "force closing");
		tcp_close (c);
		return;
	}

	/* this condition will happen because the server isnt actually supposed
	 * to be using this code, but does just to cleanup the logic flow */
	if (list_find (open_connections, c))
		return;

	/* remove the data associated with this connection */
	gt_transfer_ref (c, NULL, NULL);

	/* track it */
	open_connections = list_prepend (open_connections, c);
#else /* !PERSISTENT_HTTP */
	tcp_close (c);
#endif /* PERSISTENT_HTTP */
}

/*****************************************************************************/

/*
 * Sets up circular references describe by the following graph:
 *
 *   TCPC --->  Chunk  --->  GtTransfer
 *        ^              ^----------|
 *        `-------------------------'
 *
 * Hope this helps :-P
 *
 */
void gt_transfer_ref (TCPC *c, Chunk *chunk, GtTransfer *xfer)
{
	/* unlinking the data */
	if (c && !chunk && !xfer)
		dataset_remove (ref_table, &c->fd, sizeof (c->fd));
	else if (c && !chunk)
	{
		/* this is an incomplete reference, so add it to the global for lookup
		 * later */
		if (!ref_table)
			ref_table = dataset_new (DATASET_HASH);

		dataset_insert (&ref_table, &c->fd, sizeof (c->fd), xfer, 0);
	}

	if (c)
		c->udata     = chunk;

	if (chunk)
		chunk->udata = xfer;

	if (xfer)
	{
		xfer->chunk = chunk;
		xfer->c     = c;
	}
}

/*
 * Attempt to fill in all values using the reference table described above
 * for data_ref.  This function assumes all non-NULL values at the addresses
 * supplied means that it contains valid data that is safe to read here
 */
void gt_transfer_unref (TCPC **r_c, Chunk **r_chunk,
                        GtTransfer **r_xfer)
{
	TCPC        *c     = NULL;
	Chunk       *chunk = NULL;
	GtTransfer *xfer  = NULL;
	int          i;

	/* figure out what data we have to work with */
	if (r_c)
		c = *r_c;
	if (r_chunk)
		chunk = *r_chunk;
	if (r_xfer)
		xfer = *r_xfer;

	/* lookup the special unlinked condition */
	if (c && !xfer)
		xfer = dataset_lookup (ref_table, &c->fd, sizeof (c->fd));

	/* now that we have all readable sections try to extract useful data */
	for (i = 0; i < 3; i++)
	{
		if (c)
			chunk = c->udata;

		if (chunk)
			xfer = chunk->udata;

		if (xfer)
		{
			c     = xfer->c;
			chunk = xfer->chunk;
		}

		/* we can't get anything or we've already gotten everything */
		if ((!c && !chunk && !xfer) || (c && chunk && xfer))
			break;
	}

	/* we should have absolutely everything we need now, from merely one
	 * supplied one input value.  Cool huh? */
	if (r_c)
		*r_c = c;
	if (r_chunk)
		*r_chunk = chunk;
	if (r_xfer)
		*r_xfer = xfer;
}

/*****************************************************************************/

static char *localize_request (GtTransfer *xfer, int *authorized)
{
	char          *open_path;
	char          *s_path;
	int            auth      = FALSE;
	int            need_free = FALSE;
	HTTP_Protocol *http;

	if (!xfer || !xfer->request)
		return NULL;

	/* dont call secure_path if they dont even care if it's a secure
	 * lookup */
	s_path =
	    (authorized ? file_secure_path (xfer->request) : xfer->request);

	/* NOTE: check above */
	if (authorized)
		need_free = TRUE;

	http = xfer->http;

	/* Ask the protocol for where the file is */
	if (http && http->localize_cb)
		open_path = (*http->localize_cb) (xfer, s_path, &auth);
	else
	{
		/* Hmm, what should the default localization be? */
		open_path = NULL;
	}

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
int gt_transfer_set_request (GtTransfer *xfer, char *request)
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
		/* TODO: use upload_auth_t to report queue position */

		switch (reason)
		{
		 case UPLOAD_AUTH_STALE:
			code_l = 500;              /* Internal Server Error */
			break;
		 case UPLOAD_AUTH_MAX:
		 case UPLOAD_AUTH_MAX_PERUSER:
			code_l = 503;              /* Service Unavailable */
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

static int throttle_suspend (Chunk *chunk, int s_opt, float factor)
{
	TCPC        *c    = NULL;
	GtTransfer  *xfer = NULL;

	if (!chunk)
		return FALSE;

	gt_transfer_unref (&c, &chunk, &xfer);

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

static int throttle_resume (Chunk *chunk, int s_opt, float factor)
{
	TCPC        *c    = NULL;
	GtTransfer *xfer = NULL;

	if (!chunk)
		return FALSE;

	gt_transfer_unref (&c, &chunk, &xfer);

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

	switch (transfer->type)
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

int gt_chunk_suspend (Chunk *chunk, Transfer *transfer, void *data)
{
	return throttle_suspend (chunk, throttle_sopt (transfer), 0.0);
}

int gt_chunk_resume (Chunk *chunk, Transfer *transfer, void *data)
{
	return throttle_resume (chunk, throttle_sopt (transfer), 0.0);
}
