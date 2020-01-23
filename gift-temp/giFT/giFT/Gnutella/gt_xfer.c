/*
 * $Id: gt_xfer.c,v 1.24 2003/05/04 21:23:04 hipnod Exp $
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

#include "ft_xfer.h"

#include "ft_http_client.h"
#include "ft_http_server.h"

#include "html.h"

#include "gt_share.h"
#include "gt_share_file.h"

#include "gt_protocol.h"
#include "gt_packet.h"

#include "gt_node.h"
#include "gt_netorg.h"
#include "gt_connect.h"

#include "gt_xfer.h"

/******************************************************************************/

/* An alternative to carrying the push TTL around in the source url */
#define PUSH_MAX_TTL            12

/******************************************************************************/

/* Gnutella URL format:
 *
 * (This will change at least once more)
 *
 * Gnutella://<user>:<port>:<client_guid>/<index>/<filename>
 */
#if 0
char *gt_share_url_new (Gt_Share *share, unsigned long ip, unsigned short port,
                        gt_guid *client_guid)
{
	char *buf;
	char *encoded;

	buf = malloc (1024);

	if (!(encoded = gt_url_encode (share->filename)))
	{
		free (buf);
		return NULL;
	}

	snprintf (buf, 1024, "%s://%s:%hu:%s/%u/%s", gt_proto->name,
	          net_ip_str (ip), port, guid_str (client_guid), share->index,
	          encoded);

	TRACE (("index=%d, filename=%s, url=%s", share->index, encoded, buf));

	free (encoded);

	return buf;
}
#endif

/* hopefully temporary code to make push downloads work
 *
 * server_port is the server's gnutella port. This should probably pass
 * back both the gnutella port instead and the peer's connecting port, to
 * help in disambiguating different users behind the same firewall. */
char *gt_source_url_new (char *filename, uint32_t index,
                        in_addr_t user_ip, uint16_t user_port,
                        in_addr_t server_ip, uint16_t server_port,
                        int firewalled, gt_guid *client_id)
{
	char *buf;
	char *encoded;
	char *server;
	char *user;

	buf = malloc (2048);

	if (!(encoded = gt_url_encode (filename)))
	{
		free (buf);
		return NULL;
	}

	user   = STRDUP (net_ip_str (user_ip));
	server = STRDUP (net_ip_str (server_ip));

	snprintf (buf, 2048, "%s://%s:%hu@%s:%hu[%s]:%s/%u/%s", gt_proto->name,
	          user, user_port, server, server_port, 
	          (firewalled ? "FW" : ""), guid_str (client_id),
	          index, encoded);

	free (server);
	free (user);
	free (encoded);

	return buf;
}

char *gt_share_url_new (Gt_Share *share,
                        in_addr_t user_ip, uint16_t user_port,
                        in_addr_t server_ip, uint16_t server_port,
                        int firewalled, gt_guid *client_id)
{
	return gt_source_url_new (share->filename, share->index,
	                          user_ip, user_port, server_ip, server_port,
	                          firewalled, client_id);
}

#if 0
static int parse_url (char *url, uint32_t *r_ip, uint16_t *r_port,
                      char **r_pushid, uint32_t *r_index, char **r_fname)
{
	string_sep (&url, "://");

	/* TODO: check for more errors */

	*r_ip     = inet_addr (string_sep (&url, ":"));
	*r_port   = ATOUL     (string_sep (&url, ":"));
	*r_pushid =            string_sep (&url, "/");
	*r_index  = ATOUL     (string_sep (&url, "/"));
	*r_fname  = url;

	return TRUE;
}
#endif

