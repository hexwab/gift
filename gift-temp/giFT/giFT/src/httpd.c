/*
 * $Id: httpd.c,v 1.13 2003/04/25 08:12:43 jasta Exp $
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

#include "config.h"

#include "gift.h"

#include "file.h"
#include "tcpc.h"
#include "fdbuf.h"

#include "httpd.h"

/*****************************************************************************/

typedef struct
{
	char     *method;                  /* GET|POST|... */
	char     *request;                 /* /foo/bar.baz */
	uint16_t  version;                 /* 0x0101 or 0x0100 */
	Dataset  *params;                  /* X-Foo: Bar */
} HTTPRequest;

typedef struct
{
	uint16_t  version;
	int       code;                    /* 200, 404, ... */
	Dataset  *params;
} HTTPResponse;

typedef struct
{
	TCPC         *c;
	input_id      send_id;

	HTTPRequest  *req;
	HTTPResponse *resp;

	FILE         *reqf;
	char         *reqpath;             /* /home/jasta/.giFT/httpd/foo/bar.baz */
} HTTPC;

/*****************************************************************************/

static void free_request (HTTPRequest *req);
static void free_response (HTTPResponse *resp);

/*****************************************************************************/

static char *get_codestr (int code)
{
	char *str;

	switch (code)
	{
	 case 200: str = "OK";                 break;
	 case 404: str = "Document Not Found"; break;
	 default:  abort ();                   break;
	}

	return str;
}

/*****************************************************************************/

static HTTPC *httpc_new (TCPC *c)
{
	HTTPC *hc;

	if (!(hc = MALLOC (sizeof (HTTPC))))
		return NULL;

	hc->c = c;

	return hc;
}

static void httpc_free (HTTPC *hc)
{
	if (!hc)
		return;

	free_request (hc->req);
	free_response (hc->resp);

	if (hc->reqf)
		fclose (hc->reqf);

	free (hc->reqpath);
	free (hc);
}

static void httpc_close (HTTPC *hc)
{
	if (!hc)
		return;

	input_remove (hc->send_id);
	tcp_close (hc->c);
	httpc_free (hc);
}

/*****************************************************************************/

static int parse_params (HTTPRequest *req, char *data)
{
	char *key;
	char *line;

	if (!req || !data)
		return FALSE;

	while ((line = string_sep_set (&data, "\r\n")))
	{
		key = string_sep (&line, ": ");

		/* check line again as it was moved to the value of the current key */
		if (key && line)
			dataset_insertstr (&req->params, key, line);
	}

	return TRUE;
}

static HTTPRequest *new_request (char *data, size_t len)
{
	HTTPRequest *req;
	char        *verstr;

	if (!(req = MALLOC (sizeof (HTTPRequest))))
		return NULL;

	req->method  = STRDUP (string_sep (&data, " "));
	req->request = STRDUP (string_sep_set (&data, " \r\n"));
	verstr       = string_sep_set (&data, " \r\n");

	/* detect an incomplete request */
	if (!req->method || !req->request || !verstr)
	{
		free (req);
		return NULL;
	}

	if ((verstr = string_sep (&verstr, "/")))
	{
		char *major;
		char *minor;

		major = string_sep (&verstr, ".");
		minor = verstr;

		req->version = ((ATOI(major) & 0xff) << 8) | (ATOI(minor) & 0xff);
	}

	parse_params (req, data);

	return req;
}

static void free_request (HTTPRequest *req)
{
	if (!req)
		return;

	dataset_clear (req->params);

	free (req->method);
	free (req->request);
	free (req);
}

/*****************************************************************************/

static HTTPResponse *new_response (uint8_t vermajor, uint8_t verminor, int code)
{
	HTTPResponse *resp;

	if (!(resp = MALLOC (sizeof (HTTPResponse))))
		return NULL;

	resp->version = (vermajor << 8) | (verminor);
	resp->code = code;

	return resp;
}

static void free_response (HTTPResponse *resp)
{
	if (!resp)
		return;

	dataset_clear (resp->params);
	free (resp);
}

