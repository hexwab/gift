/*
 * $Id: if_search.h,v 1.6 2005/04/16 19:43:43 mkern Exp $
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

#ifndef __IF_SEARCH_H
#define __IF_SEARCH_H

/*****************************************************************************/

typedef enum
{
	IF_SEARCH_QUERY = 0,
	IF_SEARCH_BROWSE,
	IF_SEARCH_LOCATE
} IFSearchType;

typedef struct
{
	IFSearchType type;
	char        *query;
	char        *exclude;
	char        *realm;
	Dataset     *meta;
	List        *protocols;            /* list of all protocols that are being
	                                    * waited on for completion */
} IFSearch;

/*****************************************************************************/

IFEvent *if_search_new (TCPC *c, if_event_id requested, IFSearchType type,
                        char *query, char *exclude, char *realm,
                        Dataset *meta);
void if_search_finish (IFEvent *event);
void if_search_add (IFEvent *event, Protocol *p);
void if_search_remove (IFEvent *event, Protocol *p);

void if_search_result (IFEvent *event, char *user, char *node,
                       char *href, unsigned long avail, FileShare *file);

/* Returns TRUE if no more protocols are waiting for completion. */
BOOL if_search_empty (IFEvent *event);

/*****************************************************************************/

/* this is a hack so that it is available in if_share.c */
void append_meta_data (Interface *cmd, FileShare *file);

/*****************************************************************************/

#endif /* __IF_SEARCH_H */
