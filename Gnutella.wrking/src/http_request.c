/*
 * $Id: http_request.c,v 1.24 2004/03/24 06:20:48 hipnod Exp $
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
#include "gt_version.h"

#include "gt_accept.h"
#include "gt_web_cache.h"

#include "http_request.h"

/*****************************************************************************/

#define MAX_REDIRECTS      (5)

/*****************************************************************************/

static void decode_chunked_data (int fd, input_id id, TCPC *c);
static void read_chunked_header (int fd, input_id id, TCPC *c);

/*****************************************************************************/

/*
 * Dummy callbacks
 */

static void dummy_close (HttpRequest *r, int code)
{
	return;
}

static BOOL dummy_recv (HttpRequest *r, char *d, size_t l)
{
	return TRUE;
}

static BOOL dummy_add_header (HttpRequest *r, Dataset **d)
{
	return TRUE;
}

static BOOL dummy_redirect (HttpRequest *r, const char *h, const char *p)
{
	return TRUE;
}

/*****************************************************************************/

BOOL gt_http_url_parse (char *value, char **r_host, char **r_path)
{
	char *host_name;

	if (r_host)
		*r_host = NULL;
	if (r_path)
		*r_path = NULL;

	string_sep (&value, "http://");

	/* divide the url in two parts */
	host_name = string_sep (&value, "/");

	if (r_host)
		*r_host = host_name;

	if (r_path)
		*r_path = STRING_NOTNULL (value);

	if (!host_name || host_name[0] == 0)
		return FALSE;

	return TRUE;
}

static void setup_dummy_functbl (HttpRequest *r)
{
	r->close_req_func  = dummy_close;
	r->recv_func       = dummy_recv;
	r->add_header_func = dummy_add_header;
	r->redirect_func   = dummy_redirect;
}

HttpRequest *gt_http_request_new (const char *url, const char *request)
{
	HttpRequest *req;
	char        *dup;
	char        *host;
	char        *path;

	if (!(dup = STRDUP (url)))
		return NULL;

	if (!gt_http_url_parse (dup, &host, &path) ||
		!(req = MALLOC (sizeof (HttpRequest))))
	{
		free (dup);
		return NULL;
	}

	req->host      = STRDUP (host);
	req->path      = STRDUP (path);
	req->request   = STRDUP (request);
	req->timeout   = 0;
	req->redirects = 0;
	req->headers   = NULL;

	/* setup functbl */
	setup_dummy_functbl (req);

	free (dup);

	return req;
}

static void gt_http_request_free (HttpRequest *req)
{
	if (!req)
		return;

	dataset_clear (req->headers);

	free (req->host);
	free (req->path);
	free (req->request);

	free (req);
}

void gt_http_request_close (HttpRequest *req, int error_code)
{
	/* notify the callback */
	req->close_req_func (req, error_code);

	if (req->c)
		tcp_close (req->c);

	timer_remove_zero (&req->timeout);

	gt_http_request_free (req);
}

/*****************************************************************************/

static BOOL request_timeout (HttpRequest *req)
{
	GT->DBGFN (GT, "request to %s timed out", req->host);
	gt_http_request_close (req, -1);
	return FALSE;
}

void gt_http_request_set_timeout (HttpRequest *req, time_t time)
{
	if (!req)
		return;

	if (req->timeout)
		timer_remove (req->timeout);

	req->timeout = timer_add (time, (TimerCallback)request_timeout, req);
}

void gt_http_request_set_proxy (HttpRequest *req, const char *proxy)
{
	free (req->proxy);
	req->proxy = NULL;

	if (!proxy)
		return;

	req->proxy = STRDUP (proxy);
}

void gt_http_request_set_conn (HttpRequest *req, TCPC *c)
{
	assert (c->udata == NULL);
	assert (req->c == NULL);

	req->c = c;
	c->udata = req;
}

void gt_http_request_set_max_len (HttpRequest *req, size_t max_len)
{
	req->max_len = max_len;
}

