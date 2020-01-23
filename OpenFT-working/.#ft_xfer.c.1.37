/*
 * $Id: ft_xfer.c,v 1.37 2003/06/06 04:06:33 jasta Exp $
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

#include "src/mime.h"
#include "lib/file.h"

#include "ft_html.h"
#include "ft_netorg.h"

#include "md5.h"

#include "ft_xfer.h"

/*****************************************************************************/

/* #define PERSISTENT_HTTP */

#ifdef PERSISTENT_HTTP
/* this list is very misleading...it actually means open connections which
 * do not have an immediate "use"...they in waiting for some action to need
 * to be performed but are remaining open anyway */
static List *open_connections = NULL;
#endif /* PERSISTENT_HTTP */

/*****************************************************************************/

/* broken down source url for parsing */
struct _ft_source
{
	char *url;
	in_addr_t ip;
	in_port_t port;
	in_addr_t search_ip;
	in_port_t search_port;
	char *request;
};

static Dataset *ref_table = NULL;

/* log_format_time */
static const char *month_tab =
	"Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec ";

/*****************************************************************************/

/* we need to evaluate the protocol-specific source-line in the form:
 *
 * NORMAL:
 * OpenFT://node:port/requested/file
 *
 *  - or -
 *
 * FIREWALLED REQUEST:
 * OpenFT://node:my_http_port@searchnode:openft_port/requested/file
 */
static int parse_request (char *source,
                          in_addr_t *ip, in_port_t *port,
                          in_addr_t *s_ip, in_port_t *s_port,
                          char **r_request)
{
	char *ip_str, *port_str;
	char *s_ip_str, *s_port_str;
	char *ptr;

	if (!source)
		return FALSE;

	string_sep (&source, "://");       /* get rid of this useless crap */

	/* divide the string into two sides */
	if (!(ptr = string_sep (&source, "/")) || !source)
		return FALSE;

	/* pull off the left-hand operands */
	ip_str     = string_sep (&ptr, ":");
	port_str   = string_sep (&ptr, "@");
	s_ip_str   = string_sep (&ptr, ":");
	s_port_str = ptr;

	if (!ip_str || !port_str)
		return FALSE;

	/* return the values we parsed out */
	if (ip)
		*ip = net_ip (ip_str);
	if (port)
		*port = (in_port_t)ATOI (port_str);
	if (s_ip)
		*s_ip = net_ip (s_ip_str);
	if (s_port)
		*s_port = (in_port_t)ATOI (s_port_str);

	/* source is now "request/without/leading/slash", which is wrong.  We
	 * are going to efficiently (but lacking safety) rewind and put the '/'
	 * back */
	source--;
	*source = '/';

	/* give the source back */
	if (r_request)
		*r_request = source;

	/* hooray, we've got a download */
	return TRUE;
}

static void ft_source_free (struct _ft_source *src)
{
	if (!src)
		return;

	free (src->url);
	free (src);
}

/* wrapper for parse_request */
static struct _ft_source *ft_source_new (char *url)
{
	struct _ft_source *src;

	if (!(src = malloc (sizeof (struct _ft_source))))
		return NULL;

	if (!(src->url = STRDUP (url)))
	{
		free (src);
		return NULL;
	}

	if (!parse_request (src->url,
	                    &src->ip, &src->port,
	                    &src->search_ip, &src->search_port,
	                    &src->request))
	{
		ft_source_free (src);
		return NULL;
	}

