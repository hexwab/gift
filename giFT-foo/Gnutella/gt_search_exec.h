/*
 * $Id: gt_search_exec.h,v 1.7 2003/06/07 05:24:14 hipnod Exp $
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

#ifndef __GT_SEARCH_EXEC_H__
#define __GT_SEARCH_EXEC_H__

/******************************************************************************/

#define LOG_RESULTS      gt_config_get_int("search/log_results=0")

/******************************************************************************/

typedef struct gt_token_set
{
	uint32_t    *data;
	size_t       data_len;
	size_t       len;
} GtTokenSet;

/******************************************************************************/

GtTokenSet     *gt_token_set_new     (void);
void            gt_token_set_free    (GtTokenSet *ts);
void            gt_token_set_append  (GtTokenSet *ts, uint32_t token);

/******************************************************************************/

List *gt_search_exec (char *query, GtSearchType type, void *extended,
                      uint8_t ttl, uint8_t hops);

/******************************************************************************/

#endif /* __GT_SEARCH_EXEC_H__ */
