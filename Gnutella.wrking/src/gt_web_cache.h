/*
 * $Id: gt_web_cache.h,v 1.8 2004/02/23 04:18:43 hipnod Exp $
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

#ifndef GIFT_GT_WEB_CACHE_H_
#define GIFT_GT_WEB_CACHE_H_

/*****************************************************************************/

/* this should be in http_request.c instead */
#define HTTP_PROXY              gt_config_get_str("http/proxy")

/*****************************************************************************/

typedef struct gt_web_cache
{
	/* host name of the web_cache */
	char  *host_name;

	/* xxxx in http://foo.com/xxxx */
	char  *remote_path;

	/* last time we visited this webcache */
	time_t visited_time;
} Gt_WebCache;

/*****************************************************************************/

/* initialize the gwebcaches from the ~/.giFT/Gnutella/gwebcaches file */
BOOL gt_web_cache_init (void);

/* cleanup any data allocated for gwebcaches */
void gt_web_cache_cleanup (void);

/* talk to the web caches to find more nodes (and webcaches) */
void gt_web_cache_update  (void);

/*****************************************************************************/

#endif /* GIFT_GT_WEB_CACHE_H_ */
