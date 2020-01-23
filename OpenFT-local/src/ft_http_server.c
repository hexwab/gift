/*
 * $Id: ft_http_server.c,v 1.69 2005/02/27 16:42:54 mkern Exp $
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

#include "ft_netorg.h"
#include "md5.h"

#include "ft_http_server.h"
#include "ft_http_client.h"

/*****************************************************************************/

/*
 * Free-form version string to be sent back with every HTTP server reply.
 * This will eventually be used by the spider to produce more robust network
 * statistics and graphs, and has possible debugging applications down the
 * road.
 */
static char *http_versionstr = NULL;

/*****************************************************************************/

static BOOL method_head (TCPC *c, FTHttpRequest *req);
static BOOL method_get (TCPC *c, FTHttpRequest *req);
static BOOL method_push (TCPC *c, FTHttpRequest *req);
static BOOL method_unsupported (TCPC *c, FTHttpRequest *req);
static FTTransfer *prep_upload (TCPC *c, FTHttpRequest *req, Share *share);
static void send_file (int fd, input_id id, FTTransfer *xfer);

/*****************************************************************************/

/*
 * Accept a new incoming HTTP connection.  Please note that this does not
 * necessarily mean this is a download or an upload, merely that it is an
 * incoming TCP connection.
 */
void ft_http_server_incoming (int fd, input_id id, TCPC *c)
{
	TCPC *new_c;

	if (!(new_c = tcp_accept (c, FALSE)))
		return;

	/* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (FT_CFG_LOCAL_MODE)
	{
		if (!net_match_host (c->host, FT_CFG_LOCAL_ALLOW))
		{
			tcp_close (new_c);
			return;
		}
	}

	/* read the request */
	input_add (new_c->fd, new_c, INPUT_READ,
	           (InputCallback)get_client_request, TIMEOUT_DEF);
}

void get_client_request (int fd, input_id id, TCPC *c)
{
	FTHttpRequest *http_request;
	FDBuf         *buf;
	char          *data;
	size_t         data_len;
	BOOL           ret;
	int            n;

	if (fd == -1 || id == 0)
	{
		FT->DBGSOCK (FT, c, "PUSH command timed out");
		tcp_close (c);
		return;
	}

	buf = tcp_readbuf (c);
	assert (buf != NULL);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		tcp_close (c);
		return;
	}

	if (n > 0)
		return;

	data = (char *)fdbuf_data (buf, &data_len);

	/* look for the two trailing \n's optionally preceeded by \r chars */
	if (!(http_check_sentinel (data, data_len)))
		return;

	fdbuf_release (buf);

	if (!(http_request = ft_http_request_unserialize (data)))
	{
		tcp_close (c);
		return;
	}

	input_remove (id);

	if (strcasecmp (http_request->method, "HEAD") == 0)
		ret = method_head (c, http_request);
	else if (strcasecmp (http_request->method, "GET") == 0)
		ret = method_get (c, http_request);
	else if (strcasecmp (http_request->method, "PUSH") == 0)
		ret = method_push (c, http_request);
	else
		ret = method_unsupported (c, http_request);

	ft_http_request_free (http_request);

	/* make sure all queued writes are committed */
	tcp_flush (c, TRUE);

	if (!ret)
		tcp_close (c);
}

/*****************************************************************************/

static BOOL write_node (FTNode *node, FILE *f)
{
	/* dont show nodes which havent finished handshaking */
	if (node->session->stage < 4)
		return FALSE;

	fprintf (f, "%s %hu %hu %hu 0x%08x\n",
	         net_ip_str (node->ninfo.host),
	         (unsigned short)node->ninfo.port_openft,
	         (unsigned short)node->ninfo.port_http,
	         (unsigned short)node->ninfo.klass,
	         (unsigned int)node->version);

	return TRUE;
}

static BOOL create_nodes_share (Share *share)
{
	FILE *f;
	int   n;
	char *host_path;

	if (!(host_path = file_host_path (share->path)))
		return FALSE;

	if (!(f = fopen (host_path, "wb")))
		return FALSE;

	free (host_path);

	n = ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 0,
	                       FT_NETORG_FOREACH(write_node), f);

	/* HACK: prevent the file size from being zero */
	if (n <= 0)
		fprintf (f, "\n");

	fclose (f);

	return TRUE;
}

