/*
 * $Id: ft_routing.h,v 1.3 2004/09/04 21:38:53 hexwab Exp $
 *
 * Copyright (C) 2004 giFT project (gift.sourceforge.net)
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

#ifndef __FT_ROUTING_H
#define __FT_ROUTING_H

/*****************************************************************************/

#define MD5_FILTER_HASHES   1
#define MD5_FILTER_BITS     18 /* 32K */

/* unused */
#define TOKEN_FILTER_HASHES 2
#define TOKEN_FILTER_BITS   16

#define FILTER_SYNC_INTERVAL (10*MINUTES)

/*****************************************************************************/

/* A bloom filter that can be efficiently synchronized with remote
 * hosts. */
typedef struct {
	BloomFilter *filter;
	BloomFilter *sync;
} SyncFilter;

extern SyncFilter md5_filter;

/*****************************************************************************/

BOOL ft_routing_init (void);
BOOL ft_routing_free (void);
BOOL ft_routing_sync (SyncFilter *f);

/*****************************************************************************/

#endif
