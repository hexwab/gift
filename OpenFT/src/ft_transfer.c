/*
 * $Id: ft_transfer.c,v 1.16 2005/01/02 21:44:40 mkern Exp $
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

#include "ft_netorg.h"

#include "ft_transfer.h"
#include "ft_http_client.h"

/*****************************************************************************/

/*
 * Manage all push requests so that when requests come in we can locate the
 * constructed FTTransfer object originally requested to download the file.
 * Please note that the key here is actually just a copy of the struct
 * ft_source parsed from the original request.  Instead of using that key for
 * lookup, we iterate through the dataset looking for a match on ip (of the
 * user who is uploading and will send us the PUSH request) and actual
 * request made.
 */
static Dataset *pushes = NULL;

/*
 * Manage all downloads so that any other system may access them through
 * ft_downloads_access.  This is actually just a hack for proto/ft_push.c to
 * drop firewalled sources when the user is no longer a child at the named
 * search node.
 */
static List *downloads = NULL;

/*
 * Cached available from openft_upload_avail.  Accessed by ft_upload_avail.
 */
static unsigned long last_avail = 0;

/*****************************************************************************/

static BOOL init_source (struct ft_source *src)
{
	memset (src, 0, sizeof (struct ft_source));

	return TRUE;
}

static void finish_source (struct ft_source *src)
{
	free (src->request);
}

static struct ft_source *new_source (void)
{
	struct ft_source *src;

	if (!(src = MALLOC (sizeof (struct ft_source))))
		return NULL;

	return src;
}

static void free_source (struct ft_source *src)
{
	if (!src)
		return;

	finish_source (src);
	free (src);
}

/*****************************************************************************/

/*
 * url comes in two possible forms:
 *
 * Direct Connection:
 * OpenFT://ip:http_port/requested/file?md5=...
 *
 * Indirection Connection:
 * OpenFT://ip:http_port@search_ip:search_openft_port/requested/file?md5=...
 *
 * Please note that the "?md5=..." portion of the URL is optional and is only
 * used to optimize away a dataset_find call in the daemon.
 *
 * Also note that I lied, "?md5=..." isnt implemented at all yet.
 */
static BOOL decompose_source (struct ft_source *src, char *url)
{
	char *lefthand, *righthand;
	char *proto;
	char *ip_str, *port_str;
	char *sip_str, *sport_str;

	assert (url != NULL);

	/* move beyond the OpenFT:// prefix */
	proto = string_sep (&url, "://");

	/* this can happen if the URL is missing the slashes (the
	 * proto name and colon are guaranteed) */
	if (strcmp (proto, "OpenFT") != 0)
		return FALSE;

	/* divide the string in to two parts, one pointing to
	 * "ip:http_port" and the other to "requested/file?md5=..." */
	lefthand = string_sep (&url, "/");
	righthand = url;

	if (!lefthand || !righthand)
		return FALSE;

	ip_str   = string_sep (&lefthand, ":");
	port_str = string_sep (&lefthand, "@");

	/* these two are required */
	if (!ip_str || !port_str)
		return FALSE;

	/* if no '@' was found from the previous call, lefthand will be NULL and
	 * will thusly set both of these to NULL */
	sip_str   = string_sep (&lefthand, ":");
	sport_str = lefthand;

	/* finally fill in the src structure */
	src->host        = (in_addr_t)(net_ip       (ip_str));
	src->port        = (in_port_t)(gift_strtoul (port_str));
	src->search_host = (in_addr_t)(net_ip       (sip_str));
	src->search_port = (in_port_t)(gift_strtoul (sport_str));

	/* get the request by rewinding the righthand "requested/file?md5=..."
	 * and setting the original '/' back on the buffer */
	*(--righthand) = '/';
	src->request = STRDUP (righthand);

	return TRUE;
}

static BOOL parse_source (struct ft_source *src, const char *url)
{
	char *url_dup;
	BOOL  ret = FALSE;

	assert (src != NULL);
	assert (url != NULL);

	/* initialize the source and copy the url in */
	if (!(init_source (src)))
		return FALSE;

	if (!(url_dup = STRDUP (url)))
	{
		finish_source (src);
		return FALSE;
	}

	/* do all the real work :) */
	ret = decompose_source (src, url_dup);
	free (url_dup);

	if (!ret)
		finish_source (src);

	return ret;
}

