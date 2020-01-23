/*
 * $Id: as_search.h,v 1.10 2004/12/24 13:40:59 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SEARCH_H
#define __AS_SEARCH_H

/*****************************************************************************/

#define INVALID_SEARCH_ID 0

typedef enum
{
	SEARCH_QUERY = 0,
	SEARCH_LOCATE
} ASSearchType;

/* May be ASRealm + 100 (see as_meta.h) */
typedef enum
{
	SEARCH_ANY      = 0x64,
	SEARCH_AUDIO    = 0x65,
	SEARCH_SOFTWARE = 0x67,
	SEARCH_VIDEO    = 0x69,
	SEARCH_DOCUMENT = 0x6a,
	SEARCH_IMAGE    = 0x6b	
} ASSearchRealm;

typedef struct as_search_t ASSearch;

/* Called for each result received. If result is NULL the search terminated
 * (possibly timed out). If duplicate is TRUE a result with the same hash was
 * reported for this search before.
 */
typedef void (*ASSearchResultCb) (ASSearch *search, ASResult *result,
                                  as_bool duplicate);

struct as_search_t
{
	ASSearchType type;     /* normal or hash search */

	as_uint16    id;       /* search id used on network and interface */

	as_bool      intern;   /* TRUE if the search wasn't started and should not
	                        * be display to the GUI. e.g. intern source search
	                        */

	as_bool      finished; /* TRUE if search is finished, timed out or was
	                        * cancelled. Now new results will be accepted.
	                        */

	timer_id finish_timer; /* Timer started when query is sent to first
	                        * supernode. Finishes search when invoked.
	                        */

	/* Contains a key for each ip we sent the search to */
	ASHashTable *sent_supernodes;

	/* data for normal searches */
	unsigned char *query; /* query string */
	ASSearchRealm realm;  /* realm to search in */

	/* data for hash searches */
	ASHash *hash;
	
	ASHashTable *results; /* hash table keyed by file hash and containing a
	                       * list of results per hash. */

	ASSearchResultCb result_cb;

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* create new search */
ASSearch *as_search_create (as_uint16 id, ASSearchResultCb result_cb,
                            const char *query, ASSearchRealm realm);

/* create new hash search */
ASSearch *as_search_create_locate (as_uint16 id, ASSearchResultCb result_cb,
                                   ASHash *hash);

/* free search */
void as_search_free (ASSearch *search);

/*****************************************************************************/

/* send a query to the specified supernode */
as_bool as_search_send (ASSearch *search, ASSession *session);

/* returns number of supernodes this search was sent to */
unsigned int as_search_sent_count (ASSearch *search);

/* returns TRUE if the search was already sent to this supernode */
as_bool as_search_sent_to (ASSearch *search, ASSession *session);

/*****************************************************************************/

/* Add result to search and raise result callback. If result is NULL the
 * search is labeled as finished and the callback is raised a final time to
 * reflect that 
 */
void as_search_add_result (ASSearch *search, ASResult *result);

/* Get list of search result for this file hash. Caller must not modify list
 * in any way.
 */
List *as_search_get_results (ASSearch *search, ASHash *hash);

/*****************************************************************************/

#endif /* __AS_SEARCH_H */

