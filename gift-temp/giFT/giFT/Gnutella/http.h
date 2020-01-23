/*
 * $Id: http.h,v 1.4 2003/03/20 05:01:10 rossta Exp $
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

#ifndef __GIFT_HTTP_H
#define __GIFT_HTTP_H

/*****************************************************************************/

/* rename this to HTTP_Transfer */
struct _gt_transfer;
struct _protocol;

typedef char *(*HTTP_LocalizeCB) (struct _gt_transfer *xfer, char *s_path,
                                  int *authorized);
typedef struct _http_protocol
{
	struct _protocol    *p;

	HTTP_LocalizeCB      localize_cb;  /* protocol-specific function
	                                    * to map http requests for files to
	                                    * local files */
} HTTP_Protocol;

/*****************************************************************************/

HTTP_Protocol *http_protocol_new        (struct _protocol *);
void           http_protocol_register   (HTTP_Protocol *http);
void           http_protocol_unregister (HTTP_Protocol *http);
HTTP_Protocol *http_protocol_get        (struct _protocol *);

/*****************************************************************************/

#endif /* __GIFT_HTTP_H */
