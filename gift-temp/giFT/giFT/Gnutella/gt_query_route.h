/*
 * $Id: gt_query_route.h,v 1.6 2003/05/04 07:36:10 hipnod Exp $
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

struct _gt_packet;

/*****************************************************************************/

struct _zlib_stream;

struct _query_patch
{
	int                   seq_size;
	int                   seq_num;
	int                   compressed;

	/* where the last patch left off in the table */
	int                   table_pos;

	struct _zlib_stream  *stream;
};

struct _query_router
{
	char                 *table;
	size_t                size;

	struct _query_patch  *patch;
};

typedef struct _query_router QueryRouter;
typedef struct _query_patch  QueryPatch;

/*
 * Delimiters for words in the query-router protocol hash function.
 * Dunno if this is right,
 */
#define QRP_DELIMITERS       "'`~!@#$%^&*()_-+={}[]|\\;:,./?<> \t\n\v"

/*****************************************************************************/

uint32_t          query_router_hash_str (char *words, size_t bits);

/*****************************************************************************/

void              gt_query_router_self_init    ();
void              gt_query_router_self_destroy ();
unsigned char    *gt_query_router_self         (size_t *size, int *version);
void              gt_query_router_self_add     (FileShare *file);
void              gt_query_router_self_remove  (FileShare *file);

/*****************************************************************************/

QueryRouter *query_router_new    (size_t size, int infinity);
void         query_router_free   (QueryRouter *router);

void         query_router_update (QueryRouter *router, unsigned int seq_num,
                                  unsigned int seq_size, int compressed,
                                  int bits, unsigned char *zdata,
                                  unsigned int size);


/*****************************************************************************/

void    query_route_table_submit   (Connection *c);
void    query_route_table_reset    (Connection *c);

/*****************************************************************************/

#endif /* __GT_QUERY_ROUTE_H__ */
