/*
 * $Id: gt_search.h,v 1.5 2003/05/04 09:16:13 hipnod Exp $
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
struct _file_share;

typedef enum
{
	GT_SEARCH_HASH,
	GT_SEARCH_KEYWORD,
} GtSearchType;

typedef struct _gt_search
{
	/* giFT event */
	IFEvent        *event;

	/* what kind of search this is */
	GtSearchType    type;

	/* the guid used to identify the search */
	char           *guid;

	/* the query used for the search */
	char           *query;

	/* expires the search */
	unsigned long   timeout_timer;

	/* results count */
	size_t          results;
} Gt_Search;

/******************************************************************************/

Gt_Search   *gt_search_new     (IFEvent *event, char *query, GtSearchType type);
void         gt_search_free    (Gt_Search *search);
void         gt_search_disable (IFEvent *event);
void         gt_search_reply   (Gt_Search *search, struct tcp_conn *c,
                                unsigned long ip, unsigned short gt_port,
                                gt_guid *client_guid, int availability,
                                int firewalled, struct _file_share *file);
Gt_Search   *gt_search_find    (gt_guid *guid);

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
