/*
 * $Id: ft_search_obj.h,v 1.2 2003/05/05 09:49:11 jasta Exp $
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

#ifndef __FT_SEARCH_OBJ_H
#define __FT_SEARCH_OBJ_H

/*****************************************************************************/

/**
 * @file ft_search_obj.h
 *
 * @brief Simple search object to manage both locally requested searches and
 *        forwarded queries.
 *
 * @note All search-related objects and their appropriate APIs are defined
 *       here.  This included browse and search forwarding.
 *
 * @todo Unify this API while maintaining divided data structures. There is a
 *       lot of code duplication here.  I'm thinking perhaps an FTQuery object
 *       that contains them both, or something.
 */

/*****************************************************************************/

#include "ft_guid.h"

/*****************************************************************************/

/**
 * Define this if you wish to use FT_SEARCH_HIDDEN (documented below).
 */
/* #define FT_SEARCH_PARANOID */

/*****************************************************************************/

/**
 * Search methods and options.
 */
typedef enum
{
	/**
	 * @name Methods
	 */
	FT_SEARCH_FILENAME = 0x01,         /**< Token query based on share path */
	FT_SEARCH_MD5      = 0x02,         /**< Single query based on MD5 sum */
	FT_SEARCH_HOST     = 0x04,         /**< Browsing */

	/**
	 * @name Options
	 */
	FT_SEARCH_LOCAL    = 0x08,         /**< Search locally shared files
										*   through iteration */

	/**
	 * Do not send human readable search strings.  In their place, send the
	 * ::ft_search_tokenize output.  This is not cryptographically secure and
	 * is only expected to appease the casual privacy nut, sorry.
	 */
	FT_SEARCH_HIDDEN   = 0x10
} FTSearchType;

/**
 * Search parameters.  I'm hoping to be able to use this structure more
 * generically throughout the rest of the code.
 */
typedef struct
{
	FTSearchType type;                 /**< Search method/options */

	char        *realm;                /**< Crude mime filtering */

	char        *query;                /**< Literal search string */
	char        *exclude;              /**< Exclude matches */

	uint32_t    *qtokens;              /**< Query tokens */
	uint32_t    *etokens;              /**< Exclude tokens */
} FTSearchParams;

/**
 * Structure to manage locally requested searches.
 */
typedef struct
{
	IFEvent       *event;              /**< Handle to reply to giFT */
	ft_guid_t     *guid;               /**< Semi-unique search id */
	timer_id       timeout;            /**< Automatic search timeout */

	FTSearchParams params;             /**< Search parameters */

	Dataset       *waiting_on;         /**< Nodes that were sent search requests,
	                                    *   but have yet to reply.  The
	                                    *   search is considered complete
	                                    *   when either the timeout is
	                                    *   reached, or this becomes empty */
} FTSearch;

/**
 * Similar to FTSearch, except used to manage and isolate browse requests.
 * See ::ft_browse_new for more information.
 */
typedef struct
{
	IFEvent       *event;              /**< Handle to reply to giFT */
	ft_guid_t     *guid;               /**< Semi-unique browse id (isolated
	                                    *   from search ids) */
	timer_id       timeout;            /**< Automatic search timeout */
	in_addr_t      user;               /**< Address of the node being
	                                    *   browsed */
} FTBrowse;

/**
 * Structure used to manage remote search forwarding requests.  This code is
 * used by search nodes only.
 */
typedef struct
{
	ft_guid_t     *guid;               /**< Original search id */
	timer_id       timeout;            /**< Forward timeout  */
	in_addr_t      src;                /**< Node that requested we forward
	                                    *   the search */
	in_addr_t      dst;                /**< Node we have delivered the
	                                    *   search to.  Replies will be
	                                    *   delivered back to
	                                    *   FTSearchFwd::src */
} FTSearchFwd;

/*****************************************************************************/

/**
 * Construct a new local search object.  This indirectly coordinates requests
 * over OpenFT and replies to giFT.
 *
 * @param event   giFT reply handle.
 */
FTSearch *ft_search_new (IFEvent *event, FTSearchType type, char *realm,
                         char *query, char *exclude);

/**
 * Elegantly cleanup the search.  This will destroy the object as well as
 * complete the event with giFT.
 */
void ft_search_finish (FTSearch *srch);

/**
 * Lookup a search by it's semi-unique identifier.  This is used when search
 * results come in from remote nodes, they will be carrying the identifier we
 * originally created with ::ft_search_new.
 */
FTSearch *ft_search_find (ft_guid_t *guid);

/**
 * Locate a search object by the original event argument.  This is used for
 * ::ft_search_disable.
 */
FTSearch *ft_search_find_by_event (IFEvent *event);

/**
 * Disable the search event handler.  This is done when giFT asks us to
 * cancel a currently active search.
 */
void ft_search_disable (FTSearch *srch);

/*****************************************************************************/

unsigned int ft_search_sentto (FTSearch *srch, FTNode *node);
unsigned int ft_search_rcvdfrom (FTSearch *srch, FTNode *node);

/*****************************************************************************/

/**
 * Nearly identical to ::ft_search_new, except that the event is specialized,
 * and isolated from the search data storage.  This is a necessary evil to
 * improve uniqueness of data transmissions on OpenFT.
 */
FTBrowse *ft_browse_new (IFEvent *event, in_addr_t user);

/**
 * Gracefully terminate a previously constructed browse event.
 */
void ft_browse_finish (FTBrowse *browse);

/**
 * Lookup a browse by guid, and assert that the user matches.  This is used
 * when receiving results, which are only valid coming from the user we are
 * browsing (the protocol demands direct browsing or no browsing at all).
 * This may change in future versions, but for now, we may as well provide
 * the extra security while the design allows.
 */
FTBrowse *ft_browse_find (ft_guid_t *guid, in_addr_t user);

/**
 * @see ft_search_find_by_event
 */
FTBrowse *ft_browse_find_by_event (IFEvent *event);

/**
 * @see ft_search_disable
 */
void ft_browse_disable (FTBrowse *browse);

/*****************************************************************************/

/**
 * Create a new forward object.  Please note that this function may fail if a
 * search is already being processed with the supplied guid.  Check errors.
 *
 * @param guid  Original search id to forward back with the results.
 * @param src   Node that we will be replying back to.
 * @param dst   Node that we sent the new search to.
 */
FTSearchFwd *ft_search_fwd_new (ft_guid_t *guid, in_addr_t src, in_addr_t dst);

/**
 * Destroy a forward object.
 */
void ft_search_fwd_finish (FTSearchFwd *sfwd);

/**
 * Locate a forward object by guid and forwarded destination.  This operation
 * is a lookup, not a find.
 */
FTSearchFwd *ft_search_fwd_find (ft_guid_t *guid, in_addr_t dst);

/*****************************************************************************/

#endif /* __FT_SEARCH_OBJ_H */