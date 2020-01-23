/*
 * $Id: ft_search_db.h,v 1.21 2003/10/26 13:15:43 jasta Exp $
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

#ifndef __FT_SEARCH_DB_H
#define __FT_SEARCH_DB_H

/*****************************************************************************/

/**
 * @file ft_search_db.h
 *
 * @brief Access the search node database(s).
 *
 * Provides various routines for insertion and lookup of the search node
 * database(s), both global and local.  The database design boils down to
 * simple redundancy of data through artificial indexes.  Please note that
 * this design has changed slightly with the introduction of a DB_ENV, as
 * well as moving all peruser shares to a single file (multiple databases
 * still).  When reading the following description try to keep in mind that
 * it was not updated to reflect this change.
 *
 * First, all md5 sums indexed are archived in the master primary database
 * (md5.index) for all child users. At each entry is a simple 4 byte host
 * used to link as a sort of unique foreign key into the peruser primary
 * database (children.data) which has the complete share record.  This
 * database relationship is used for quick lookups of md5 searches without
 * requiring that each peruser database be open at all times (which they are
 * not).
 *
 * The most important index is the master secondary database (tokens.index)
 * which uses simple 4 byte hashed "tokens" as keys with data entries
 * containing a foreign key reference to the peruser primary database by
 * providing both an md5 sum as well as the complete host.  When a search
 * request is issued, each element of the request is tokenized and hashed to
 * reflect the exact same format as was inserted into the master secondary
 * database, then the code performs a single lookup per token in the query.
 * This results in a list of libdb cursors pointing to the set that matches
 * only the token standing alone.  In order to get the result set from both
 * (or multiple) query tokens, we iterate through the shortest result set
 * attempting to create a subset representing the intersection.  Once this is
 * completed, we iterate the final result set performing a single lookup in
 * the peruser databases per matched entry.
 *
 * There are many downfalls to this design, however.  First of all, when a
 * new share is to be inserted, it not only needs to be inserted in both the
 * per-user primary database which contains the complete record, but also one
 * insertion into the master primary as well as a variable number of
 * insertions into the master secondary database.  Naturally, this can become
 * quite costly as the total number of insertions per file averages out to
 * about 15.  On top of that, when the host is scheduled for removal (for
 * example, our child user has disconnected from the network), we must first
 * iterate the entire per-user primary database.  Upon doing so, we will
 * perform a partial lookup of each record so that we know the tokens that
 * need to be removed, as well as the straight md5 key.  Since this is
 * clearly a very expensive operation we first set a "dirty" flag on the host
 * while we're removing in the background so that we know not to allow the
 * user back until removal has finished, and that no results that would
 * normally be selected from the design with this user are valid.
 *
 * For more information on what constitutes a share token see
 * ::ft_search_tokenize.
 */

/*****************************************************************************/

#ifdef USE_LIBDB
# ifdef HAVE_DB4_DB_H
#  include <db4/db.h>
# endif
# ifdef HAVE_DB_H
#  include <db.h>
# endif
# ifdef HAVE_DB3_DB_H
#  include <db3/db.h>
# endif
#endif /* USE_LIBDB */

/*****************************************************************************/

/**
 * Object which holds several temporary variables used when performing
 * database operations.  This structure is referred to by FTSession for
 * children nodes however all management of its memory is internal, including
 * its initial allocation and assignment to the FTSession object.
 */
typedef struct ft_search_db
{
	FTNode *node;                      /**< one search db entry per child */

#ifdef USE_LIBDB
	char   *share_idx_name;            /**< database name */
	DB     *share_idx;
	DBC    *remove_curs;               /**< required cursor used to remove
	                                    *   each individual share when a
	                                    *   host removal operation is
	                                    *   requested */
#endif /* USE_LIBDB */

	unsigned long shares;              /**< total files currently shared */
	double        size;                /**< total size (MB) */
	Array   *dups;                     /* duplicated hashes */
} FTSearchDB;

/* shorthand */
#define FT_SEARCH_DB(node) ((node)->session->search_db)

/*****************************************************************************/

/**
 * Initialize the database environment.  This should be called before the
 * first call to ::ft_search_db_insert.
 *
 * @return Boolean success or failure.  If the return value is FALSE, this
 *         node cannot function as a search node.
 */