static HTTPResponse *get_response (HTTPC *hc)
{
	HTTPResponse *resp;
	struct stat   st;

	assert (hc->reqf == NULL);
	assert (hc->reqpath != NULL);

	if (!(resp = new_response (0x01, 0x00, 404)))
		return NULL;

	/* we are going to use a 404 page, so give the client some heads up */
	dataset_insertstr (&resp->params, "Content-Type", "text/html");

	if (!file_stat (hc->reqpath, &st))
		return resp;

	resp->code = 200;

	/* now that we know the file size, set it in the headers...we aren't going
	 * to provide that luxury for the 404 page*/
	dataset_insertstr (&resp->params, "Content-Length",
	                   stringf ("%lu", (unsigned long)st.st_size));

	return resp;
}

static int add_param (Dataset *d, DatasetNode *node, String *msg)
{
	string_appendf (msg, "%s: %s\r\n", node->key, node->value);
	return FALSE;
}

static int send_response (HTTPC *hc)
{
	String       *msg;
	HTTPResponse *resp;
	int           ret;

	if (!(msg = string_new (NULL, 0, 0, TRUE)))
		return -1;

	resp = hc->resp;
	assert (resp != NULL);

	/* add reply header */
	string_appendf (msg, "HTTP/%i.%i %i %s\r\n",
	                (resp->version >> 8) & 0xff, resp->version & 0xff,
	                resp->code, get_codestr (resp->code));

	/* add params */
	dataset_foreach (resp->params, DATASET_FOREACH(add_param), msg);

	/* terminate */
	string_append (msg, "\r\n");

	/* send and cleanup */
	ret = tcp_write (hc->c, msg->str, msg->len);

	string_free (msg);

	return ret;
}

/*****************************************************************************/

static void set_request (HTTPRequest *req, char *request)
{
	free (req->request);
	req->request = STRDUP (request);
}

static void clean_request (HTTPC *hc)
{
	HTTPRequest *req;
	char        *sec;
	char        *path;

	req = hc->req;
	assert (req != NULL);

	/* apply special HTTP logic */
	if (!strcmp (req->request, "/"))
		set_request (req, "/index.html");

	/* make sure they're not trying to pull something sneaky on us */
	if (!(sec = file_secure_path (req->request)))
		return;

	/* determine the local pathname that was actually requested */
	if ((path = gift_conf_path ("httpd/%s", sec)))
		hc->reqpath = file_host_path (path);

	free (sec);
}

static FILE *get_404_page (char *path)
{
	FILE *f;

	/* file already exists, return it */
	if ((f = fopen (path, "r")))
		return f;

	if (!(f = fopen (path, "w+")))
		return NULL;

	fprintf (f,
	         "<html>\n"
	         " <head><title>Document Not Found</title></head>\n"
	         " <body>\n"
	         "  <h1>Document Not Found</h1>\n"
	         "  <p>The requested resource could not be found.  Sorry.</p>\n"
	         " </body>\n"
	         "</html>\n");

	/* move back to the beginning for reading */
	fseek (f, 0, SEEK_SET);

	return f;
}

static int open_file (HTTPC *hc)
{
	if (hc->reqf)
		return TRUE;

	assert (hc->reqpath != NULL);

	if (!(hc->reqf = fopen (hc->reqpath, "r")))
	{
		if (!(hc->reqf = get_404_page (gift_conf_path ("httpd/404.html"))))
			return FALSE;
	}

	return TRUE;
}

static void send_file (int fd, input_id id, HTTPC *hc)
{
	unsigned char buf[RW_BUFFER];
	size_t        read_len;
	int           sent_len;

	if (fd < 0 || id == 0 || !open_file (hc))
	{
		httpc_close (hc);
		return;
	}

	if (!(read_len = fread (buf, sizeof (char), sizeof (buf), hc->reqf)))
	{
		if (ferror (hc->reqf))
		{
			GIFT_ERROR (("unable to read from %s: %s", hc->reqpath,
			             GIFT_STRERROR()));
		}

		httpc_close (hc);
		return;
	}

	if ((sent_len = tcp_send (hc->c, buf, read_len)) < read_len)
	{
		httpc_close (hc);
		return;
	}
}

