/*
 * $Id: http_request.c,v 1.15 2003/07/01 10:51:42 hipnod Exp $
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

static void decode_chunked_data (int fd, input_id id, TCPC *c);
static void read_chunked_header (int fd, input_id id, TCPC *c);

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

	if (!(http_request = MALLOC (sizeof (HttpRequest))))
		return NULL;

	http_request->host    = STRDUP (host);
	http_request->path    = STRDUP (path);
	http_request->request = STRDUP (request);
	http_request->timeout = 0;

	return http_request;
}

static void http_request_free (HttpRequest *req)
{
	if (!req)
		return;

	dataset_clear (req->headers);

	free (req->host);
	free (req->path);
	free (req->request);

	free (req);
}

void http_request_close (HttpRequest *req, int error_code)
{
	/* notify the callback */
	if (req->close_req_func)
		(*req->close_req_func) (req, error_code);

	if (req->c)
		tcp_close (req->c);

	timer_remove_zero (&req->timeout);

	http_request_free (req);
}

/*****************************************************************************/

static int request_timeout (HttpRequest *req)
{
	GT->DBGFN (GT, "request to %s timed out", req->host);
	http_request_close (req, -1);
	return FALSE;
}

void http_request_set_timeout (HttpRequest *req, time_t time)
{
	if (!req)
		return;

	if (req->timeout)
		timer_remove (req->timeout);

	req->timeout = timer_add (time, (TimerCallback)request_timeout, req);
}

void http_request_set_conn (HttpRequest *req, TCPC *c)
{
	assert (c->udata == NULL);
	assert (req->c == NULL);

	req->c = c;
	c->udata = req;
}

void http_request_set_max_len (HttpRequest *req, size_t max_len)
{
	req->max_len = max_len;
}

/*****************************************************************************/