static int parse_url (char *url, uint32_t *r_user_ip, uint16_t *r_user_port,
                      uint32_t *r_server_ip, uint16_t *r_server_port,
                      int *firewalled, char **r_pushid,
                      uint32_t *r_index, char **r_fname)
{
	char *port_and_flags;
	char *flag;

	string_sep (&url, "://");

	/* TODO: check for more errors */

	*r_user_ip     = inet_addr (string_sep (&url, ":"));
	*r_user_port   = ATOUL     (string_sep (&url, "@"));
	*r_server_ip   = inet_addr (string_sep (&url, ":"));

	/* handle bracketed flags after port. ugh, this is so ugly */
	port_and_flags = string_sep (&url, ":");
	*r_server_port = ATOUL (string_sep (&port_and_flags, "["));

	if (!string_isempty (port_and_flags))
	{
		/* grab any flags inside the brackets */
		while ((flag = string_sep_set (&port_and_flags, ",]")))
		{
			if (!STRCMP (flag, "FW"))
				*firewalled = TRUE;
		}
	}

	*r_pushid      =            string_sep (&url, "/");
	*r_index       = ATOUL     (string_sep (&url, "/"));
	*r_fname       = url;

	return TRUE;
}

GtSource *gt_source_new (char *url)
{
	GtSource  *new_gt;
	char      *fname;
	char      *url0;
	char      *guid_ascii;

	url0 = url;
	if (!(url = STRDUP (url0)))
		return NULL;

	if (!(new_gt = malloc (sizeof (GtSource))))
	{
		free (url);
		return NULL;
	}

	memset (new_gt, 0, sizeof (GtSource));

#if 0
	if (!parse_url (url, &new_gt->ip, &new_gt->port, &push_id,
	                &new_gt->index, &fname))
#endif
	if (!parse_url (url, &new_gt->user_ip, &new_gt->user_port,
	                &new_gt->server_ip, &new_gt->server_port,
	                &new_gt->firewalled, &guid_ascii, &new_gt->index, &fname))
	{
		free (url);
		free (new_gt);
		return NULL;
	}

	new_gt->filename = STRDUP (fname);
	new_gt->guid     = guid_bin (guid_ascii);

	free (url);

	return new_gt;
}

/* 
 * The source URL is stored on disk and could be outdated if the format has 
 * changed. This updates it with any changes.
 */
static void replace_url (Source *src, GtSource *gt)
{
	char *url;
	char *decoded;

	/* need to decode the filename, as it is encoded */
	if (!(decoded = gt_url_decode (gt->filename)))
		return;

	if (!(url = gt_source_url_new (decoded, gt->index, 
	                               gt->user_ip, gt->user_port, 
	                               gt->server_ip, gt->server_port, 
	                               gt->firewalled, gt->guid)))
	{
		free (decoded); 
		return;
	}

	free (decoded);

	/* swap urls */
	free (src->url);
	src->url = url;
}

void gt_source_free (GtSource *gt)
{
	free (gt->guid);
	free (gt->filename);
	free (gt);
}

/******************************************************************************/

static FileShare *lookup_index (GtTransfer *xfer, char *request)
{
	FileShare *file;
	char      *index;
	char      *filename;

	filename = request;
	index    = string_sep (&filename, "/");

	if (!filename || !index)
		return NULL;

	/* the filename may or may not be url encoded */
	filename = gt_url_decode (filename);
	file = gt_share_local_lookup_by_index (ATOUL (index), filename);
	free (filename);

	return file;
}

static FileShare *lookup_urns (GtTransfer *xfer, char *urns)
{
	FileShare *file = NULL;
	char      *urn;

	/*
	 * Try to lookup all urns provided in the header, 
	 * until one is found.
	 */
	while (!file && !string_isempty (urns))
	{
		urn = string_sep_set (&urns, ", ");
		file = gt_share_local_lookup_by_urn (urn);
	}

	return file;
}