/*****************************************************************************/

static void add_download (FTTransfer *xfer)
{
	downloads = list_prepend (downloads, xfer);
}

static void remove_download (FTTransfer *xfer)
{
	downloads = list_remove (downloads, xfer);
}

Array *ft_downloads_access (void)
{
	Array *array = NULL;
	List  *ptr;

	/*
	 * If you're wondering why we are just basically copying this into an
	 * Array, it is because the only usage of this routine will need a locked
	 * copy of the downloads list as FT->download_stop will be called from
	 * within its loop.  Oh, and I'm lazy :)
	 */
	for (ptr = downloads; ptr; ptr = list_next (ptr))
		array_push (&array, ptr->data);

	return array;
}

/*****************************************************************************/

FTTransfer *ft_transfer_new (TransferType dir,
                             Transfer *t, Chunk *c, Source *s)
{
	FTTransfer *xfer;

	assert (dir == TRANSFER_DOWNLOAD || dir == TRANSFER_UPLOAD);

	if (!(xfer = MALLOC (sizeof (FTTransfer))))
		return NULL;

	/* hmm...why do we make a copy of this? */
	xfer->dir = dir;

	if (t)
		assert (transfer_direction (t) == dir);

	/* assign the giFT transfer parameters */
	ft_transfer_set_transfer (xfer, t);
	ft_transfer_set_chunk (xfer, c);
	ft_transfer_set_source (xfer, s);

	if (dir == TRANSFER_DOWNLOAD)
	{
		/* maintain an index of all downloads for ft_download_access_by_ip,
		 * which will be used for error handling in proto/ft_push.c */
		add_download (xfer);
	}

	return xfer;
}

static void ft_transfer_free (FTTransfer *xfer)
{
	if (!xfer)
		return;

	/* see ft_transfer_new or add_download for more information on why
	 * we need this crappy little hack */
	if (xfer->dir == TRANSFER_DOWNLOAD)
		remove_download (xfer);

	file_close (xfer->f);

	tcp_close (xfer->http);

#if 0
	/* TODO: these don't need to linger on the transfer object, their
	 * usefulness is limited to the stages of setting up the connection */
	ft_http_request_free (xfer->http_request);
	ft_http_reply_free (xfer->http_reply);
#endif

	free (xfer);
}

void ft_transfer_stop (FTTransfer *xfer)
{
	Transfer *t;
	Chunk    *c;
	Source   *s;

	if (!xfer)
		return;

	t = ft_transfer_get_transfer (xfer);
	assert (t != NULL);

	c = ft_transfer_get_chunk (xfer);
	assert (c != NULL);

	s = ft_transfer_get_source (xfer);
	assert (s != NULL);

	/* giFT is not allowed to disassociate the chunk and the source it
	 * originally gave us */
	assert (c->source == s);
	assert (s->chunk == c);

	/*
	 * When we call chunk_write with NULL and 0 as the final parameters, giFT
	 * will attempt to relocate this source, deactivating it first.
	 * Deactivation will cause p->download_stop to be raised and we can free
	 * the structure as though the user had requested its destruction (this
	 * function implies that internally OpenFT requested its destruction).
	 */
	FT->chunk_write (FT, t, c, s, NULL, 0);
}

/*****************************************************************************/

void ft_transfer_set_transfer (FTTransfer *xfer, Transfer *transfer)
{
	if (!xfer)
		return;

	assert (xfer->transfer == NULL);
	xfer->transfer = transfer;
}

Transfer *ft_transfer_get_transfer (FTTransfer *xfer)
{
	if (!xfer)
		return NULL;

	return xfer->transfer;
}

void ft_transfer_set_chunk (FTTransfer *xfer, Chunk *chunk)
{
	if (!xfer)
		return;

	assert (xfer->chunk == NULL);
	xfer->chunk = chunk;
}

