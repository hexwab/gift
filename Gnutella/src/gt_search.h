/*
 * $Id: gt_search.h,v 1.19 2004/01/18 05:43:13 hipnod Exp $
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

#ifndef GIFT_GT_SEARCH_H_
#define GIFT_GT_SEARCH_H_

/******************************************************************************/

struct tcp_conn;
struct file_share;

typedef enum
{
	GT_SEARCH_HASH,
	GT_SEARCH_KEYWORD,
} gt_search_type_t;

/*
 * These flags exist in what used to be the MinSpeed field of
 * queries. The documentation for this field is arranged as two
 * bytes in big-endian order, but this uses it in little-endian
 * order so as to be consistent with the rest of the protocol.
 */
typedef enum gt_query_flags
{
	QF_WANTS_XML     = 0x0020,    /* servent wants XML metadata */
	QF_ONLY_NON_FW   = 0x0040,    /* source desires non-firewalled hits only */
	QF_HAS_FLAGS     = 0x0080,    /* this query has this interpretation */
} gt_query_flags_t;

typedef struct gt_search
{
	/* giFT event */
	IFEvent        *event;

	/* what kind of search this is */
	gt_search_type_t type;

	/* the guid used to identify the search */
	char           *guid;

	/* the query used for the search */
	char           *query;

	/* the hash to look for this search if a URN query XXX should use gt_urn_t
	 * here, but the interface is just too borked */
	char           *hash;

	/* the realm used for this query, if any */
	char           *realm;

	/* expires the search according to critieria: see 
	 * gt_search.c:search_timeout */
	timer_id        timeout_timer;

	/* when the search was started */
	time_t          start;

	/* how many nodes this search has been submitted to */
	size_t          submitted;

	/* the last time we submitted to a node */
	time_t          last_submit;

	/* time the last result for this search was seen */
	time_t          last_result;

	/* results count */
	size_t          results;
} GtSearch;

/* the default ttl for searches */
#define GT_SEARCH_TTL          (5)

/******************************************************************************/

GtSearch    *gt_search_new     (IFEvent *event, char *query,
                                gt_search_type_t type);
void         gt_search_free    (GtSearch *search);
void         gt_search_disable (IFEvent *event);
void         gt_search_reply   (GtSearch *search, struct tcp_conn *c,
                                in_addr_t ip, in_port_t gt_port,
                                gt_guid_t *client_guid, int availability,
                                BOOL firewalled, struct file_share *file);
GtSearch    *gt_search_find    (gt_guid_t *guid);

/* submit active searches to a node after a timeout */
void  gt_searches_submit (struct tcp_conn *c, time_t delay);

/******************************************************************************/

BOOL gnutella_search        (Protocol *p, IFEvent *event, char *query,
                             char *exclude, char *realm, Dataset *meta);

BOOL gnutella_locate        (Protocol *p, IFEvent *event, char *htype,
                             char *hash);

void gnutella_search_cancel (Protocol *p, IFEvent *event);

/******************************************************************************/

void gt_search_init         (void);
void gt_search_cleanup      (void);

/******************************************************************************/

#endif /* GIFT_GT_SEARCH_H_ */