static FileShare *lookup_uri_res (GtTransfer *xfer, char *request)
{
	FileShare  *file     = NULL;
	char       *resolver = NULL;
	char       *urn;

	resolver = string_sep (&request, "?");
	urn      = string_sep (&request, " ");

	if (resolver && !strcasecmp (resolver, "N2R"))
	{
		string_trim (request);
		file = lookup_urns (xfer, urn);
	}

	if (file)
		GT->dbg (GT, "file=%s", SHARE_DATA(file)->hpath);

	return file;
}

/* Take a request for a file, i.e. everything after GET in:
 *
 * "GET /get/1279/filename.mp3"
 *
 * and convert it to a localized path to a file.
 *
 * Path has been "secured" already if necessary.
 *
 * The path returned must be static.
 * TODO: this interface is a bit bizarre */
char *gt_localize_request (GtTransfer *xfer, char *s_path, int *authorized)
{
	static char open_path[PATH_MAX];
	char       *namespace;
	char       *path, *path0;
	char       *content_urns;
	FileShare  *file;

	/* TODO: use authorized for Browse Host (BH) requests */
	if (!STRCMP (s_path, "/"))
	{
		/* *authorized = TRUE; */
		TRACE (("received unimplemented Browse Host request"));
		return NULL;
	}

	if (authorized)
		*authorized = FALSE;

	if (!(path0 = path = STRDUP (s_path)))
		return NULL;

	GT->dbg (GT, "path=%s", path);

	/* get rid of leading slash */
	string_sep (&path, "/");
	namespace = string_sep (&path, "/");

	if (!namespace || !path)
	{
		GT->DBGFN (GT, "null namespace or path: %s %s\n", namespace, path);
		free (path0);
		return NULL;
	}

	/*
	 * If the client supplied "X-Gnutella-Content-URN: ", lookup
	 * by that instead of the request.
	 */
	content_urns = dataset_lookupstr (xfer->header, "x-gnutella-content-urn");

	if (content_urns)
		file = lookup_urns (xfer, content_urns);
	else if (!strcasecmp (namespace, "get"))
		file = lookup_index (xfer, path);
	else if (!strcasecmp (namespace, "uri-res"))
		file = lookup_uri_res (xfer, path);

	/*
	 * Set xfer->content_urn [which replies with 'X-Gnutella-Content-URN']
	 * to a comma-separated list of all URNs for this file.
	 */
	xfer->content_urns = gt_share_local_get_urns (file);

	if (!file)
	{
		GT->DBGFN (GT, "bad request: %s %s", namespace, path);
		free (path0);
		return NULL;
	}

	free (path0);

	if (!share_complete (file))
		return NULL;

	/* argh, need to return static data */
	strcpy (open_path, SHARE_DATA(file)->hpath);

	return open_path;
}

/******************************************************************************/

/*
 * Setup a request string. Try request-by-hash (/uri-res/N2R?urn:sha1:..),
 * but if there are any problems, fallback to a "/get/<index>/<filename>"
 * request.
 */
static char *request_str (Chunk *chunk, uint32_t index, char *filename)
{
	static char request[RW_BUFFER];
	char       *hash = chunk->transfer->hash;

	/*
	 * Sigh, the uri-res method doesn't seem to work with many clients,
	 * so we do it probablistically. Ideally, we would attach some
	 * info to the download as to whether the uri-res failed or not,
	 * either through the source url or some other way, and if it failed
	 * fallback to an index request.
	 */
	if (3.0 * rand () / (RAND_MAX + 1.0) < 1)
		goto do_index_request;

	GT->dbg (GT, "hash=%s", hash);

	if (hash)
	{
		char *str0, *str;

		if (!(str0 = STRDUP (hash)))
			goto do_index_request;

		str = str0;
		string_sep (&str, ":");

		/* hashes are canonically uppercase on the gnet */
		string_upper (str);

		if (str)
		{
			snprintf (request, sizeof (request), "/uri-res/N2R?urn:sha1:%s",
			          str);
			free (str0);
			return request;
		}

		free (str0);
	}

do_index_request:
	/* NOTE: filename doesnt have leading '/' here, that may change */
	snprintf (request, sizeof (request) - 1, "/get/%u/%s", index, filename);

	return request;
}

