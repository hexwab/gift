/*
 * $Id: http_request.c,v 1.4 2003/03/20 05:01:10 rossta Exp $
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

#include "gt_accept.h"
#include "gt_web_cache.h"

#include "http_request.h"

/*****************************************************************************/

/* This file suffers from severe duplication with ft_http_client.c */

/*****************************************************************************/

static void decode_chunked_data (int fd, input_id id, Connection *c);
static void read_chunked_header (int fd, input_id id, Connection *c);

/*****************************************************************************/

/*
 * Only allow alphanumeric, '.', and '/' chars to be involved in the url
 */
static int url_is_clean (char *url)
{
	char c;
	int  ret;

	assert (url);

	ret = TRUE;

	while ((c = *url++))
	{
		if (isalnum (c) || c == '.' || c == '/')
			continue;

		ret = FALSE;
	}

	return ret;
}

int http_parse_url (char *url, char **r_host_name, char **r_request)
{
	char *host_name, *request;
	int   prefix_len = strlen ("http://");

	assert (url);

	if (r_host_name)
		*r_host_name = NULL;
	if (r_request)
		*r_request = NULL;

	if (!url_is_clean (url))
		return FALSE;

	if (!strncmp (url, "http://", prefix_len))
		url += prefix_len;

	host_name = string_sep (&url, "/");
	request   = url;

	if (r_host_name)
		*r_host_name = host_name;
	if (r_request)
		*r_request = request;

	return TRUE;
}

HttpRequest *http_request_new (char *host, char *path, char *request)
{
	HttpRequest *http_request;

	if (!(http_request = malloc (sizeof (HttpRequest))))
		return NULL;

	memset (http_request, 0, sizeof (HttpRequest));

	http_request->host    = STRDUP (host);
	http_request->path    = STRDUP (path);
	http_request->request = STRDUP (request);

	return http_request;
}

void http_request_free (HttpRequest *req)
{
	free (req->host);
	free (req->path);
	free (req->request);

	dataset_clear (req->headers);
}

void http_request_close (HttpRequest *req, int error_code)
{
	/* notify the callback */
	if (req->close_req_func)
		(*req->close_req_func) (req, error_code);

	if (req->c)
		tcp_close (req->c);

	http_request_free (req);
}

/*****************************************************************************/

static int write_data (HttpRequest *req, char *data, int len)
{
	if (req->recv_func && !(*req->recv_func) (req, data, len))
	{
		http_request_close (req, -1);
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static int write_header (Dataset *d, DatasetNode *node, String *s)
{
	char *header = node->key;
	char *field  = node->value;

	string_appendf (s, "%s: %s\r\n", header, field);

	return FALSE;
}

static int http_send (Connection *c, char *command, char *request,
                      Dataset *headers)
{
	String      *s;
	int          ret;

	if (!command || !request)
		return -1;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return -1;

	string_appendf (s, "%s %s HTTP/1.1\r\n", command, request);

	dataset_foreach (headers, DATASET_FOREACH (write_header), s);
	string_append (s, "\r\n");

	ret = net_send (c->fd, s->str, s->len);
	string_free (s);

	return ret;
}

static HttpRequest *get_request (Connection *c)
{
	return c->udata;
}

/*****************************************************************************/

static void decode_chunked_data (int fd, input_id id, Connection *c)
{
	HttpRequest *req;
	FDBuf       *buf;
	char        *data;
	int          data_len;
	int          n;

	req = get_request (c);

	if (!req->size)
	{
		http_request_close (req, 200);
		return;
	}

	buf = tcp_readbuf (c);

	if ((n = fdbuf_fill (buf, req->size)) < 0)
	{
		TRACE (("error on host %s: %s", req->host, GIFT_STRERROR ()));
		http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &data_len);
	fdbuf_release (buf);

	if (!write_data (req, data, data_len))
		return;

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) read_chunked_header, TIMEOUT_DEF);
}

