/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#ifndef __SL_SEARCH_H
#define __SL_SEARCH_H

#include "sl_soulseek.h"

/*****************************************************************************/

typedef struct
{
	uint32_t token;
	Protocol *p;
	IFEvent *event;
	char *exclude;
} SLSearchList;

typedef enum
{
	QUERY_REALM_EVERYTHING = 0x3f,
	QUERY_REALM_AUDIO      = 0x21,
	QUERY_REALM_VIDEO      = 0x22,
	QUERY_REALM_IMAGES     = 0x23,
	QUERY_REALM_DOCUMENTS  = 0x24,
	QUERY_REALM_SOFTWARE   = 0x25
} SLQueryRealm;

typedef struct
{
	char *username;
	SLPeer *peer;
} SLBrowseList;

/*****************************************************************************/

// initialize the searches
SLSearchList *sl_search_create();

// returns a search list item that has the given token associated
SLSearchList *sl_search_get_search(uint32_t token);

// returns a browse list item for the given username
SLBrowseList *sl_search_get_browse(char *username);

// returns a browse list item for the given peer
SLBrowseList *sl_search_get_browse_by_peer(SLPeer *peer);

// cleans up the search list
void sl_search_destroy(SLSearchList *list);

// called by giFT to initiate search
int sl_gift_cb_search(Protocol *p, IFEvent *event, char *query, char *exclude, char *realm, Dataset *meta);

// called by giFT to initiate browse
int sl_gift_cb_browse(Protocol *p, IFEvent *event, char *user, char *node);

// called by giFT to locate file
int sl_gift_cb_locate(Protocol *p, IFEvent *event, char *htype, char *hash);

// called by giFT to cancel search/locate/browse
void sl_gift_cb_search_cancel(Protocol *p, IFEvent *event);

/*****************************************************************************/

#endif /* __SL_SEARCH_H */