/*****************************************************************************/

static BOOL write_data (HttpRequest *req, char *data, size_t len)
{
	if (!req)
		return FALSE;

	req->recvd_len += len;

	/* check if we overflowed the max length the user wants to receive */
	if (req->max_len > 0 && req->recvd_len > req->max_len)
	{
		GT->DBGFN (GT, "%s sent %lu bytes overflowing max length of %lu",
		           req->host, req->recvd_len, req->max_len);
		gt_http_request_close (req, -1);
		return FALSE;
	}

	/* send the data to the listener */
	if (req->recv_func (req, data, len) == FALSE)
	{
		gt_http_request_close (req, -1);
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

	GT->DBGSOCK (GT, c, "<http_request.c> sending:\n%s", s->str);

	ret = tcp_send (c, s->str, s->len);
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
		gt_http_request_close (req, 200);
		return;
	}

	buf = tcp_readbuf (c);

	if ((n = fdbuf_fill (buf, req->size)) < 0)
	{
		GT->DBGFN (GT, "error on host %s: %s", req->host, GIFT_NETERROR ());
		gt_http_request_close (req, -1);
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
	           (InputCallback)read_chunked_header, TIMEOUT_DEF);
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
		gt_http_request_close (req, -1);
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
		gt_http_request_close (req, -1);
		return;
	}

	if (req->size == 0)
	{
		/* ok, done */
		if (!write_data (req, NULL, 0))
			return;

		/* there could be a CRLF at the end. should we read it?
		 * To avoid screwing up persistent http, yes.. */
		gt_http_request_close (req, 200);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)decode_chunked_data, TIMEOUT_DEF);
}

/* read the amount of data specified by Content-Length: */
static void read_file (int fd, input_id id, TCPC *c)
{
	HttpRequest   *req;
	FDBuf         *buf;
	int            n;
	size_t         len;
	unsigned char *data;

	req = get_request (c);

	if (!req->size)
	{
		gt_http_request_close (req, 200);
		return;
	}

	buf = tcp_readbuf (c);

	if ((n = fdbuf_fill (buf, req->size)) < 0)
	{
		GT->DBGFN (GT, "error from %s: %s", req->host, GIFT_NETERROR ());
		gt_http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	data = fdbuf_data (buf, &len);
	fdbuf_release (buf);

	if (!write_data (req, data, len))
		return;

	/*
	 * We've read all the data, the total length of the request being provided
	 * by fdbuf_fill().  Now send the closing notification to our callback.
	 */
	if (!write_data (req, NULL, 0))
		return;

	/* success */
	gt_http_request_close (req, 200);
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
		gt_http_request_close (req, -1);
		return;
	}

	/* terminate the buffer */
	data[n] = 0;

	if (n == 0)
	{
		/* signal to the listener that EOF was reached */
		if (!write_data (req, NULL, 0))
			return;

		gt_http_request_close (req, 200);
		return;
	}

	if (!write_data (req, data, n))
		return;
}

static void reset_request (HttpRequest *req, const char *host,
                           const char *path)
{
	free (req->host);
	free (req->path);
	req->host = STRDUP (host);
	req->path = STRDUP (path);

	dataset_clear (req->headers);
	req->headers = NULL;
}

/*
 * This will do a limited redirect on the same connection.
 * One bug is it doesn't care if the Location header posts a different port,
 */