Chunk *ft_transfer_get_chunk (FTTransfer *xfer)
{
	if (!xfer)
		return NULL;

	return xfer->chunk;
}

void ft_transfer_set_source (FTTransfer *xfer, Source *source)
{
	if (!xfer)
		return;

	assert (xfer->source == NULL);
	xfer->source = source;
}

Source *ft_transfer_get_source (FTTransfer *xfer)
{
	if (!xfer)
		return NULL;

	return xfer->source;
}

void ft_transfer_set_fhandle (FTTransfer *xfer, FILE *f)
{
	if (!xfer)
		return;

	assert (xfer->f == NULL);
	xfer->f = f;
}

FILE *ft_transfer_get_fhandle (FTTransfer *xfer)
{
	if (!xfer)
		return NULL;

	return xfer->f;
}

/*****************************************************************************/

void ft_transfer_status (FTTransfer *xfer, SourceStatus status,
                         const char *text)
{
	Source *source;

	if (!xfer)
		return;

	source = ft_transfer_get_source (xfer);
	assert (source != NULL);

	FT->source_status (FT, source, status, text);
}

/* simple short-hand that's used a lot :) */
void ft_transfer_stop_status (FTTransfer *xfer, SourceStatus status,
                              const char *text)
{
	ft_transfer_status (xfer, status, text);
	ft_transfer_stop (xfer);
}

/*****************************************************************************/

static BOOL push_send_request (struct ft_source *src)
{
	FTNode   *route;
	FTPacket *pkt;
	int       ret;

	/* if we are our own search node in this case, don't involve the socket
	 * routing, just send directly to the user we want (who must be one of
	 * our children if we got this far) */
	if (src->search_host == 0)
	{
		route = ft_netorg_lookup (src->host);

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
		if ((route = ft_node_register (src->search_host)))
		{
			ft_node_set_port (route, src->search_port);
			ft_session_connect (route, FT_PURPOSE_PUSH_FWD);
		}

		if (!(pkt = ft_packet_new (FT_PUSH_FWD_REQUEST, 0)))
			return FALSE;

		/* tell the parent to forward the message to the follow user */
		ft_packet_put_ip (pkt, src->host);
	}

	/* add the actual file request we plan to make */
	ft_packet_put_str (pkt, src->request);

	/* abuse the same logic above to actually deliver the packet */
	if (src->search_host == 0)
		ret = ft_packet_send (FT_CONN(route), pkt);
	else
		ret = ft_packet_sendto (src->search_host, pkt);

	/* ret < 0 is the only error condition for ft_packet_send and friends */
	return (ret >= 0);
}

static void push_add (struct ft_source *src, FTTransfer *xfer)
{
	/* since we dont care about lookup time, dont waste the space with a
	 * hash table */
	if (!pushes)
		pushes = dataset_new (DATASET_LIST);

	/* laziness alert: we are just going to use a dataset to store the key
	 * and value for us, but we will only lookup using dataset_find */
	xfer->push_node =
	    dataset_insert (&pushes, src, sizeof (struct ft_source), xfer, 0);
}

static void push_remove (FTTransfer *xfer)
{
	if (xfer->push_node)
	{
		dataset_remove_node (pushes, xfer->push_node);
		xfer->push_node = NULL;
	}
}

static BOOL push_find_xfer (ds_data_t *key, ds_data_t *value, void **args)
{
	in_addr_t        *host    = args[0];
	char             *request = args[1];
	struct ft_source *src     = key->data;

	if (src->host != *host)
		return FALSE;

	return (strcmp (src->request, request) == 0);
}

FTTransfer *push_access (in_addr_t host, const char *request)
{
	FTTransfer  *xfer;
	DatasetNode *node;
	void *args[] = { &host, (void *)request };

	/* first lookup */
	if (!(node = dataset_find_node (pushes, DS_FIND(push_find_xfer), args)))
		return NULL;

	/* copy the pointer, but we dont need to copy all of the memory because
	 * the dataset was not being asked to hold an internal copy */
	xfer = node->value->data;
	assert (xfer->push_node == node);

	/* once the connection comes back we no longer need this temporary data */
	push_remove (xfer);

	return xfer;
}

/*****************************************************************************/

