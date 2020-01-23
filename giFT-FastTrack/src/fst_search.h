/*
 * $Id: fst_search.h,v 1.20 2004/11/10 20:00:57 mkern Exp $
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
#include "fst_source.h"
#include "fst_packet.h"
#include "fst_meta.h"

/*****************************************************************************/

typedef enum
{	QUERY_REALM_EVERYTHING = 0xbf,
	QUERY_REALM_AUDIO      = 0xa1,
	QUERY_REALM_VIDEO      = 0xa2,
	QUERY_REALM_IMAGES     = 0xa3,
	QUERY_REALM_DOCUMENTS  = 0xa4,
	QUERY_REALM_SOFTWARE   = 0xa5
} FSTQueryRealm;

typedef enum
{
	QUERY_CMP_EQUALS    = 0x00,
	QUERY_CMP_ATMOST    = 0x02,
	QUERY_CMP_APPROX    = 0x03,
	QUERY_CMP_ATLEAST   = 0x04,
	QUERY_CMP_SUBSTRING = 0x05
} FSTQueryCmp;

/*****************************************************************************/

typedef enum
{
	SearchTypeSearch,
	SearchTypeBrowse,
	SearchTypeLocate
} FSTSearchType;

typedef struct
{
	IFEvent *gift_event;		/* giFT event used to send results back  */
	unsigned int fst_id;		/* id used with FastTrack protocol  */
	FSTSearchType type;         /* type of search  */

	Dataset *sent_nodes;        /* Pointers to nodes in node cache we already
	                             * sent this search to. Entries are removed as
	                             * each supernode reports end of results or
	                             * disconnects. The search is finished when 
	                             * this becomes empty.
	                             */

	unsigned int search_more;   /* auto search more count down */

	int banlist_filter;			/* cache for config key main/banlist_filter */

	unsigned int replies;			/* number of received replies  */
	unsigned int fw_replies;		/* number of firewalled replies  */
	unsigned int banlist_replies;	/* number of replies which were dropped
									 * because the ip was on the ban list
									 */

	char *query;       /* space delimited words to search for  */
	char *exclude;     /* words to exlude from search */
	char *realm;       /* mime type we're searching for e.g. "audio" */

	FSTHash *hash;     /* the hash to look for if this is SearchTypeLocate */
} FSTSearch;


typedef struct
{
	List *searches;				/* list of active searches */
	
	fst_uint16 current_ft_id;	/* running number used FastTrack protocol id */

} FSTSearchList;


typedef struct
{
	FSTSource *source;

	char *filename;
	fst_uint32 filesize;
	fst_uint32 file_id;
	FSTHash *hash;

	List *metatags;	/* list of FSTMetaTags */

} FSTSearchResult;


/*****************************************************************************/

/* called by giFT to initiate search */
int fst_giftcb_search (Protocol *p, IFEvent *event, char *query, char *exclude,
					   char *realm, Dataset *meta);

/* called by giFT to initiate browse */
int fst_giftcb_browse (Protocol *p, IFEvent *event, char *user, char *node);

/* called by giFT to locate file */
int fst_giftcb_locate (Protocol *p, IFEvent *event, char *htype, char *hstr);

/* called by giFT to cancel search/locate/browse */
void fst_giftcb_search_cancel (Protocol *p, IFEvent *event);

/*****************************************************************************/

/* allocate and init new search */
FSTSearch *fst_search_create (IFEvent *event, FSTSearchType type, char *query,
							  char *exclude, char *realm);

/* free search */
void fst_search_free (FSTSearch *search);

/* send search request to supernode and increment search->count */
int fst_search_send_query (FSTSearch *search, FSTSession *session);

/* send search request to all sessions and increment search->count */
int fst_search_send_query_to_all (FSTSearch *search);

/*****************************************************************************/

/* allocate and init searchlist */
FSTSearchList *fst_searchlist_create ();

/* free searchlist */
void fst_searchlist_free (FSTSearchList *searchlist);

/* add search to list */
void fst_searchlist_add (FSTSearchList *searchlist, FSTSearch *search);

/* remove search from list */
void fst_searchlist_remove (FSTSearchList *searchlist, FSTSearch *search);

/* lookup search by FastTrack id */
FSTSearch *fst_searchlist_lookup_id (FSTSearchList *searchlist, fst_uint16 fst_id);

/* lookup search by giFT event */
FSTSearch *fst_searchlist_lookup_event (FSTSearchList *searchlist, IFEvent *event);

/* send queries to supernode for every search in list
 */
int fst_searchlist_send_queries (FSTSearchList *searchlist,
                                 FSTSession *session);

/* process reply and send it to giFT
 * accepts SessMsgQueryReply and SessMsgQueryEnd
 */
int fst_searchlist_process_reply (FSTSearchList *searchlist,
                                  FSTSession *session,
								  FSTSessionMsg msg_type, FSTPacket *msg_data);

/* Terminate all queries sent to session since it disconnected us. */
void fst_searchlist_session_disconnected (FSTSearchList *searchlist,
                                          FSTSession *session);

/*****************************************************************************/

/* FXIME: These should never be accessed from outside fst_search.c. They
 * shouldn't be here.
 */

/* alloc and init result */
FSTSearchResult *fst_searchresult_create ();

/* free result */
void fst_searchresult_free (FSTSearchResult *result);

/* add meta data tag to result */
void fst_searchresult_add_tag (FSTSearchResult *result, FSTMetaTag *tag);

/* send result to gift */
int fst_searchresult_write_gift (FSTSearchResult *result, FSTSearch *search);

/*****************************************************************************/

#endif /* __FST_SEARCH_H */
