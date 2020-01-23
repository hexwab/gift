/*
 * $Id: gt_search.h,v 1.12 2003/06/07 05:46:31 hipnod Exp $
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

#ifndef __GT_SEARCH_H__
#define __GT_SEARCH_H__

/******************************************************************************/

struct tcp_conn;
struct file_shsare;

typedef enum
{
	GT_SEARCH_HASH,
	GT_SEARCH_KEYWORD,
} GtSearchType;

typedef struct gt_search
{
	/* giFT event */
	IFEvent        *event;

	/* what kind of search this is */
	GtSearchType    type;

	/* the guid used to identify the search */
	char           *guid;

	/* the query used for the search */
	char           *query;

	/* the realm used for this query, if any */
	char           *realm;

	/* expires the search */
	unsigned long   timeout_timer;

	/* results count */
	size_t          results;
} GtSearch;

/******************************************************************************/

GtSearch    *gt_search_new     (IFEvent *event, char *query, GtSearchType type);
void         gt_search_free    (GtSearch *search);
void         gt_search_disable (IFEvent *event);
void         gt_search_reply   (GtSearch *search, struct tcp_conn *c,
                                in_addr_t ip, in_port_t gt_port,
                                gt_guid_t *client_guid, int availability,
                                int firewalled, struct file_share *file);
GtSearch    *gt_search_find    (gt_guid_t *guid);

/* submit active searches to a node after a timeout */
void  gt_searches_submit (struct tcp_conn *c, time_t delay);

/******************************************************************************/

int  gnutella_search        (Protocol *p, IFEvent *event, char *query,
                             char *exclude, char *realm, Dataset *meta);

int  gnutella_locate        (Protocol *p, IFEvent *event, char *htype,
                             char *hash);

void gnutella_search_cancel (Protocol *p, IFEvent *event);

/******************************************************************************/

#endif /* __GT_SEARCH_H__ */
