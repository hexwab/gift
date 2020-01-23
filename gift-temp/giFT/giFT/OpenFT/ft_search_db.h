/*
 * $Id: ft_search_db.h,v 1.11 2003/05/05 09:49:11 jasta Exp $
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

/**
 * Initialize the database environment.  This should be called before the
 * first call to ::ft_search_db_insert.
 *
 * @return Boolean success or failure.  If the return value is FALSE, this
 *         node cannot function as a search node.
 */
int ft_search_db_init ();

/**
 * Destroy the database environment and cleanup any lose files.  Not
 * implemented as of this writing.
 */
void ft_search_db_destroy ();

/*****************************************************************************/

/**
 * Insert a new file.  The implementation details become pretty sticky as
 * data from this file is inserted in three different databases.  Consider
 * this function extremely expensive.
 *
 * @param shost  Host sharing this particular file.
 * @param file   Complete shared object.
 *
 * @return Boolean success or failure.
 */
int ft_search_db_insert (FTSHost *shost, FileShare *file);

/**
 * Remove a previously inserted record.  Uses a set of record lookups to
 * retrieve all the data that must be destroyed, and then destroys it.
 *
 * @param md5  Indexed key from the master primary database.
 *
 * @return Boolean success or failure.
 */
int ft_search_db_remove (FTSHost *shost, unsigned char *md5);

/**
 * Remove all rows necessary to completely eliminate all traces of the host
 * supplied.  Clearly, this is used when a user disconnects and may
 * optionally use dirty records to slowly remove the data, given the
 * expensive nature of this database design.
 *
 * @param shost
 * @param fptr   Function pointer to be called when the removal has completed.
 *               This is used because the removal is actually handled in
 *               the background as it is a VERY expensive operation.
 *
 * @retval FALSE  Fatal error and fptr will not be called, you must cleanup
 *                for yourself.
 * @retval TRUE   All is well.  Removal has been scheduled.
 */
int ft_search_db_remove_host (FTSHost *shost, void (*fptr)(FTSHost *));

/**
 * Syncs the master primary and secondary, and the per-user databases.  This
 * is implemented (and useful) only when DEBUG has been enabled.
 *
 * @param shost  Optional per-user specification.
 *
 * @return Boolean success or failure.
 */
int ft_search_db_sync (FTSHost *shost);

/**
 * Close the per-user primary database and unlink any files associated.  This
 * does not affect the master databases.
 *
 * @param shost
 * @param rm     If TRUE, the file associated with this database will be
 *               deleted from the underlying filesystem.
 *
 * @return Boolean success or failure.
 */
int ft_search_db_close (FTSHost *shost, int rm);

/*****************************************************************************/

/**
 * Retrieve a complete share record from the per-user primary database.  This
 * does not in any way go through the master primary database.
 *
 * @param shost
 * @param md5
 *
 * @return Pointer to a referenced and complete FileShare object or NULL
 *         if the lookup could not be performed.
 */
FileShare *ft_search_db_lookup_md5 (FTSHost *shost, unsigned char *md5);

/**
 * Search the entire database scheme for all complete share records matching
 * a particular md5sum.  This uses an extremely fast lookup in the master
 * primary database, then slower lookups for each element in the result set.
 *
 * @param list         Optional list to prepend to.  Useful for appending
 *                     multiple searches to each other.
 * @param md5          16-byte MD5 to look for
 * @param max_results  Maximum number of search results to add.  If 0,
 *                     no limit will be placed on the result set.
 *
 * @return List of referenced and complete FileShare pointers representing
 *         the result set, prepended to the input list parameter or NULL
 *         if an empty set is returned.
 */
int ft_search_db_md5 (Array **a, unsigned char *md5, int max_results);

/**
 * Identical to ::ft_search_db_md5 except that the secondary database is
 * queried and the search is much more complex and expensive.
 */
int ft_search_db_tokens (Array **a, char *realm, uint32_t *query,
						 uint32_t *exclude, int max_results);

/*****************************************************************************/

#endif /* __FT_SEARCH_DB_H */