static void set_ft_transfer (Chunk *c, FTTransfer *xfer)
{
	/*
	 * There is a bug somewhere in giFT/src/download.c where it is possible
	 * that two p->download_start calls would be made without first calling
	 * p->download_stop (which we would set c->udata to NULL in).  This bug
	 * is extremely difficult to track down, and so we have decreased the
	 * severity of this problem for the end user.
	 *
	 * This bug is likely to be accidentally fixed when we rewrite a large
	 * portion of the transfer subsystem for giFT 0.12.x.
	 */
	if (c->udata != NULL)
		FT->err (FT, "BUG: %p->udata=%p", c, c->udata);

	c->udata = xfer;
}

static FTTransfer *get_ft_transfer (Chunk *c)
{
	FTTransfer *xfer;

	if ((xfer = c->udata))
		assert (xfer->chunk == c);
	else
		FT->err (FT, "no OpenFT transfer associated with Chunk %p", c);

	return xfer;
}

/*****************************************************************************/

BOOL openft_download_start (Protocol *p, Transfer *t, Chunk *c, Source *s)
{
	FTTransfer *xfer;
	struct ft_source *src;
	BOOL ret;

	/* OpenFT is much better at catching giFT bugs than any other plugin :) */
	assert (t != NULL);
	assert (c != NULL);
	assert (s != NULL);
	assert (c->source == s);
	assert (s->chunk == c);

	/*
	 * Code commented out and moved to ft_http_client.c:client_get_request.
	 * Please please please make sure you add c->transmit!
	 */
#if 0
	/* we are writing this here to emphasize the importance of these
	 * operations, but we really just pass to http_client_get */
	start = c->start + c->transmit;    /* very important to add transmit */
	stop  = c->stop;
#endif

	if (!(src = s->udata))
	{
		FT->DBGFN (FT, "no preparsed source data found");
		return FALSE;
	}

	if (src->host == 0)
		return FALSE;

	if (src->search_host == 0 || src->search_port == 0)
	{
		if (src->port == 0)
			return FALSE;
	}

	if (!(xfer = ft_transfer_new (TRANSFER_DOWNLOAD, t, c, s)))
		return FALSE;

	/* special indirection connection form of the url string, indicating to
	 * us that we should ask for the user to connect to us through the search
	 * node */
	if (src->search_port == 0)
		FT->source_status (FT, s, SOURCE_WAITING, "Connecting");
	else
	{
		FT->source_status (FT, s, SOURCE_WAITING, "Awaiting connection");

		/* use the OpenFT protocol to get a request coming back to us, then
		 * setup a temporary table to lookup when the PUSH request comes
		 * back with push_add */
		if (!(ret = push_send_request (src)))
		{
			/* TODO: p->source_abort? */
			FT->source_status (FT, s, SOURCE_CANCELLED, "No PUSH route");
			ft_transfer_free (xfer);
			return FALSE;
		}

		push_add (src, xfer);
	}

	/* lets try to make a direct connection for ourselves... */
	if (src->search_port == 0)
	{
		if (!(ft_http_client_get (xfer)))
		{
			FT->DBGFN (FT, "sigh, unable to connect");
			ft_transfer_free (xfer);
			return FALSE;
		}
	}

	set_ft_transfer (c, xfer);

	return TRUE;
}

void openft_download_stop (Protocol *p, Transfer *t, Chunk *c, Source *s,
                           BOOL complete)
{
	FTTransfer *xfer;

	if (!(xfer = get_ft_transfer (c)))
	{
		assert (c->udata == NULL);
		return;
	}

	push_remove (xfer);
	ft_transfer_free (xfer);

	/* cant use set_ft_transfer, it asserts c->udata's current value */
	c->udata = NULL;
}

/*****************************************************************************/

void openft_upload_stop (Protocol *p, Transfer *t, Chunk *c, Source *s)
{
	FTTransfer *xfer;

	/* set_ft_transfer is partially implemented in
	 * ft_http_server.c:get_openft_transfer */
	if (!(xfer = get_ft_transfer (c)))
	{
		assert (c->udata == NULL);
		return;
	}

	ft_transfer_free (xfer);
	c->udata = NULL;
}