static int write_data (HttpRequest *req, char *data, size_t len)
{
	if (!req)
		return FALSE;

	req->recvd_len += len;

	/* check if we overflowed the max length the user wants to receive */
	if (req->max_len > 0 && req->recvd_len > req->max_len)
	{
		GT->DBGFN (GT, "%s sent %lu bytes overflowing max length of %lu",
		           req->host, req->recvd_len, req->max_len);
		http_request_close (req, -1);
		return FALSE;
	}

	/* send the data to the callback */
	if (req->recv_func && !(*req->recv_func) (req, data, len))
	{
		http_request_close (req, -1);
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static void write_header (ds_data_t *key, ds_data_t *value, String *s)
{
	char *header = key->data;
	char *field  = value->data;

	string_appendf (s, "%s: %s\r\n", header, field);
}

static int http_send (TCPC *c, char *command, char *request,
                      Dataset *headers)
{
	String      *s;
	int          ret;

	if (!command || !request)
		return -1;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return -1;

	string_appendf (s, "%s %s HTTP/1.1\r\n", command, request);

	dataset_foreach (headers, DS_FOREACH(write_header), s);
	string_append (s, "\r\n");

	GT->dbgsock (GT, c, "<http_request.c> sending:\n%s", s->str);

	ret = net_send (c->fd, s->str, s->len);
	string_free (s);

	return ret;
}

static HttpRequest *get_request (TCPC *c)
{
	return c->udata;
}

/*****************************************************************************/

static void decode_chunked_data (int fd, input_id id, TCPC *c)
{
	HttpRequest *req;
	FDBuf       *buf;
	char        *data;
	int          data_len = 0;
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
		GT->DBGFN (GT, "error on host %s: %s", req->host, GIFT_NETERROR ());
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

static void read_chunked_header (int fd, input_id id, TCPC *c)
{
	HttpRequest *req;
	FDBuf       *buf;
	char        *response;
	int          n;

	req = get_request (c);

	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		GT->DBGFN (GT, "error on %s: %s", req->host, GIFT_NETERROR ());
		http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, NULL);
	fdbuf_release (buf);

	/* read the chunk size, its a hexadecimal integer */
	req->size = strtoul (response, NULL, 16);
	GT->DBGFN (GT, "server sent chunk size of %lu", req->size);

	if (req->size == ULONG_MAX)
	{
		GT->DBGFN (GT, "overflow reading chunk size: %s", GIFT_STRERROR ());
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

/* read the amount of data specified by Content-Length: */
static void read_file (int fd, input_id id, TCPC *c)
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
		GT->DBGFN (GT, "error from %s: %s", req->host, GIFT_NETERROR ());
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

/* callback to read when no Content-Length: header is provided */
static void read_until_eof (int fd, input_id id, TCPC *c)
{
	char          data[RW_BUFFER];
	int           n;
	HttpRequest  *req;

	req = get_request (c);

	if ((n = tcp_recv (c, data, sizeof (data) - 1)) < 0)
	{
		GT->DBGFN (GT, "error from %s: %s", req->host, GIFT_NETERROR());
		http_request_close (req, -1);
		return;
	}

	/* terminate the buffer */
	data[n] = 0;

	if (n == 0)
	{
		http_request_close (req, 200);
		return;
	}

	if (!write_data (req, data, n))
		return;
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

static void read_headers (int fd, input_id id, TCPC *c)
{
	HttpRequest *req;
	FDBuf       *buf;
	char        *response;
	size_t       response_len = 0;
	char        *encoding;
	char        *len_str;
	int          n;

	req = get_request (c);
	buf = tcp_readbuf (c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		GT->DBGFN (GT, "error reading from %s: %s", net_peer_ip (c->fd), 
		           GIFT_NETERROR ());
		http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);

	if (!http_headers_terminated (response, response_len))
		return;

	fdbuf_release (buf);

	if (!parse_server_response (response, req))
		return;

	input_remove (id);

	encoding = dataset_lookupstr (req->headers, "transfer-encoding");

	if (encoding && !strcasecmp (encoding, "chunked"))
	{
		input_add (fd, c, INPUT_READ,
		           (InputCallback)read_chunked_header, TIMEOUT_DEF);
		return;
	}

	if (!(len_str = dataset_lookupstr (req->headers, "content-length")))
	{
		GT->warn (GT, "no Content-Length header from %s", req->host);
		input_add (fd, c, INPUT_READ,
		           (InputCallback)read_until_eof, TIMEOUT_DEF);
		return;
	}

	if ((req->size = ATOI (len_str)) == -1)
	{
		GT->DBGFN (GT, "bad size (%s) in content length field for %s", 
		           len_str, req->host);
		http_request_close (req, -1);
		return;
	}

	input_add (fd, c, INPUT_READ,
	           (InputCallback)read_file, TIMEOUT_DEF);
}

static int send_request (HttpRequest *req)
{
	Dataset *headers = NULL;
	String  *s;
	int      ret;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return -1;

	string_appendf (s, "http://%s/", req->host);

	if (!req->path || string_isempty (req->path))
		string_appendf (s, "?%s", req->request);
	else
		string_appendf (s, "%s?%s", req->path, req->request);

	dataset_insertstr (&headers, "host", req->host); /* required by HTTP/1.1 */
	dataset_insertstr (&headers, "user-agent", gt_version ());

	if (req->add_header_func && !(*req->add_header_func) (req, &headers))
	{
		/* Hmm, this is our error, what should the error code be */
		http_request_close (req, -1);
		dataset_clear (headers);
		string_free (s);
		return -1;
	}

	ret = http_send (req->c, "GET", s->str, headers);

	dataset_clear (headers);
	string_free (s);

	return ret;
}

void http_request_handle (int fd, input_id id, TCPC *c)
{
	HttpRequest *req;

	req = get_request (c);

	if (send_request (req) <= 0)
	{
		GT->DBGFN (GT, "send failed: %s", GIFT_NETERROR());
		http_request_close (req, -1);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback) read_headers, TIMEOUT_DEF);
}