static Share *access_nodes_share (FTHttpRequest *req)
{
	static Share  *share = NULL;
	struct stat    st;
	unsigned char *md5;
	char *host_path;

	/* access the build the share element if this is the first time
	 * calling */
	if (!share)
	{
		char *unix_path;

		host_path = gift_conf_path ("OpenFT/nodes.serve");
		unix_path = file_unix_path (host_path);

		if (!unix_path)
			return NULL;

		share = share_new (unix_path);
		free (unix_path);

		if (!share)
			return NULL;

		share->mime = "text/plain";
	}

	assert (share != NULL);
	assert (share->path != NULL);

	/* zero the portions that we will be changed */
	share->size = 0;
	share_clear_hash (share);

	/*
	 * Actually write out the file for serving.
	 *
	 * WARNING: If a request here is made while another is being finished,
	 * the file being delivered will be truncated and rewritten.  This needs
	 * to be cleaned up some how.
	 */
	if (!(create_nodes_share (share)))
	{
		FT->DBGFN (FT, "unable to create %s for serving", share->path);
		return NULL;
	}

	/* need host path for file access */
	if (!(host_path = file_host_path (share->path)))
		return NULL;

	/* set the share size */
	if (stat (host_path, &st) != 0)
	{
		FT->DBGFN (FT, "unable to stat %s: %s", host_path, GIFT_STRERROR());
		free (host_path);
		return NULL;
	}

	if (st.st_size == 0)
	{
		FT->DBGFN (FT, "unable to serve %s: empty file", host_path);
		free (host_path);
		return NULL;
	}

	share->size = st.st_size;

	/* set the share hash */
	if (!(md5 = md5_digest (host_path, 0)))
	{
		FT->DBGFN (FT, "unable to hash %s: %s", host_path, GIFT_STRERROR());
		free (host_path);
		return NULL;
	}

	share_set_hash (share, "MD5", md5, 16, TRUE);
	free (md5);
	free (host_path);

	return share;
}

static int auth_http_code (int response)
{
	int code;

	/* translate giFT's upload authorization error codes into something
	 * more suitable for HTTP */
	switch (response)
	{
	 case UPLOAD_AUTH_ALLOW:        code = 200;  break;
	 case UPLOAD_AUTH_STALE:        code = 500;  break;
	 case UPLOAD_AUTH_MAX:
	 case UPLOAD_AUTH_MAX_PERUSER:
	 case UPLOAD_AUTH_HIDDEN:       code = 503;  break;
	 case UPLOAD_AUTH_NOTSHARED:
	 default:                       code = 404;  break;
	}

	return code;
}

static BOOL http_is_secure_path (const char *path)
{
	const char *reject;
	size_t      len;

	if (path[0] != '/')
		return FALSE;

#ifdef WIN32
	reject = "\\/";
#else
	reject = "/";
#endif

	while (1)
	{
		if ((len = strcspn (path, reject)) > 0)
		{
			/*
			 * On Windows, drop anything that starts with a single '.', as
			 * things like '...' are synonyms for '../..'.  On UNIX, we don't
			 * need to be as cautious, dropping only the explicit use of '..'
			 * as a directory element.
			 */
#ifdef WIN32
			if (path[0] == '.')
				return FALSE;
#else /* !WIN32 */
			if (len == 2 && strncmp (path, "..", 2) == 0)
				return FALSE;
#endif /* WIN32 */
		}

		if (path[len] == '\0')
			break;

		path += len + 1;
	}

	return TRUE;
}

static Share *auth_get_request (TCPC *c, FTHttpRequest *req, int *authresp,
                                upload_auth_t *auth_info)
{
	char  *reqstr;
	Share *share = NULL;
	int    ret   = UPLOAD_AUTH_NOTSHARED;

	/* access the secure decoded path through a lot of wasted allocation :) */
	reqstr = http_url_decode (req->request);
	assert (reqstr != NULL);

	if (http_is_secure_path (reqstr) == TRUE)
	{
		/*
		 * Handle a GET request for /nodes as a special exception to provide
		 * a snapshot of the [connected] nodes cache for debugging/diagnostic
		 * purposes.  This may be replaced in the future...
		 */
		if (strcmp (reqstr, "/nodes") == 0)
		{
			if ((share = access_nodes_share (req)))
				ret = UPLOAD_AUTH_ALLOW;
			else
				ret = UPLOAD_AUTH_NOTSHARED;
		}
		else
		{
			/*
			 * First we need to lookup the share entry that we are referring
			 * to before we can request authorization from giFT.  Then, we
			 * need to actually ask giFT for authorization.
			 */
			if ((share = FT->share_lookup (FT, SHARE_LOOKUP_HPATH, reqstr)))
				ret = FT->upload_auth (FT, net_ip_str (c->host), share, auth_info);
			else
				ret = UPLOAD_AUTH_NOTSHARED;
		}
	}

	free (reqstr);

	/* pass back the UPLOAD_AUTH_* response */
	assert (authresp != NULL);
	*authresp = ret;

	return share;
}

