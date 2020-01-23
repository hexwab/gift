/*
 * $Id: as_search_man.h,v 1.6 2005/12/18 17:32:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SEARCH_MAN_H_
#define __AS_SEARCH_MAN_H_

/*****************************************************************************/

typedef struct
{
	/* We have two hash tables one for all searches keyed by search id and
	 * and extra one for hash searches keyed by file hash. Both tables point
	 * to the same result data.
	 */
	ASHashTable *searches;
	ASHashTable *hash_searches;

	as_uint16 next_search_id; /* our source of search ids */

} ASSearchMan;

/*****************************************************************************/

/* allocate and init search manager */
ASSearchMan *as_searchman_create ();

/* free manager */
void as_searchman_free (ASSearchMan *man);

/*****************************************************************************/

/* handle result packet */
as_bool as_searchman_result (ASSearchMan *man, ASSession *session,
                             ASPacket *packet);

/* handle new supernode session by sending pending searches to it */
void as_searchman_new_session (ASSearchMan *man, ASSession *session);

/*****************************************************************************/

/* create and run new query search */
ASSearch *as_searchman_search (ASSearchMan *man, ASSearchResultCb result_cb,
                               const char *query, ASSearchRealm realm);

/* create and run new hash search */
ASSearch *as_searchman_locate (ASSearchMan *man, ASSearchResultCb result_cb,
                               ASHash *hash);

/* Stop search but keep previous results until search is removed. Raises
 * search callback with NULL result.
 */
as_bool as_searchman_stop (ASSearchMan *man, ASSearch *search);

/* Remove and free search and all results. Does not raise search result
 * callback.
 */
as_bool as_searchman_remove (ASSearchMan *man, ASSearch *search);

/*****************************************************************************/

/* get any search by id */
ASSearch *as_searchman_lookup (ASSearchMan *man, as_uint16 search_id);

/* get _hash_ search by hash */
ASSearch *as_searchman_lookup_hash (ASSearchMan *man, ASHash *hash);

/* returns TRUE if search is currently managed by search man */
as_bool as_searchman_valid_search (ASSearchMan *man, ASSearch *search);

/*****************************************************************************/

#endif /* __AS_SEARCH_MAN_H_ */
