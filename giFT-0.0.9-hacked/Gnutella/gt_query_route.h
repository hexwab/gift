/*
 * $Id: gt_query_route.h,v 1.10 2003/06/01 09:37:15 hipnod Exp $
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

#ifndef __GT_QUERY_ROUTE_H__
#define __GT_QUERY_ROUTE_H__

/*****************************************************************************/

/*
 * Delimiters for words in the query-router protocol hash function.
 * This is what LimeWire uses.
 */
#define QRP_DELIMITERS       " -._+/*()\\/"

/*****************************************************************************/

struct gt_packet;
struct gt_zlib_stream;

typedef struct gt_query_router GtQueryRouter;
typedef struct gt_query_patch  GtQueryPatch;

struct gt_query_patch
{
	int       seq_size;
	int       seq_num;
	int       compressed;

	/* where the last patch left off in the table */
	int       table_pos;

	struct gt_zlib_stream  *stream;
};

struct gt_query_router
{
	char          *table;
	size_t         size;

	GtQueryPatch  *patch;
};

/*****************************************************************************/

uint32_t          gt_query_router_hash_str (char *words, size_t bits);

/*****************************************************************************/

void              gt_query_router_self_init    (void);
void              gt_query_router_self_destroy (void);
unsigned char    *gt_query_router_self         (size_t *size, int *version);
void              gt_query_router_self_add     (FileShare *file);
void              gt_query_router_self_remove  (FileShare *file);

/*****************************************************************************/

GtQueryRouter *gt_query_router_new    (size_t size, int infinity);
void           gt_query_router_free   (GtQueryRouter *router);

void           gt_query_router_update (GtQueryRouter *router, 
                                       size_t seq_num, size_t seq_size, 
                                       int compressed, int bits, 
                                       unsigned char *zdata, size_t size);


/*****************************************************************************/

void    query_route_table_submit   (TCPC *c);
void    query_route_table_reset    (TCPC *c);

/*****************************************************************************/

#endif /* __GT_QUERY_ROUTE_H__ */