/*
 * Parse the Range: bytes=0-1000 header format used by HTTP.
 *
 * WARNING: This function is called multiple times for a single request
 * as the object hierarchy offers no logical place to store the start/stop
 * result.
 */
static BOOL get_request_range (FTHttpRequest *req, off_t *start, off_t *stop)
{
	char *range, *range0;              /* duped memory */
	BOOL ret;

	/* access and dup the Range header information for parsing */
	if (!(range = STRDUP (dataset_lookupstr (req->keylist, "Range"))))
		return FALSE;

	/* save the original copy for free'ing */
	range0 = range;

	if (!(string_sep (&range, "bytes=")) || !range)
		ret = FALSE;
	else
	{
		*start = (off_t)(gift_strtoul (string_sep (&range, "-")));
		*stop  = (off_t)(gift_strtoul (string_sep (&range, " ")));

		ret = TRUE;
	}

	free (range0);

	return ret;
}

static char *server_version (void)
{
	if (http_versionstr == NULL)
	{
		http_versionstr = stringf_dup ("OpenFT/%d.%d.%d.%d (%s)",
		                               OPENFT_MAJOR, OPENFT_MINOR,
		                               OPENFT_MICRO, OPENFT_REV,
		                               platform_version());
	}

	return http_versionstr;
}

static void add_reply_success (FTHttpReply *reply, FTHttpRequest *req,
                               Share *share, upload_auth_t *auth_info)
{
	Hash *hash;
	char *md5str;
	char *buf_range;
	char *buf_length;
	char *server;
	off_t start = 0;
	off_t stop  = 0;
	off_t entity;

	hash = share_get_hash (share, "MD5");
	assert (hash != NULL);

	if (!(get_request_range (req, &start, &stop)) || stop == 0)
		stop = share->size;

	/* total entity size */
	entity = share->size;
	assert (entity > 0);

	/* construct the Content-Range reply */
	buf_range = stringf_dup ("bytes %lu-%lu/%lu",
	                         (unsigned long)(start),
	                         (unsigned long)(stop - 1),
	                         (unsigned long)(entity));

	/* total content length */
	buf_length = stringf_dup ("%lu",
	                          (unsigned long)(stop - start));

	/* create the ascii-representation of the internal hash to deliver */
	md5str = hash->algo->dspfn (hash->data, hash->len);
	assert (md5str != NULL);

	/* calculate the free-form server version, mostly for statistics and
	 * debugging purposes */
	server = server_version();
	assert (server != NULL);

	dataset_insertstr (&reply->keylist, "Content-Range",  buf_range);
	dataset_insertstr (&reply->keylist, "Content-Length", buf_length);
	dataset_insertstr (&reply->keylist, "Content-Type",   share->mime);
	dataset_insertstr (&reply->keylist, "Content-MD5",    md5str);
	dataset_insertstr (&reply->keylist, "Server",         server);

	if (strcmp (req->request, "/nodes") == 0)
	{
		dataset_insertstr (&reply->keylist, "X-Class",
		                   stringf ("%d", openft->ninfo.klass));
	}

	if (openft->ninfo.alias)
		dataset_insertstr (&reply->keylist, "X-OpenftAlias", openft->ninfo.alias);

	/* i chose allocation here so that we didnt have to intersperse the ugly
	 * stringf calls with the insert block...sigh */
	free (buf_range);
	free (buf_length);
	free (md5str);
}

static void add_reply_503_queued (FTHttpReply *reply, FTHttpRequest *req,
                                  Share *share, upload_auth_t *auth_info)
{
	char *buf_pos;
	char *buf_retry;

	/*
	 * Report the current position in the local queue for the remote peer.
	 * This data will be directly shown to user through the interface
	 * protocol as the protocol-specific status.
	 */
	buf_retry = stringf_dup ("%u", (60 * SECONDS));
	buf_pos   = stringf_dup ("%u of %u",
	                         auth_info->queue_pos, auth_info->queue_ttl);

	dataset_insertstr (&reply->keylist, "X-ShareStatus",   "Queued");
	dataset_insertstr (&reply->keylist, "X-QueuePosition", buf_pos);
	dataset_insertstr (&reply->keylist, "X-QueueRetry",    buf_retry);

	free (buf_retry);
	free (buf_pos);
}

static void add_reply_503_hidden (FTHttpReply *reply, FTHttpRequest *req,
                                  Share *share, upload_auth_t *auth_info)
{
	/* bit of a hack to show more info when the user has actually selected
	 * to "hide" shares */
	dataset_insertstr (&reply->keylist, "X-ShareStatus", "Not sharing");
}

