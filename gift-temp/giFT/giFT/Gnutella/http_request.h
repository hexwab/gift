/*
 * $Id: http_request.h,v 1.3 2003/03/20 05:01:10 rossta Exp $
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

	Connection       *c;
	Dataset          *headers;

	/* amount of data to expect:
	 * (1) in the chunk, when Transfer-Encoding: Chunked is used
	 * (2) in the body, from the Content-Length: header */
	unsigned long     size;

	HttpReceiveFunc   recv_func;
	HttpAddHeaderFunc add_header_func;
	HttpCloseFunc     close_req_func;
	void             *data;

} HttpRequest;

/*****************************************************************************/

HttpRequest    *http_request_new   (char *host, char *path, char *request);
void            http_request_free  (HttpRequest *req);
void            http_request_close (HttpRequest *req, int code);

/*****************************************************************************/

void    handle_http_request (int fd, input_id id, Connection *c);

/*****************************************************************************/

#endif /* __GIFT_HTTP_REQUEST_H */