/*****************************************************************************/

static TCPC *get_connection (Transfer *t, Chunk *c, Source *s)
{
	FTTransfer *xfer;

	xfer = get_ft_transfer (c);
	assert (xfer != NULL);

	if (!xfer->http)
	{
		FT->DBGFN (FT, "no connection found for throttling...");
		return NULL;
	}

	return xfer->http;
}

BOOL openft_chunk_suspend (Protocol *p, Transfer *t, Chunk *c, Source *s)
{
	TCPC *http;

	if (!(http = get_connection (t, c, s)))
		return FALSE;

	input_suspend_all (http->fd);

	return TRUE;
}

BOOL openft_chunk_resume (Protocol *p, Transfer *t, Chunk *c, Source *s)
{
	TCPC *http;

	if (!(http = get_connection (t, c, s)))
		return FALSE;

	input_resume_all (http->fd);

	return TRUE;
}

/*****************************************************************************/

BOOL openft_source_add (Protocol *p, Transfer *t, Source *s)
{
	struct ft_source *src;

	assert (s != NULL);
	assert (s->url != NULL);
	assert (s->udata == NULL);

	if (!(src = new_source ()))
		return FALSE;

	if (!(parse_source (src, s->url)))
	{
		FT->DBGFN (FT, "failed to parse '%s'", s->url);
		free (src);
		return FALSE;
	}

	/* attach our user data */
	s->udata = src;

	return TRUE;
}

void openft_source_remove (Protocol *p, Transfer *t, Source *s)
{
	struct ft_source *src;

	/* i cant imagine why this wouldnt be set? */
	assert (s->udata != NULL);

	if ((src = s->udata))
	{
		free_source (src);
		s->udata = NULL;
	}
}

/*****************************************************************************/

static int cmp_sources (Source *a, struct ft_source *ft_a,
                        Source *b, struct ft_source *ft_b)
{
	int ret;

	if (!(parse_source (ft_a, a->url)))
		return -1;

	if (!(parse_source (ft_b, b->url)))
		return 1;

	if ((ret = INTCMP (ft_a->host, ft_b->host)))
		return ret;

	if ((ret = strcmp (a->hash, b->hash)))
		return ret;

	if ((ret = strcmp (ft_a->request, ft_b->request)))
		return ret;

	/* 0 */
	return ret;
}

/*
 * TODO: It is likely that at least one of these sources has been put through
 * p->source_add and has our parsed struct ft_source object attached.  We
 * should probably use that instead of re-parsing both unconditionally.
 */
int openft_source_cmp (Protocol *p, Source *a, Source *b)
{
	struct ft_source *ft_a = new_source ();
	struct ft_source *ft_b = new_source ();
	int               ret;

	ret = cmp_sources (a, ft_a, b, ft_b);

	free_source (ft_a);
	free_source (ft_b);

	return ret;
}

int openft_user_cmp (Protocol *p, const char *a, const char *b)
{
	char *ptr;

	/* move past the alias */
	if ((ptr = strchr (a, '@')))
		a = ptr + 1;

	if ((ptr = strchr (b, '@')))
		b = ptr + 1;

	return strcmp (a, b);
}

/*****************************************************************************/

static BOOL submit_avail (FTNode *node, unsigned long *avail)
{
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_CHILD_PROP, 0)))
		return FALSE;

	ft_packet_put_uint32 (pkt, (uint32_t)(*avail), TRUE);
	ft_packet_send (FT_CONN(node), pkt);

	return TRUE;
}

void openft_upload_avail (Protocol *p, unsigned long avail)
{
	/* record the availability change so we can use it later */
	last_avail = avail;

	ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(submit_avail), &avail);
}

/*****************************************************************************/

unsigned long ft_upload_avail (void)
{
	/* WARNING: THIS BUG STILL EXISTS! */
#if 0
	unsigned long curr_avail;

	/* check to make sure our last record availability is the current
	 * one just in case giFT has not reported an avail change as it should */
	curr_avail = upload_availability ();
	assert (curr_avail == last_avail);
#endif

	return last_avail;
}