static FTHttpReply *construct_reply (FTHttpRequest *req, int authresp, int code,
                                     Share *share, upload_auth_t *auth_info)
{
	FTHttpReply *reply;

	if (!(reply = ft_http_reply_new (code)))
		return NULL;

	/* only a select set of codes here need special headers */
	if (code >= 200 && code <= 299)
		add_reply_success (reply, req, share, auth_info);
	else if (code == 503)
	{
		if (authresp == UPLOAD_AUTH_MAX)
			add_reply_503_queued (reply, req, share, auth_info);
		else if (authresp == UPLOAD_AUTH_HIDDEN)
			add_reply_503_hidden (reply, req, share, auth_info);
	}

	return reply;
}

/* this is more like a hackish macro to implement HEAD and GET, so by all
 * means feel free to lart me */
static Share *head_get_and_write (TCPC *c, FTHttpRequest *req, int *http_code)
{
	Share         *share;
	int            authresp = UPLOAD_AUTH_NOTSHARED;
	int            code;
	FTHttpReply   *reply;
	upload_auth_t  auth_info;

	/* call FT->upload_auth (FT, ...) */
	share = auth_get_request (c, req, &authresp, &auth_info);
	code  = auth_http_code (authresp);

	/* deliver the reply */
	reply = construct_reply (req, authresp, code, share, &auth_info);
	ft_http_reply_send (reply, c);

	if (http_code)
		*http_code = code;

	return share;
}

static BOOL method_head (TCPC *c, FTHttpRequest *req)
{
	head_get_and_write (c, req, NULL);

	/* always abort the connection immediately aftering sending the HEAD
	 * response */
	return FALSE;
}

static BOOL method_get (TCPC *c, FTHttpRequest *req)
{
	FTTransfer *xfer;
	Share      *share;
	int         code;

	/* do all the magic necessary to authorize with giFT, get header
	 * information, and deliver it */
	share = head_get_and_write (c, req, &code);

	/*
	 * Abort the connection after we send the header in the event of an
	 * error.  Please note that this does go against what the HTTP RFC says
	 * is "good server behaviour".  Sorry :)
	 */
	if (code < 200 || code > 299)
		return FALSE;

	/* logic in auth_get_request should guarantee that if the code is within
	 * the 200 range, share is non-NULL */
	assert (share != NULL);

	/*
	 * Handles registration with giFT, opening of the shared path, and
	 * transfer object construction.  Perhaps this should be divided out
	 * at some point...
	 */
	if (!(xfer = prep_upload (c, req, share)))
	{
		FT->err (FT, "unable to begin upload to %s for %s",
		         net_ip_str (c->host), share->path);
		return FALSE;
	}

	xfer->http = c;

	input_add (xfer->http->fd, xfer, INPUT_WRITE,
	           (InputCallback)send_file, TIMEOUT_DEF);

	return TRUE;
}

static BOOL method_push (TCPC *c, FTHttpRequest *req)
{
	FTTransfer *xfer;

	/*
	 * We need to locate the FTTransfer object in order to proceed, but we
	 * also do this for security reasons so that we know we have actually
	 * requested the file they are about to send us.
	 */
	if (!(xfer = push_access (c->host, req->request)))
	{
		FT->DBGSOCK (FT, c, "unable to find push entry for %s", req->request);
		return FALSE;
	}

	ft_transfer_status (xfer, SOURCE_WAITING, "Received HTTP PUSH");

	/* switch over the connection state as though we have just completed
	 * a new outgoing connection */
	xfer->http = c;
	input_add (xfer->http->fd, xfer, INPUT_WRITE,
	           (InputCallback)get_complete_connect, TIMEOUT_DEF);

	return TRUE;
}

static BOOL method_unsupported (TCPC *c, FTHttpRequest *req)
{
	FTHttpReply *reply;

	if (!(reply = ft_http_reply_new (501)))
		return FALSE;

	/* TODO: add more to the reply? */
	ft_http_reply_send (reply, c);

	return FALSE;
}

/*****************************************************************************/

static FILE *open_share (Share *share)
{
	FILE *f;
	char *host_path;

	if (!(host_path = file_host_path (share->path)))
		return NULL;

	f = fopen (host_path, "rb");
	free (host_path);

	return f;
}