	return src;
}

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
static void access_log (FTTransfer *xfer)
{
	Chunk *chunk = NULL;
	FILE  *f;
	char  *referer;
	char  *user_agent;
	char  *alias;

	if (!xfer || !xfer->request)
		return;

	ft_transfer_unref (NULL, &chunk, &xfer);

	/* lookup useful HTTP headers */
	referer    = dataset_lookup (xfer->header, "Referer",    8);
	user_agent = dataset_lookup (xfer->header, "User-Agent", 11);
	alias      = dataset_lookupstr (xfer->header, "X-OpenftAlias");


	if (!referer)
		referer = "-";

	if (!user_agent)
		user_agent = "-";

	if (!alias)
		alias = "-";

	/* open the log file */
	if (!(f = fopen (gift_conf_path ("access.log"), "a")))
	{
		FT->err (FT, "failed to open %s", gift_conf_path ("access.log"));
		return;
	}

	/* write this entry */
	fprintf (f, "%s ", net_ip_str (xfer->ip));
	fprintf (f, "%s - ", alias);
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

void ft_transfer_log (FTTransfer *xfer)
{
	TCPC  *c     = NULL;
	Chunk *chunk = NULL;

	if (!xfer || !xfer->request)
		return;

	ft_transfer_unref (&c, &chunk, &xfer);

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

FTTransfer *ft_transfer_new (FTTransferCB cb, in_addr_t ip, in_port_t port,
                             off_t start, off_t stop)
{
	FTTransfer *xfer;

	if (!(xfer = MALLOC (sizeof (FTTransfer))))
		return NULL;

	xfer->callback = cb;

	/* parsed information about the source */
	xfer->ip       = ip;
	xfer->port     = port;

	xfer->start    = start;
	xfer->stop     = stop;

	xfer->shared   = TRUE;

	return xfer;
}

/*
 * Please see below if you think you should be using this function, because
 * you should not be :)
 */
static void ft_transfer_free (FTTransfer *xfer)
{
	if (!xfer)
		return;

	free (xfer->command);
	free (xfer->request);
	free (xfer->request_path);

	if (xfer->header)
		dataset_clear (xfer->header);

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
static void ft_transfer_cancel (Chunk *chunk, TransferType type)
{
	TCPC              *c           = NULL;
	FTTransfer        *xfer        = NULL;
	int                force_close = FALSE;
	struct _ft_source *src;

	if (!chunk)
		return;

	ft_transfer_unref (&c, &chunk, &xfer);

	if (chunk->source && (src = ft_source_new (chunk->source->url)))
	{
		http_server_indirect_remove (src->ip, src->request);

#ifdef OPENFT_DEBUG
		if (http_server_indirect_lookup (src->ip, src->request))
			FT->DBGFN (FT, "fuck");
#endif /* OPENFT_DEBUG */

		ft_source_free (src);
	}

	assert (http_server_indirect_find (chunk) == NULL);

	/* each time this function is called we _MUST_ uncouple the transfer
	 * and the chunk or the code to follow will eventually call this func
	 * again!! */
	if (!xfer)
		return;

#ifdef PERSISTENT_HTTP
	/* reset connection state */
	if (type == TRANSFER_DOWNLOAD)
		http_client_reset (c);
	else if (type == TRANSFER_UPLOAD)
		http_server_reset (c);
#endif /* PERSISTENT_HTTP */

	/*
	 * We must force a socket close under the conditions that the value of
	 * stop was changed and we have no way of communicating this over HTTP,
	 * or if this chunk has not completed successfully and is being
	 * deactivated due to an error condition.
	 */
	if (chunk->stop_change || chunk->start + chunk->transmit < chunk->stop)
		force_close = TRUE;

	/* do not emit a callback signal */
	xfer->callback = NULL;
	ft_transfer_close (xfer, force_close);
}

/*****************************************************************************/

/*
 * This function is very critical to OpenFT's transfer system.  It is called
 * anytime either the client or server HTTP implementations need to "cleanup"
 * all data associated.  This includes disconnecting the socket, unlinking
 * itself from the chunk system and registering this status with giFT, just
 * in case this is premature.  If anything is leaking or fucking up, blame
 * this :)
 */
void ft_transfer_close (FTTransfer *xfer, int force_close)
{
	TCPC  *c     = NULL;
	Chunk *chunk = NULL;

	if (!xfer)
		return;

	ft_transfer_unref (&c, &chunk, &xfer);

	assert (xfer != NULL);

	/* get rid of the hacks described in ft_transfer_ref */
	if (c && c->fd >= 0)
		dataset_remove (ref_table, &c->fd, sizeof (c->fd));

	/* write to the access.log */
	ft_transfer_log (xfer);

	/* if we have associated a chunk with this transfer we need to make sure
	 * we remove cleanly detach */
	if (chunk)
	{
		chunk->data = NULL;

		/* make sure that we let the server know that a transfer with
		 * this name from this user will no longer have been requested */
		http_server_indirect_remove (xfer->ip, xfer->request);

		/*
		 * notify the transfer callback that we have terminated this
		 * connection.  let giFT handle the rest
		 *
		 * NOTE:
		 * see ft_transfer_cancel for some warnings about this code
		 */
		if (xfer->callback)
			(*xfer->callback) (chunk, NULL, 0);

		/* WARNING: chunk is free'd in the depths of xfer->callback! */
	}

	/* HTTP/1.0 does not support persist connections or something...i dunno */
	if (!STRCMP (xfer->version, "HTTP/1.0"))
		force_close = TRUE;

	ft_transfer_free (xfer);

	http_connection_close (c, force_close);
}

/*****************************************************************************/

void ft_transfer_status (FTTransfer *xfer, SourceStatus status, char *text)
{
	TCPC  *c     = NULL;
	Chunk *chunk = NULL;

	if (!xfer)
		return;

	ft_transfer_unref (&c, &chunk, &xfer);
	FT->source_status (FT, chunk->source, status, text);
}

/*****************************************************************************/

#ifdef PERSISTENT_HTTP
static int find_open (TCPC *c, in_addr_t *ip)
{
	char *c_str;
	char *ip_str;
	int   ret;

	c_str  = STRDUP (net_peer_ip (c->fd));
	ip_str = STRDUP (net_ip_str (*ip));

	ret = STRCMP (c_str, ip_str);

	free (c_str);
	free (ip_str);

	return ret;
}

static TCPC *lookup_connection (in_addr_t ip, in_port_t port)
{
	List *link;
	TCPC *c = NULL;

	link = list_find_custom (open_connections, &ip, (CompareFunc) find_open);

	if (link)
	{
		c = link->data;

		FT->DBGFN (FT, "using previous connection to %s:%hu",
		           net_ip_str (ip), port);

		/* remove from the open list */
		open_connections = list_remove_link (open_connections, link);
		input_remove (c);
	}

	return c;
}
#endif /* PERSISTENT_HTTP */

/*
 * Handles outgoing HTTP connections.  This function is capable of
 * retrieving an already connected socket that was left over from a previous
 * transfer (only with PERSISTENT_HTTP).
 */
TCPC *http_connection_open (in_addr_t ip, in_port_t port)
{
	TCPC *c;

#ifdef PERSISTENT_HTTP
	if (!(c = lookup_connection (ip, port)))
#endif /* PERSISTENT_HTTP */
		c = tcp_open (ip, port, FALSE);

	return c;
}

/*****************************************************************************/

void http_connection_close (TCPC *c, int force_close)
{
	if (!c)
		return;

#ifdef PERSISTENT_HTTP
	if (force_close)
	{
		open_connections = list_remove (open_connections, c);
		tcp_close (c);
		return;
	}

	fdbuf_release (tcp_readbuf (c));

	/* this condition will happen because the server isnt actually supposed
	 * to be using this code, but does just to cleanup the logic flow */
	if (list_find (open_connections, c))
		return;

	/* remove the data associated with this connection */
	ft_transfer_ref (c, NULL, NULL);

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
 *       TCPC  --->  Chunk  --->  FTTransfer
 *        ^            ^------------|
 *        `-------------------------'
 *
 * Hope this helps :-P
 *
 */
void ft_transfer_ref (TCPC *c, Chunk *chunk, FTTransfer *xfer)
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
		c->udata    = chunk;

	if (chunk)
		chunk->data = xfer;

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
void ft_transfer_unref (TCPC **r_c, Chunk **r_chunk,
                        FTTransfer **r_xfer)
{
	TCPC       *c     = NULL;
	Chunk      *chunk = NULL;
	FTTransfer *xfer  = NULL;
	int         i;

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
			xfer = chunk->data;

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

static char *gen_nodepage (in_addr_t ip, char *sec)
{
	char *ret;
	char *npath;

	npath = gift_conf_path ("OpenFT/np-%s.html", sec);

	if (!(ret = html_page_index (ip, npath, sec)))
		return "";

	return ret;
}

static char *localize_request (FTTransfer *xfer, int *authorized)
{
	static char open_path[PATH_MAX];
	char       *s_path;
	int         auth = FALSE;

	if (!xfer || !xfer->request)
		return NULL;

	/* dont call secure_path if they dont even care if it's a secure
	 * lookup */
	s_path =
	    (authorized ? file_secure_path (xfer->request) : xfer->request);

	/* handle the various OpenFT-specific request forms */
	if (!strcmp (s_path, "/") || !strcmp (s_path, "/index.html") ||
	    !strcmp (s_path, "/nodes.html"))
	{
		/* => /home/jasta/.giFT/OpenFT/np-nodes.html */
		snprintf (open_path, sizeof (open_path) - 1, "%s",
				  gen_nodepage (xfer->ip, "nodes"));
		auth = TRUE;
	}
	else if (!strcmp (s_path, "/shares.html"))
	{
		/* => /home/jasta/.giFT/OpenFT/np-shares.html */
		snprintf (open_path, sizeof (open_path) - 1, "%s",
				  gen_nodepage (xfer->ip, "shares"));
		auth = TRUE;
	}
	else if (!strcmp (s_path, "/robots.txt"))
	{
		/* => /usr/local/share/giFT/OpenFT/robots.txt */
		snprintf (open_path, sizeof (open_path) - 1, "%s/%s%s",
		          platform_data_dir (), "OpenFT", s_path);
		auth = TRUE;
	}
	else if (!strcmp (s_path, "/OpenFT/shares.gz"))
	{
		/* => /home/jasta/.giFT/OpenFT/shares */
		snprintf (open_path, sizeof (open_path) - 1, "%s",
				  gift_conf_path (s_path + 1));
		auth = TRUE;
	}
	else if (!strncmp (s_path, "/OpenFT/", 8))
	{
		/* /OpenFT/top.jpg => /usr/local/share/giFT/OpenFT/top.jpg */
		snprintf (open_path, sizeof (open_path) - 1, "%s%s",
		          platform_data_dir (), s_path);
		auth = TRUE;
	}
	else
	{
		char *decoded = url_decode (xfer->request);

		/* /shared/windows%20users%20love%20spaces.jpg =>
		 * /shared/windows users love spaces.jpg */
		snprintf (open_path, sizeof (open_path) - 1, "%s", decoded);
		free (decoded);

		auth = FALSE;
	}

	/* NOTE: check above */
	if (authorized)
	{
		*authorized = auth;
		free (s_path);
	}

	/* we need a unix style path for authorization */
	return open_path;
}

/*
 * request is expected in the form:
 *   /shared/Fuck%20Me%20Hard.mpg
 */
int ft_transfer_set_request (FTTransfer *xfer, char *request)
{
	FileShare *file;
	char      *shared_path;

	free (xfer->request);
	xfer->request = NULL;

	/* lets keep this sane shall we */
	if (!request || *request != '/')
		return FALSE;

	xfer->request      = STRDUP (request);
	xfer->request_path = url_decode (request);   /* storing here for opt */

	/* try to fill in the hash */
	if (!(shared_path = localize_request (xfer, NULL)))
		return TRUE;

	/* look to see if this file is explicitly shared, if it is, let's
	 * extract it's MD5 sum so that we can register to giFT with it */
	if ((file = share_find_file (shared_path)) && SHARE_DATA(file))
	{
		Hash *hash = share_get_hash (file, "MD5");

		if (hash)
		{
			assert (hash->len == 16);
			xfer->hash = md5_string (hash->data);
		}
	}

	return TRUE;
}

/* attempts to open the requested file locally.
 * NOTE: this handles verification */
FILE *ft_transfer_open_request (FTTransfer *xfer, int *code)
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
		return NULL;

	/* check to see if we matched a special OpenFT condition.  If we did, the
	 * localized path is in the wrong order, so convert it (just to be
	 * converted back again...it's easier to maintain this way :) */
	if (auth)
	{
		xfer->shared = FALSE;
		full_path = file_unix_path (shared_path);
	}
	else
	{
		FileShare *file;
		AuthReason reason;

		/* we didn't catch a special OpenFT condition, so check giFT's shares
		 * table for a match */
		file = upload_auth (net_ip_str (xfer->ip), shared_path, &reason);

		switch (reason)
		{
		 case AUTH_STALE:
			code_l = 500;              /* Internal Server Error */
			break;
		 case AUTH_MAX:
		 case AUTH_MAX_PERUSER:
			code_l = 503;              /* Service Unavailable */
			break;
		 case AUTH_INVALID:
			code_l = 404;              /* Not Found */
			break;
		 case AUTH_ACCEPTED:
			code_l = 200;              /* OK */
			xfer->open_path_size = file->size;
			xfer->content_type = file->mime;
			full_path = STRDUP(SHARE_DATA(file)->path);
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
		xfer->content_type   = mime_type (host_path);
	}

	if (!(f = fopen (host_path, "rb")))
	{
		*code = 500;
		return NULL;
	}

	/* NOTE: ft_transfer_close will be responsible for freeing this now */
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
void ft_download (Chunk *chunk, unsigned char *segment, size_t len)
{
	FT->chunk_write (FT, chunk->transfer, chunk, chunk->source,
					 segment, len);
}

/* report back the progress of this upload chunk */
void ft_upload (Chunk *chunk, unsigned char *segment, size_t len)
{
	FT->chunk_write (FT, chunk->transfer, chunk, chunk->source,
					 segment, len);
}

/*****************************************************************************/

static int send_push (char *request,
                      off_t start, off_t stop,
                      in_addr_t ip, in_port_t port,
                      in_addr_t search_ip, in_port_t search_port)
{
	FTNode   *route;
	FTPacket *pkt;
	int       ret;

	/* if we are our own search node in this case, don't involve the socket
	 * routing, just send directly to the user we want (who must be one of
	 * our children if we got this far) */
	if (search_ip == 0)
	{
		route = ft_netorg_lookup (ip);

		/* skip the forward request and send directly to the user who we
		 * are downloading from */
		if (!(pkt = ft_packet_new (FT_PUSH_REQUEST, 0)))
			return FALSE;

		/* ip=0, port=0 is a special exception that will cause the node
		 * receiving this packet to assume we mean these values to be our
		 * own ip and port */
		ft_packet_put_ip     (pkt, 0);
		ft_packet_put_uint16 (pkt, 0, TRUE);
	}
	else
	{
		/* attempt to establish a direct connection with this node so
		 * that we can ask for our push request to be handled */
		if ((route = ft_node_register (search_ip)))
		{
			ft_node_set_port (route, search_port);
			ft_session_connect (route);
		}

		if (!(pkt = ft_packet_new (FT_PUSH_FORWARD, 0)))
			return FALSE;

		/* tell the parent to forward the message to the follow user */
		ft_packet_put_ip (pkt, ip);
	}

	/* add the actual file request we plan to make */
	ft_packet_put_str (pkt, request);

	/* add the chunk data we want */
	ft_packet_put_uint32 (pkt, (uint32_t)start, TRUE);
	ft_packet_put_uint32 (pkt, (uint32_t)stop, TRUE);

	/* abuse the same logic above to actually deliver the packet */
	if (search_ip == 0)
		ret = ft_packet_send (FT_CONN(route), pkt);
	else
		ret = ft_packet_sendto (search_ip, pkt);

	/* ret < 0 is the only error condition for ft_packet_send and friends */
	return (ret >= 0);
}

int openft_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source)
{
	FTTransfer    *xfer;
	char          *url;
	uint32_t       ip;
	uint16_t       port;
	uint32_t       search_ip;
	uint16_t       search_port;
	char          *request;
	off_t          start;
	off_t          stop;

	if (!(url = STRDUP (source->url)))
		return FALSE;

	start = chunk->start + chunk->transmit;
	stop  = chunk->stop;

	if (!parse_request (url, &ip, &port, &search_ip, &search_port, &request))
	{
		free (url);
		return FALSE;
	}

	/* firewalled download, relay the request to the search node
	 * NOTE: lookup_connection is called to check if we already have an
	 * active (persisting) connection to this firewalled user.  If we do then
	 * we can fall through and use http_client_get directly as it will
	 * succeed in locating the active connection in the same way */
	if (search_port
#ifdef PERSISTENT_HTTP
		&& !lookup_connection (ip, port)
#endif /* PERSISTENT_HTTP */
		)
	{
		int ret;

		FT->source_status (FT, chunk->source,
		                   SOURCE_WAITING, "Awaiting connection");

		ret = send_push (request, start, stop,
		                 ip, port, search_ip, search_port);

		if (ret)
			http_server_indirect_add (chunk, ip, request);

		free (url);

		return ret;
	}

	/* create the transfer structure */
	if (!(xfer = ft_transfer_new (ft_download, ip, port, start, stop)))
	{
		free (url);
		return FALSE;
	}

	if (!ft_transfer_set_request (xfer, request))
	{
		FT->DBGFN (FT, "UI made an invalid request for '%s'", request);;
		free (url);
		ft_transfer_close (xfer, TRUE);
		return FALSE;
	}

	free (url);
	http_client_get (chunk, xfer);

	return TRUE;
}

void openft_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source, int complete)
{
	assert (chunk != NULL);

	/* TODO: use complete */
	ft_transfer_cancel (chunk, TRANSFER_DOWNLOAD);
}

/*****************************************************************************/

void openft_upload_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source)
{
	ft_transfer_cancel (chunk, TRANSFER_UPLOAD);
}

static int submit_uploads (FTNode *node, unsigned long *avail)
{
	ft_packet_sendva (FT_CONN(node), FT_MODSHARE_REQUEST, 0, "hl",
	                  FALSE, *avail);
	return TRUE;
}

void openft_upload_avail (Protocol *p, unsigned long avail)
{
	ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(submit_uploads), &avail);
}

/*****************************************************************************/

static int throttle_suspend (Chunk *chunk, int s_opt, float factor)
{
	TCPC       *c    = NULL;
	FTTransfer *xfer = NULL;

	if (!chunk)
		return FALSE;

	ft_transfer_unref (&c, &chunk, &xfer);

	if (!xfer || !xfer->c)
	{
		FT->DBGFN (FT, "no connection found to suspend");
		return FALSE;
	}

	input_suspend_all (xfer->c->fd);

	if (factor)
		net_sock_adj_buf (xfer->c->fd, s_opt, factor);

	return TRUE;
}

static int throttle_resume (Chunk *chunk, int s_opt, float factor)
{
	TCPC       *c    = NULL;
	FTTransfer *xfer = NULL;

	if (!chunk)
		return FALSE;

	ft_transfer_unref (&c, &chunk, &xfer);

	if (!xfer || !xfer->c)
	{
		FT->DBGFN (FT, "no connection found to resume");
		return FALSE;
	}

	input_resume_all (xfer->c->fd);

#if 0
	if (factor)
		net_sock_adj_buf (xfer->c->fd, s_opt, factor);
#endif

	return TRUE;
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

int openft_chunk_suspend (Protocol *p, Transfer *transfer, Chunk *chunk,
                          Source *source)
{
	return throttle_suspend (chunk, throttle_sopt (transfer), 0.0);
}

int openft_chunk_resume (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source)
{
	return throttle_resume (chunk, throttle_sopt (transfer), 0.0);
}

/*****************************************************************************/

int openft_source_remove (Protocol *p, Transfer *transfer, Source *source)
{
	uint32_t      ip;
	char         *request;
	char         *url;

	if (!(url = STRDUP (source->url)))
		return FALSE;

	if (!parse_request (url, &ip, NULL, NULL, NULL, &request))
	{
		free (url);
		return FALSE;
	}

	http_server_indirect_remove (ip, request);
	free (url);

	return TRUE;
}

/*****************************************************************************/

int openft_source_cmp (Protocol *p, Source *a, Source *b)
{
	struct _ft_source *ft_a = NULL;
	struct _ft_source *ft_b = NULL;
	int ret = 0;

	if (!(ft_a = ft_source_new (a->url)) ||
	    !(ft_b = ft_source_new (b->url)))
	{
		ft_source_free (ft_a);
		ft_source_free (ft_b);
		return -1;
	}

	if (ft_a->ip > ft_b->ip)
		ret =  1;
	else if (ft_a->ip < ft_b->ip)
		ret = -1;

	/* TODO -- check the hash */

	if (ret == 0)
		ret = strcmp (ft_a->request, ft_b->request);

	ft_source_free (ft_a);
	ft_source_free (ft_b);

	return ret;
}

int openft_user_cmp (Protocol *p, char *a, char *b)
{
	char *ptr;

	if ((ptr = strchr (a, '@')))
		a = ptr + 1;

	if ((ptr = strchr (b, '@')))
		b = ptr + 1;

	return strcmp (a, b);
}
