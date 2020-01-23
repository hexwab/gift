/*
 * $Id: fst_search.h,v 1.4 2003/06/27 17:29:52 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_SEARCH_H
#define __FST_SEARCH_H

#include "fst_session.h"
#include "fst_packet.h"

/*****************************************************************************/

typedef enum
{	QUERY_REALM_EVERYTHING	= 0x3f,
	QUERY_REALM_AUDIO		= 0x21,
	QUERY_REALM_VIDEO		= 0x22,
	QUERY_REALM_IMAGES		= 0x23,
	QUERY_REALM_DOCUMENTS	= 0x24,
	QUERY_REALM_SOFTWARE	= 0x25
} FSTQueryRealm;

typedef enum
{
	QUERY_CMP_EQUALS=0x00,
	QUERY_CMP_ATMOST=0x02,
	QUERY_CMP_APPROX=0x03,
	QUERY_CMP_ATLEAST=0x04,
	QUERY_CMP_SUBSTRING=0x05
} FSTQueryCmp;

/*****************************************************************************/

typedef enum { SearchTypeSearch, SearchTypeBrowse, SearchTypeLocate } FSTSearchType;

typedef struct
{
	IFEvent *gift_event;		// giFT event used to send results back
	unsigned int fst_id;			// id used with FastTrack protocol
	FSTSearchType type;          // type of search
	unsigned int sent;			// number of times a query for this search has been sent to supernodes
	unsigned int replies;		// number of received replies
	unsigned int fw_replies;	// number of firewalled replies

	char *query;                // space delimited words to search for
	char *exclude;              // words to exlude from search
	char *realm;                // (part of) mime type we're searching for, e.g. "audio"
} FSTSearch;


typedef struct
{
	List *searches;				// list of active searches
	
	fst_uint16 current_ft_id;	// running number used FastTrack protocol id

} FSTSearchList;


/*****************************************************************************/

// called by giFT to initiate search
int gift_cb_search (Protocol *p, IFEvent *event, char *query, char *exclude, char *realm, Dataset *meta);

// called by giFT to initiate browse
int gift_cb_browse (Protocol *p, IFEvent *event, char *user, char *node);

// called by giFT to locate file
int gift_cb_locate (Protocol *p, IFEvent *event, char *htype, char *hash);

// called by giFT to cancel search/locate/browse
void gift_cb_search_cancel (Protocol *p, IFEvent *event);

/*****************************************************************************/

// allocate and init new search
FSTSearch *fst_search_create (IFEvent *event, FSTSearchType type, char *query, char *exclude, char *realm);

// free search
void fst_search_free (FSTSearch *search);

// send search request to supernode and increment search->count
int fst_search_send_query (FSTSearch *search, FSTSession *session);

/*****************************************************************************/

// allocate and init searchlist
FSTSearchList *fst_searchlist_create ();

// free searchlist
void fst_searchlist_free (FSTSearchList *searchlist);

// add search to list
void fst_searchlist_add (FSTSearchList *searchlist, FSTSearch *search);

// remove search from list
void fst_searchlist_remove (FSTSearchList *searchlist, FSTSearch *search);

// lookup search by FastTrack id
FSTSearch *fst_searchlist_lookup_id (FSTSearchList *searchlist, fst_uint16 fst_id);

// lookup search by giFT event
FSTSearch *fst_searchlist_lookup_event (FSTSearchList *searchlist, IFEvent *event);

// send queries for every search in list if search->count == 0 or resent == TRUE
int fst_searchlist_send_queries (FSTSearchList *searchlist, FSTSession *session, int resent);

// process reply and send it to giFT, accepts SessMsgQueryReply and SessMsgQueryEnd
int fst_searchlist_process_reply (FSTSearchList *searchlist, FSTSessionMsg msg_type, FSTPacket *msg_data);

/*****************************************************************************/

#endif /* __FST_SEARCH_H */
