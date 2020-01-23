/*
 * ft_search.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __FT_SEARCH_H
#define __FT_SEARCH_H

/*****************************************************************************/

/**
 * @file ft_search.h
 *
 * \todo this
 */

/*****************************************************************************/

#include "ft_event.h"

/*****************************************************************************/

/**
 * Define this if you wish to use FT_SEARCH_HIDDEN (documented below)
 */
#undef FT_SEARCH_PARANOID

/*****************************************************************************/

/**
 * Search method/option bitwise OR
 */
typedef enum
{
	/**
	 * @name Methods
	 * Search methods
	 */
	FT_SEARCH_FILENAME = 0x01,         /**< Token query based on share path */
	FT_SEARCH_MD5      = 0x02,         /**< Single query based on MD5 sum */
	FT_SEARCH_HOST     = 0x04,         /**< Browsing */

	/**
	 * @name Options
	 * Search options
	 */
	FT_SEARCH_LOCAL    = 0x08,         /**< Search locally shared files
										*   through iteration */

	/**
	 * Do not send human readable search strings.  In their place, send the
	 * ::ft_search_tokenize output.  This is not cryptographically secure and
	 * should not appease the most analy secure user, sorry.
	 */
	FT_SEARCH_HIDDEN   = 0x10
} FTSearchType;

/**
 * Structure to manage remote searches
 */
typedef struct
{
	FTEvent     *event;                /**< event link */
	FTSearchType type;                 /**< search flags */

	Dataset     *ref;                  /**< dataset representing all search
	                                    *   nodes remotely queuried.  this is
	                                    *   used to determine when the search
	                                    *   has completely finished */

	char        *query;                /**< human-readable query list */
	char        *exclude;              /**< human-readable exclusion list */
	char        *realm;                /**< mime group filtering */
	ft_uint32   *qtokens;              /**< hashed query */
	ft_uint32   *etokens;              /**< hashed exclusion */
} FTSearch;

/*****************************************************************************/

/**
 * Create a new search object.
 *
 * @param type
 * @param query
 * @param exclude
 * @param realm
 */
FTSearch *ft_search_new (IFEvent *event, FTSearchType type,
                         char *query, char *exclude, char *realm);

/**
 * Add to the list of parents that need to reply before this search can
 * complete successfully.
 *
 * @param search
 * @param parent
 */
void ft_search_need_reply (FTSearch *search, in_addr_t parent);

/**
 * Reply to interface protocol.  file == NULL is considered end-of-search.
 *
 * @param search
 * @param parent  The search results parent node, if one.
 * @param file    Search result itself.
 */
void ft_search_reply (FTSearch *search, Connection *parent, FileShare *file);

/*****************************************************************************/

/**
 * Force a reply on all searches from this connection.
 */
void ft_search_force_reply (FTSearch *search, in_addr_t parent);

/*****************************************************************************/

/**
 * Handle search.
 */
int openft_search (Protocol *p, IFEvent *event, char *query, char *exclude,
                   char *realm, Dataset *meta);

/**
 * Handle browse.
 */
int openft_browse (Protocol *p, IFEvent *event, char *user, char *node);

/**
 * Handle locate.
 */
int openft_locate (Protocol *p, IFEvent *event, char *hash);

/**
 * Handle search/browse/locate cancellation.
 */
void openft_search_cancel (Protocol *p, IFEvent *event);

/*****************************************************************************/

#endif /* __FT_SEARCH_H */
