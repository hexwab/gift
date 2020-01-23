/*
 * $Id: http_request.h,v 1.7 2003/07/01 10:51:42 hipnod Exp $
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

#ifndef __GIFT_HTTP_REQUEST_H
#define __GIFT_HTTP_REQUEST_H

/*****************************************************************************/

struct _http_request;

typedef void (*HttpCloseFunc)     (struct _http_request *req, int error_code);

typedef int  (*HttpReceiveFunc)   (struct _http_request *req, char *data,
                                   int len);
typedef int  (*HttpAddHeaderFunc) (struct _http_request *req,
                                   Dataset **headers);

typedef struct _http_request
{
	char             *host;
	char             *path;
	char             *request;

	TCPC             *c;
	Dataset          *headers;
	timer_id          timeout;

	/* amount of data to expect:
	 * (1) in the chunk, when Transfer-Encoding: Chunked is used
	 * (2) in the body, from the Content-Length: header */
	unsigned long     size;
	size_t            max_len;
	size_t            recvd_len;

	HttpReceiveFunc   recv_func;
	HttpAddHeaderFunc add_header_func;
	HttpCloseFunc     close_req_func;
	void             *data;

} HttpRequest;

/*****************************************************************************/

HttpRequest    *http_request_new         (char *host, char *path, char *request);
void            http_request_close       (HttpRequest *req, int code);

/*****************************************************************************/

void            http_request_set_conn    (HttpRequest *req, TCPC *c);
void            http_request_set_timeout (HttpRequest *req, time_t interval);
void            http_request_set_max_len (HttpRequest *req, size_t max_len);

/*****************************************************************************/

void    http_request_handle (int fd, input_id id, TCPC *c);

/*****************************************************************************/

#endif /* __GIFT_HTTP_REQUEST_H */