static void send_push (Chunk *chunk, GtTransfer *xfer,
                       GtSource *gt, Connection *server)
{
	Gt_Packet *packet;
	in_addr_t  ip;

	if (!(packet = gt_packet_new (GT_PUSH_REQUEST, PUSH_MAX_TTL, NULL)))
		return;

	gt_packet_put_ustr   (packet, gt->guid, 16);
	gt_packet_put_uint32 (packet, gt->index);
	gt_packet_put_ip     (packet, NODE(server)->my_ip);
	gt_packet_put_port   (packet, gt_self->gt_port);

	if (gt_packet_error (packet))
		gt_packet_free (packet);
	else
	{
		char *client_id;

		if (!(client_id = guid_str (gt->guid)))
		{
			GT->warn (GT, "not sending push request -- null guid!");
			gt_packet_free (packet);
			return;
		}

		gt_packet_send (server, packet);

		/*
		 * For servents reporting local IP addresses in query hits, dont
		 * check the IP address of the indirect request: only check the
		 * servent ID.  This is because the servent can come from an
		 * external IP address (NOTE: this is not true if the servent is
		 * really local...)
		 */
		ip = gt->user_ip;
		if (net_match_host (ip, "LOCAL"))
			ip = 0;

		gt_http_server_indirect_add (chunk, ip, client_id);
		gt_transfer_ref (NULL, chunk, xfer);

		source_status_set (chunk->source, SOURCE_WAITING, "Sent PUSH request");
	}
}

static void handle_push_download (Chunk *chunk, GtTransfer *xfer, GtSource *gt)
{
	Gt_Node *server;

	/*
	 * Try to find the server that supplied this result.
	 */
	if (!(server = gt_node_lookup (gt->server_ip, gt->server_port)))
	{
		server = gt_node_register (gt->server_ip, gt->server_port, 
		                           NODE_SEARCH, 0, 0);
	}

	if (!server)
	{
		GT->err (GT, "couldn't register server");
		return;
	}

	if (server->state & (NODE_CONNECTED | NODE_CONNECTING_2))
	{
		assert (GT_CONN(server) != NULL);

		/* Server is in a state for receiving packets -- send the push */
		send_push (chunk, xfer, gt, GT_CONN(server));
	}
	else if (server->state & NODE_CONNECTING_1)
	{
		/* dont try to connect again; wait till we're connected */
		return;
	}
	else if (gt_conn_need_connections () && gt_connect (server) >= 0)
	{
		/* We tried to connect to the server so we could deliver the
		 * push request eventually 
		 * NOTE: this doesnt send a push until the next chunk timeout */
		return;
	}
	else
	{
		/* send a push to a random server thats already connected */
		GtNode *rand_node = gt_conn_random (NODE_SEARCH, NODE_CONNECTED);

		if (!rand_node)
			return;

		send_push (chunk, xfer, gt, GT_CONN(rand_node));
	}
}

static int set_request (GtTransfer *xfer, Chunk *chunk, GtSource *gt_src)
{
	char  *request;

	if (!chunk || !xfer)
		return FALSE;

	request = request_str (chunk, gt_src->index, gt_src->filename);
	GT->dbg (GT, "request=%s", request);

	if (!gt_transfer_set_request (xfer, request))
	{
		GT->DBGFN (GT, "UI made an invalid request for '%s'", request);
		return FALSE;
	}

	return TRUE;
}

static int is_firewalled_source (GtSource *gt)
{
	if (gt_is_local_ip (gt->user_ip, gt->server_ip))
		return TRUE;

	if (gt->firewalled)
		return TRUE;

	return FALSE;
}

