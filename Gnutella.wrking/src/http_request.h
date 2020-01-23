/*
 * $Id: http_request.h,v 1.11 2004/03/24 06:20:48 hipnod Exp $
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

#ifndef GIFT_GT_HTTP_REQUEST_H_
#define GIFT_GT_HTTP_REQUEST_H_

/*****************************************************************************/

struct gt_http_request;

/*****************************************************************************/

typedef void (*HttpCloseFunc)     (struct gt_http_request *req, int error_code);

typedef BOOL (*HttpReceiveFunc)   (struct gt_http_request *req, char *data,
                                   size_t len);
typedef BOOL (*HttpAddHeaderFunc) (struct gt_http_request *req,
                                   Dataset **headers);
typedef BOOL (*HttpRedirectFunc)  (struct gt_http_request *req,
                                   const char *host, const char *path);

/*****************************************************************************/

typedef struct gt_http_request
{
	char             *host;
	char             *path;
	char             *request;
	char             *proxy;

	TCPC             *c;
	Dataset          *headers;
	timer_id          timeout;

	/* amount of data to expect:
	 * (1) in the chunk, when Transfer-Encoding: Chunked is used
	 * (2) in the body, from the Content-Length: header */
	unsigned long     size;
	size_t            max_len;
	size_t            recvd_len;

	/* number of redirects tried already */
	int               redirects;

	HttpReceiveFunc   recv_func;
	HttpAddHeaderFunc add_header_func;
	HttpCloseFunc     close_req_func;
	HttpRedirectFunc  redirect_func;
	void             *data;

} HttpRequest;

/*****************************************************************************/

HttpRequest    *gt_http_request_new         (const char *url,
                                             const char *request);
void            gt_http_request_close       (HttpRequest *req, int code);
BOOL            gt_http_url_parse           (char *url, char **r_host,
                                             char **r_path);

/*****************************************************************************/

void            gt_http_request_set_conn    (HttpRequest *req, TCPC *c);
void            gt_http_request_set_proxy   (HttpRequest *req, const char *proxy);
void            gt_http_request_set_timeout (HttpRequest *req, time_t interval);
void            gt_http_request_set_max_len (HttpRequest *req, size_t max_len);

/*****************************************************************************/

void            gt_http_request_handle      (int fd, input_id id, TCPC *c);

/*****************************************************************************/

#endif /* GIFT_GT_HTTP_REQUEST_H_ */