BOOL ft_search_db_init (const char *envpath, unsigned long cachesize);

/**
 * Destroy the database environment and cleanup any lose files.  Not
 * implemented as of this writing.
 */
void ft_search_db_destroy (void);

/*****************************************************************************/

/**
 * Insert a new file.  The implementation details become pretty sticky as
 * data from this file is inserted in three different databases.  Consider
 * this function extremely expensive.
 *
 * @param node   Host sharing this particular file.
 * @param share  Complete shared object.
 *
 * @return Boolean success or failure.
 */
BOOL ft_search_db_insert (FTNode *node, Share *share);

/**
 * Remove a previously inserted record.  Uses a set of record lookups to
 * retrieve all the data that must be destroyed, and then destroys it.
 *
 * @param node
 * @param md5   Indexed key from the master primary database.
 *
 * @return Boolean success or failure.
 */
BOOL ft_search_db_remove (FTNode *node, unsigned char *md5);

/**
 * Remove all rows necessary to completely eliminate all traces of the host
 * supplied.  Clearly, this is used when a user disconnects and may
 * optionally use dirty records to slowly remove the data, given the
 * expensive nature of this database design.
 */
BOOL ft_search_db_remove_host (FTNode *node);

/**
 * Syncs the master primary and secondary, and the per-user databases.  This
 * is implemented (and useful) only when SEARCH_DB_SYNC has been enabled.
 * See ft_search_db.c for more information.
 *
 * @param node  Optional per-user specification.  If NULL, only the global
 *              primary and secondary databases will be synced.
 *
 * @return Boolean success or failure.
 */
BOOL ft_search_db_sync (FTNode *node);

/**
 * Open the per-user database for writing.  This must be done before any
 * write action is performed.
 */
BOOL ft_search_db_open (FTNode *node);

/**
 * Tests if the per-user databse is currently open for writing.  This is merely
 * a convenience as the return value from ft_search_db_open and careful
 * tracking will tell you the very same thing this function will.
 */
BOOL ft_search_db_isopen (FTNode *node);

/**
 * Close the per-user primary database and unlink any files associated.  This
 * does not affect the master databases.
 *
 * @param node
 * @param rm     If TRUE, the file associated with this database will be
 *               deleted from the underlying filesystem.
 *
 * @return Boolean success or failure.
 */
BOOL ft_search_db_close (FTNode *node, BOOL rm);

/*****************************************************************************/

/**
 * Retrieve a complete share record from the per-user primary database.  This
 * does not in any way go through the master primary database.
 *
 * @param node
 * @param md5
 *
 * @return Pointer to a referenced and complete Share object or NULL
 *         if the lookup could not be performed.
 */
Share *ft_search_db_lookup_md5 (FTNode *node, unsigned char *md5);

/**
 * Search the entire database scheme for all complete share records matching
 * a particular md5sum.  This uses an extremely fast lookup in the master
 * primary database, then slower lookups for each element in the result set.
 *
 * @param a            Optional list to prepend to.  Useful for appending
 *                     multiple searches to each other.
 * @param md5          16-byte MD5 to look for
 * @param max_results  Maximum number of search results to add.  If 0,
 *                     no limit will be placed on the result set.
 *
 * @return List of referenced and complete Share pointers representing
 *         the result set, prepended to the input list parameter or NULL
 *         if an empty set is returned.
 */
int ft_search_db_md5 (Array **a, unsigned char *md5, int max_results);

/**
 * Identical to ::ft_search_db_md5 except that the secondary database is
 * queried and the search is much more complex and expensive.
 */
int ft_search_db_tokens (Array **a, char *realm, uint32_t *query,
			 uint32_t *exclude, uint8_t *order, int max_results);

/*****************************************************************************/

#ifdef OPENFT_TEST_SUITE

/**
 * Apply rigorous tests and benchmarking routines to a search database loaded
 * from test data and a live sampling of user queries.  This can be used to
 * compare alternate database designs as well ensure the integrity and
 * reliability of the database through stressed usage.
 */
BOOL test_suite_search_db (Protocol *p);

#endif /* OPENFT_TEST_SIUTE */

/*****************************************************************************/

#endif /* __FT_SEARCH_DB_H */
