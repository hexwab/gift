/*
 * $Id: http.h,v 1.6 2003/06/01 09:34:48 hipnod Exp $
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
struct gt_transfer;
struct protocol;

typedef char *(*HTTP_LocalizeCB) (struct gt_transfer *xfer, char *s_path,
                                  int *authorized);
typedef struct _http_protocol
{
	struct protocol     *p;

	HTTP_LocalizeCB      localize_cb;  /* protocol-specific function
	                                    * to map http requests for files to
	                                    * local files */
} HTTP_Protocol;

/*****************************************************************************/

HTTP_Protocol *http_protocol_new        (struct protocol *);
void           http_protocol_register   (HTTP_Protocol *http);
void           http_protocol_unregister (HTTP_Protocol *http);
HTTP_Protocol *http_protocol_get        (struct protocol *);

/*****************************************************************************/

#endif /* __GIFT_HTTP_H */