static int self_is_firewalled (void)
{
	if (!gt_self->firewalled)
		return FALSE;

	/* Pretend we are not firewalled at the beginning in order
	 * to possibly get more connections, to prove we are not firewalled. */
	if (gt_uptime () < 10 * EMINUTES)
		return FALSE;

	/* we are firewalled */
	return TRUE;
}

int gnutella_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                             Source *source)
{
	GtTransfer    *xfer;
	GtSource      *gt;
	off_t          start;
	off_t          stop;

	if (!(gt = gt_source_new (source->url)))
		return FALSE;

	replace_url (source, gt);

	GT->dbg (GT, "ip=%s port=%hu guid=%s index=%u filename=%s", 
	         net_ip_str (gt->user_ip), gt->user_port, 
	         guid_str (gt->guid), gt->index, gt->filename);

	/* thank you, pretender :) */
	start = chunk->start + chunk->transmit;
	stop  = chunk->stop;

	if (!(xfer = gt_transfer_new (gt_http, gt_download, gt->user_ip,
	                              gt->user_port, start, stop)))
	{
		TRACE (("gt_transfer_new failed"));
		gt_source_free (gt);
		return FALSE;
	}

	if (!set_request (xfer, chunk, gt))
	{
		gt_transfer_close (xfer, TRUE);
		gt_source_free (gt);
		return FALSE;
	}

	/* 
	 * Probabalistically do a push request, but only if we are not
	 * firewalled. It would be much better to do this based on which method
	 * the last request was, but there's currently no way to store this
	 * other than mangling the source url which is ugly.
	 */
	if (!self_is_firewalled() && (is_firewalled_source (gt) ||
	                             10.0 * rand() / (RAND_MAX+1.0) < 1.0))
	{
		handle_push_download (chunk, xfer, gt);
	}
	else
	{
		gt_http_client_get (gt_http, chunk, xfer);
	}

	gt_source_free (gt);

	return TRUE;
}

void gnutella_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                             Source *source, int complete)
{
	gt_download_cancel (chunk, NULL);
}

int gnutella_upload_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source, unsigned long avail)
{
	return TRUE;
}

void gnutella_upload_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source)
{
	gt_upload_cancel (chunk, NULL);
}

int gnutella_chunk_suspend (Protocol *p, Transfer *transfer, Chunk *chunk,
                            Source *source)
{
	return gt_chunk_suspend (chunk, transfer, NULL);
}

int gnutella_chunk_resume (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source)
{
	return gt_chunk_resume (chunk, transfer, NULL);
}

/*****************************************************************************/

int gnutella_source_cmp (Protocol *p, Source *a, Source *b)
{
	GtSource *gt_a = NULL;
	GtSource *gt_b = NULL;
	int       ret  = 0;

	if (!(gt_a = gt_source_new (a->url)) ||
	    !(gt_b = gt_source_new (b->url)))
	{
		gt_source_free (gt_a);
		gt_source_free (gt_b);
		return -1;
	}

	if (gt_a->user_ip > gt_b->user_ip)
		ret =  1;
	else if (gt_a->user_ip < gt_b->user_ip)
		ret = -1;

	if (ret == 0)
	{
		ret  = strcmp (gt_a->filename, gt_b->filename);
		ret |= INTCMP (gt_a->index,    gt_b->index);
	}

	gt_source_free (gt_a);
	gt_source_free (gt_b);

	return ret;
}

int gnutella_source_remove (Protocol *p, Transfer *transfer, Source *source)
{
	GtSource *gt_src;
	in_addr_t ip;

	if (!(gt_src = gt_source_new (source->url)))
		return FALSE;

	ip = gt_src->user_ip;
	if (net_match_host (ip, "LOCAL"))
		ip = 0;

	/* 
	 * NOTE: If we have multiple downloads going for this user, this may
	 *       remove the wrong one, but should keep the count of requests 
	 *       correct.
	 */
	gt_http_server_indirect_remove (NULL, ip, gt_src->guid);
	gt_source_free (gt_src);

	return TRUE;
}