static void handle_redirect (HttpRequest *req, int code)
{
	char *new_host;
	char *new_path;
	char *location;

	/* make sure the Location: header points to the same host */
	location = dataset_lookupstr (req->headers, "location");

	/* butchers Location header, but it will be freed soon anyway */
	if (!location ||
	    !gt_http_url_parse (location, &new_host, &new_path))
	{
		gt_http_request_close (req, code);
		return;
	}

	assert (new_host != NULL);

	if (++req->redirects >= MAX_REDIRECTS)
	{
		GT->DBGSOCK (GT, req->c, "Too many redirects");
		gt_http_request_close (req, code);
		return;
	}

	/*
	 * Let the caller know we're redirecting so it can reset it's ancilliary
	 * data.
	 */
	if (req->redirect_func (req, new_host, new_path) == FALSE)
	{
		gt_http_request_close (req, code);
		return;
	}

	/* setup the new request */
	reset_request (req, new_host, new_path);

	/* restart the request */
	input_remove_all (req->c->fd);
	input_add (req->c->fd, req->c, INPUT_WRITE,
	           (InputCallback)gt_http_request_handle, TIMEOUT_DEF);
}

static BOOL parse_server_response (char *reply, HttpRequest *req)
{
	char *response;
	int   code;    /* 200, 404, ... */

	response = string_sep (&reply, "\r\n");

	if (!response)
		return FALSE;

	/*    */     string_sep (&response, " ");  /* shift past HTTP/1.1 */
	code = ATOI (string_sep (&response, " ")); /* shift past 200 */

	/* parse the headers */
	gt_http_header_parse (reply, &req->headers);

	if (code >= 200 && code <= 299)
		return TRUE;

	/* redirection */
	if (code >= 300 && code <= 399)
	{
		handle_redirect (req, code);
		return FALSE; /* stop this request */
	}

	/* request error: could blacklist the server in recv_callback */
	GT->DBGFN (GT, "error parsing response from %s, closing", req->host);
	gt_http_request_close (req, code);

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
		gt_http_request_close (req, -1);
		return;
	}

	if (n > 0)
		return;

	response = fdbuf_data (buf, &response_len);

	if (response_len >= req->max_len)
	{
		GT->DBGFN (GT, "headers too large(%lu)", (long)response_len);
		gt_http_request_close (req, -1);
	}

	if (!gt_http_header_terminated (response, response_len))
		return;

	fdbuf_release (buf);
	GT->DBGFN (GT, "response=\n%s", response);

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

	req->size = ATOUL (len_str);

	if (req->max_len > 0 && req->size >= req->max_len)
	{
		GT->DBGFN (GT, "bad size (%s) in content length field for %s",
		           len_str, req->host);
		gt_http_request_close (req, -1);
		return;
	}

	input_add (fd, c, INPUT_READ,
	           (InputCallback)read_file, TIMEOUT_DEF);
}

/*
 * Determine the part after the GET.  If proxied, this need to be a complete
 * URL, and otherwise should be a simple path.
 */
static void append_request_line (String *s, HttpRequest *req)
{
	if (req->proxy)
		string_appendf (s, "http://%s", req->host);

	string_appendf (s, "/%s", STRING_NOTNULL(req->path));
}

static int send_request (HttpRequest *req)
{
	Dataset *headers = NULL;
	String  *s;
	int      ret;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
		return -1;

	append_request_line (s, req);

	if (!string_isempty (req->request))
		string_appendf (s, "?%s", req->request);

	dataset_insertstr (&headers, "Host", req->host); /* required by HTTP/1.1 */
	dataset_insertstr (&headers, "User-Agent", gt_version ());

	if (req->add_header_func (req, &headers) == FALSE)
	{
		/* Hmm, this is our error, what should the error code be */
		gt_http_request_close (req, -1);
		dataset_clear (headers);
		string_free (s);
		return -1;
	}

	ret = http_send (req->c, "GET", s->str, headers);

	dataset_clear (headers);
	string_free (s);

	return ret;
}

void gt_http_request_handle (int fd, input_id id, TCPC *c)
{
	HttpRequest *req;

	req = get_request (c);

	if (send_request (req) <= 0)
	{
		GT->DBGFN (GT, "send failed: %s", GIFT_NETERROR());
		gt_http_request_close (req, -1);
		return;
	}

	input_remove (id);
	input_add (fd, c, INPUT_READ,
	           (InputCallback)read_headers, TIMEOUT_DEF);
}