static Transfer *get_gift_transfer (Chunk **chunk, Source **source, TCPC *c,
                                    FTHttpRequest *req, Share *share,
                                    off_t start, off_t stop)
{
	Transfer *t;
	char     *alias;
	char     *user;

    /* construct the [alias@]ipaddress username form used by this plugin */
	alias = dataset_lookupstr (req->keylist, "X-OpenftAlias");
	user = ft_node_user_host (c->host, alias);

	/* and away we go... */
	t = FT->upload_start (FT, chunk, user, share, start, stop);
	assert (t != NULL);

	assert ((*chunk) != NULL);
	assert ((*chunk)->transfer == t);
	*source = (*chunk)->source;
	assert ((*source) != NULL);

	return t;
}

static FTTransfer *get_openft_transfer (Transfer *t, Chunk *c, Source *s)
{
	FTTransfer *xfer;

	/* this is no longer true... */
#if 0
	/*
	 * p->upload_start will call p->source_add for us, and we will then parse
	 * the source url.  So if we got this far, we know it damn well better
	 * have been set.
	 */
	assert (s->udata != NULL);
#endif

	if (!(xfer = ft_transfer_new (TRANSFER_UPLOAD, t, c, s)))
		return NULL;

	assert (c->udata == NULL);
	c->udata = xfer;

	return xfer;
}

static FTTransfer *prep_upload (TCPC *c, FTHttpRequest *req, Share *share)
{
	Transfer   *t;
	Chunk      *chunk;
	Source     *source;
	FTTransfer *xfer;
	FILE       *f;
	off_t       start = 0;
	off_t       stop  = 0;

	/* open the shared file locally to prepare for upload */
	if (!(f = open_share (share)))
	{
		FT->err (FT, "unable to open share described by '%s'",
		         share->path, GIFT_STRERROR());
		return NULL;
	}

	/* access the range that we will be serving so we can seek the file handle
	 * and register the upload */
	if (!(get_request_range (req, &start, &stop)) || stop == 0)
		stop = share->size;

	if ((fseek (f, start, SEEK_SET)) != 0)
	{
		FT->err (FT, "unable to seek %s: %s", share->path, GIFT_STRERROR());
		fclose (f);
		return NULL;
	}

	if (!(t = get_gift_transfer (&chunk, &source, c, req, share, start, stop)))
	{
		fclose (f);
		return NULL;
	}

	xfer = get_openft_transfer (t, chunk, source);
	assert (xfer != NULL);

	ft_transfer_set_fhandle (xfer, f);

	return xfer;
}

/*****************************************************************************/

static void send_file (int fd, input_id id, FTTransfer *xfer)
{
	unsigned char buf[RW_BUFFER];
	size_t        read_len;
	size_t        send_len;
	int           sent_len;
	Transfer     *t;
	Chunk        *c;
	Source       *s;
	FILE         *f;

	if (fd == -1 || id == 0)
	{
		FT->DBGFN (FT, "Upload time out, fd = %d, id = 0x%X.", fd, id);
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED, "Write timed out");
		return;
	}

	t = ft_transfer_get_transfer (xfer);
	c = ft_transfer_get_chunk (xfer);
	s = ft_transfer_get_source (xfer);
	f = ft_transfer_get_fhandle (xfer);

	assert (t != NULL);
	assert (c != NULL);
	assert (s != NULL);
	assert (f != NULL);

	/* overflowing shouldnt be possible, but just in case.... */
#if 0
	assert (c->start + c->transmit < c->stop);
#endif
	/*
	 * Ask giFT for the size we should send.  If this returns 0, the upload
	 * was suspended.
	 */
	if ((send_len = upload_throttle (c, sizeof (buf))) == 0)
		return;

	/* read from the file the number of bytes we plan to send */
	if ((read_len = fread (buf, sizeof (char), send_len, f)) == 0)
	{
		FT->err (FT, "unable to read upload share: %s", GIFT_STRERROR());
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED, "Local read error");
		return;
	}

	/* write the block */
	if ((sent_len = tcp_send (xfer->http, buf, read_len)) <= 0)
	{
		ft_transfer_stop_status (xfer, SOURCE_CANCELLED,
		                         stringf ("Error sending: %s", GIFT_NETERROR()));
		return;
	}

	/* short write, rewind our fread to match */
	if (sent_len < read_len)
	{
		FT->DBGFN (FT, "short write, rewinding read stream");

		if ((fseek (f, -((off_t)(read_len - sent_len)), SEEK_CUR)) != 0)
		{
			FT->err (FT, "unable to seek back: %s", GIFT_STRERROR());
			ft_transfer_stop_status (xfer, SOURCE_CANCELLED,
			                         "Local seek error");
			return;
		}
	}

	/* report our progress to giFT */
	FT->chunk_write (FT, t, c, s, buf, (size_t)sent_len);
}