static int handle_head (HTTPC *hc)
{
	assert (hc->resp == NULL);

	if (!(hc->resp = get_response (hc)))
		return FALSE;

	send_response (hc);

	/* abuse the error condition so that the socket will be closed */
	return FALSE;
}

static int handle_get (HTTPC *hc)
{
	handle_head (hc);

	/* evil hack */
	if (!hc->resp)
		return FALSE;

	hc->send_id = input_add (hc->c->fd, hc, INPUT_WRITE,
	                         (InputCallback)send_file, TIMEOUT_DEF);

	return TRUE;
}

static int handle_request (HTTPC *hc)
{
	int ret = FALSE;

	assert (hc != NULL);

	if (!hc->req)
		return FALSE;

	/* handle any special logic such as changing / to /index.html so that
	 * we don't have to worry about it for each method */
	clean_request (hc);

	if (!strcmp (hc->req->method, "GET"))
		ret = handle_get (hc);
	else if (!strcmp (hc->req->method, "HEAD"))
		ret = handle_head (hc);

	return ret;
}

/*****************************************************************************/

static int check_sentinel (char *data, size_t len)
{
	int cnt;

	assert (len > 0);
	len--;

	for (cnt = 0; len > 0 && cnt < 2; cnt++)
	{
		if (data[len--] != '\n')
			break;

		/* treat CRLF as LF */
		if (data[len] == '\r')
			len--;
	}

	return (cnt == 2);
}

static void handle_http (int fd, input_id id, HTTPC *hc)
{
	FDBuf  *buf;
	char   *data;
	size_t  data_len = 0;
	int     n;

	buf = tcp_readbuf (hc->c);

	if ((n = fdbuf_delim (buf, "\n")) < 0)
	{
		httpc_close (hc);
		return;
	}

	if (n > 0)
		return;

	data = (char *)fdbuf_data (buf, &data_len);

	/* look for two trailing \n's optionally preceeded by \r characters */
	if (!check_sentinel (data, data_len))
		return;

	fdbuf_release (buf);

	assert (hc->req == NULL);

	hc->req = new_request (data, data_len);

	if (!handle_request (hc))
		httpc_close (hc);
}

static void handle_incoming (int fd, input_id id, HTTPD *http)
{
	TCPC  *accept;
	HTTPC *hc;

	if (!(accept = tcp_accept (http->c, FALSE)))
		return;

	if (!(hc = httpc_new (accept)))
	{
		tcp_close (accept);
		return;
	}

	input_add (hc->c->fd, hc, INPUT_READ,
	           (InputCallback)handle_http, TIMEOUT_DEF);
}

/*****************************************************************************/

HTTPD *httpd_start (in_port_t port)
{
	HTTPD *httpd;

	if (!(httpd = MALLOC (sizeof (HTTPD))))
		return NULL;

	if (!(httpd->c = tcp_bind (port, FALSE)))
	{
		httpd_stop (httpd);
		return FALSE;
	}

	input_add (httpd->c->fd, httpd, INPUT_READ,
	           (InputCallback)handle_incoming, 0);

	return httpd;
}

void httpd_stop (HTTPD *httpd)
{
	if (!httpd)
		return;

	tcp_close_null (&httpd->c);
	free (httpd);
}

/*****************************************************************************/

#if 0
static char *genpage (char *request)
{
	char *str;

	str = stringf_dup ("<h1>Hi.</h1>\n");

	return str;
}

static char *handle_hit (char *request)
{
	char *page = NULL;

	if (!strcmp (request, "/index.html"))
		page = genpage (request);

	return page;
}

int main (int argc, char **argv)
{
	HTTPD *http;

	in_port_t port = 8080;

	libgift_init ("httpd", GLOG_STDOUT, NULL);

	if (!(http = httpd_start (port)))
	{
		GIFT_ERROR (("unable to start http server on port %hu: %s",
		             port, GIFT_NETERROR()));
		return 1;
	}

	httpd_handler (http, (HTTPHandler)handle_hit);

	event_loop ();

	httpd_stop (http);

	libgift_finish ();

	return 0;
}
#endif