static void read_chunked_header (int fd, input_id id, Connection *c)
{
	HttpRequest *req;
	FDBuf       *buf;
	char        *response;
	int          n;

	req = get_request (c);

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\r\n")) < 0)
	{
		TRACE (("error on %s: %s", req->host, GIFT_STRERROR ()));
		http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* read the chunk size, its a hexadecimal integer */
	req->size = strtoul (response, NULL, 16);
	TRACE (("server sent chunk size of %lu", req->size));

	if (req->size == ULONG_MAX)
	{
		TRACE (("overflow reading chunk size: %s", GIFT_STRERROR ()));
		http_request_close (req, -1);
		return;
	}

	if (req->size == 0)
	{
		/* ok, done */
		if (!write_data (req, NULL, 0))
			return;

		/* there could be a CRLF at the end. should we read it?
		 * To avoid screwing up persistent http, yes.. */
		http_request_close (req, 200);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) decode_chunked_data, TIMEOUT_DEF);
}

static void read_file (int fd, input_id id, Connection *c)
{
	HttpRequest   *req;
	FDBuf         *buf;
	int            n, len;
	unsigned char *data;

	req = get_request (c);

	if (!req->size)
	{
		http_request_close (req, 200);
		return;
	}

	buf = tcp_readbuf (c);

	if ((n = fdbuf_fill (buf, req->size)) < 0)
	{
		TRACE (("error reading from %s: %s", req->host, GIFT_STRERROR ()));
		http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &len);
	fdbuf_release (buf);

	if (!write_data (req, data, len))
		return;

	/* success */
	http_request_close (req, 200);
}

static int parse_server_response (char *reply, HttpRequest *req)
{
	char *response;
	int   code;    /* 200, 404, ... */

	response = string_sep (&reply, "\r\n");

	if (!response)
		return FALSE;

	/*    */     string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the headers */
	http_headers_parse (reply, &req->headers);

	/* Hrm, this treats 3xx requests as errors, and shouldnt */
	if (code >= 200 && code <= 299)
		return TRUE;

	/* request error: could blacklist the server in recv_callback */
	http_request_close (req, code);

	return FALSE;
}

static void read_headers (int fd, input_id id, Connection *c)
{
	HttpRequest *req;
	FDBuf       *buf;
	char        *response;
	char        *encoding;
	char        *len_str;
	int          n;

	req = get_request (c);
	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\r\n\r\n")) < 0)
	{
		TRACE (("error reading from %s: %s", net_peer_ip (c->fd),
		        GIFT_STRERROR ()));
		http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	if (!parse_server_response (response, req))
		return;

	input_remove (id);

	encoding = dataset_lookupstr (req->headers, "transfer-encoding");

	if (encoding && !strcasecmp (encoding, "chunked"))
	{
		input_add (fd, c, INPUT_READ,
		           (InputCallback) read_chunked_header, TIMEOUT_DEF);
		return;
	}

	if (!(len_str = dataset_lookupstr (req->headers, "content-length")))
	{
		TRACE (("error: didnt find content length str for %s", req->host));
		http_request_close (req, -1);
		return;
	}

	if ((req->size = ATOI (len_str)) == -1)
	{
		TRACE (("bad size (%s) in content length field for %s", len_str,
		        req->host));
		http_request_close (req, -1);
		return;
	}

	input_add (fd, c, INPUT_READ,
	           (InputCallback) read_file, TIMEOUT_DEF);
}

static int send_request (HttpRequest *req)
{
	char     req_buf[256];
	Dataset *headers = NULL;
	int      ret;

	if (!req->path || string_isempty (req->path))
	    snprintf (req_buf, sizeof (req_buf), "/?%s", req->request);
	else
		snprintf (req_buf, sizeof (req_buf), "/%s?%s", req->path, req->request);

	dataset_insertstr (&headers, "host", req->host); /* required by HTTP/1.1 */
	dataset_insertstr (&headers, "user-agent", PACKAGE " " VERSION);

	if (req->add_header_func && !(*req->add_header_func) (req, &headers))
	{
		/* Hmm, this is our error, what should the error code be */
		http_request_close (req, -1);
		return -1;
	}

	ret = http_send (req->c, "GET", req_buf, headers);

	dataset_clear (headers);

	return ret;
}

void handle_http_request (int fd, input_id id, Connection *c)
{
	HttpRequest *req;

	req = get_request (c);

	if (send_request (req) <= 0)
	{
		TRACE (("send failed: %s", GIFT_STRERROR ()));
		http_request_close (req, -1);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) read_headers, TIMEOUT_DEF);
}
