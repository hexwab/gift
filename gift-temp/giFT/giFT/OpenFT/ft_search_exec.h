/*
 * $Id: ft_search_exec.h,v 1.10 2003/05/05 09:49:11 jasta Exp $
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

#ifndef __FT_SEARCH_EXEC_H
#define __FT_SEARCH_EXEC_H

/*****************************************************************************/

/**
 * @file ft_search_exec.h
 *
 * @brief Wraps the low-level search functionality in a higher-level API that
 *        is readily utilized in protocol handling routines.
 *
 * This interface is responsible for searching both locally shared files
 * (that is, ones configured by the search nodes sharing/root), as well
 * remote child shares indexed locally.  There is also extra functionality
 * that all node types may utilize to confirm incoming search results for
 * accuracy and completeness.
 */

/*****************************************************************************/

#include "ft_search_obj.h"

/*****************************************************************************/

/**
 * Poorly named callback for handling individually confirmed search results.
 * This should be called as each result is completed by ft_search to avoid
 * buffering.
 *
 * @param file   Constructed structure for ease of use.  The reference count
 *               will not be modified in ::ft_search, and as such you must
 *               call ::ft_share_unref yourself if you wish for the object
 *               to be destroyed immediately.
 * @param udata  User data asked to be passed along by ::ft_search.
 *
 * @return Boolean success or failure.  That is, if FALSE is returned, the
 *         result will not be used as a part of the max results counter.
 */
typedef int (*FTSearchResultFn) (FileShare *file, void *udata);

/*****************************************************************************/

/**
 * Decompose any arbitrary string (usually a filename or meta data though)
 * into a list of constant-sized tokens.  The list will be sorted using
 * qsort(3).  Please note that it is important that these rules remain
 * constant across all nodes as "hidden" searches will depend on an equal
 * hashing on the remote node.
 *
 * @param str  String to decompose.
 *
 * @return Dynamically allocated zero-terminated sorted list of hashed tokens.
 */
uint32_t *ft_search_tokenize (char *str);

/**
 * Wrapper around ::ft_search_tokenize that ensures identical rules for
 * decomposing a complete shared record.  This will handle extraction of meta
 * data and any other relevant data.
 */
uint32_t *ft_search_tokenizef (FileShare *file);

/**
 * Perform a local search for files.  This function should only be used for
 * search node types, and will also handle searching of "remote" child
 * databases.  These databases are transacted before hand and stored/indexed
 * locally, so that no network traffic is used by this function, but it is
 * able to perform searches of other peers.  A lower-level description of
 * this functionality is described in ft_search_db.h.
 *
 * It is also important to note that this function may (according to \em
 * type) search locally submitted files by this search node.  That is, files
 * configured in gift:sharing/root.
 *
 * @param nmax      Maximum number of results to find before [successfully]
 *                  aborting the search.  Any value exceeding
 *                  openft:/search/max_results or less than or equal to 0
 *                  will use openft:/search/max_results.
 * @param resultfn  See ::FTSearchResultFn.
 * @param udata     See ::FTSearchResultFn.
 * @param type      Search type, OR'd with the necessary search options.
 * @param realm     Poorly implemented mime-filtering rule.  This is slated for
 *                  change, but I cannot commit to a completeion date.
 * @param query     Depends on \em type.  Likely a literal search string from
 *                  the user interface protocol, but might also be a pointer
 *                  to the first element in a tokenized list when
 *                  FT_SEARCH_HIDDEN is used.
 * @param exclude   Similar to \em query, except that this rule will be used
 *                  to exclude any results matching these tokens.
 *
 * @return Number of search results found and delivered to \em resultfn.  This
 *         number is guaranteed to never exceed \em nmax, when \em nmax is
 *         positive.
 */
int ft_search (int nmax, FTSearchResultFn resultfn, void *udata,
               FTSearchType type, char *realm, void *query, void *exclude);

/**
 * Perform a local compare of a complete share record to confirm the validity
 * of the result.  This function is basically a wrapper around the systems
 * used by ::ft_search and is only used by local nodes to ensure that the
 * search nodes are not maliciously adjusting search results.
 *
 * @return Boolean success or failure.  If TRUE, the file would be matched by
 *         the supplied search params, otherwise the result can be considered
 *         faulty.
 */
int ft_search_cmp (FileShare *file, FTSearchType type, char *realm,
                   void *query, void *exclude);

/*****************************************************************************/

#endif /* __FT_SEARCH_EXEC_H */
