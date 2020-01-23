/*
 * $Id: ft_search.h,v 1.13 2003/05/05 09:49:10 jasta Exp $
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

#ifndef __FT_SEARCH_H
#define __FT_SEARCH_H

/*****************************************************************************/

/**
 * @file ft_search.h
 *
 * @brief Handles search, browse, and locate iteraction with giFT.
 *
 * This subsystem is mostly responsible for using the various search APIs
 * defined in the rest of the source and wrapping them up for replies to the
 * daemon.
 */

/*****************************************************************************/

#include "ft_search_obj.h"

/*****************************************************************************/

/**
 * Reply to interface protocol.  If file is NULL, the call is considered the
 * end of the search.
 *
 * @param search
 * @param file         Search result itself.
 * @param parent       Node that originally supplied this result.
 */
void ft_search_reply (FTSearch *search, FileShare *file, FTNode *parent);

/**
 * Similar to ::ft_search_reply, except that fewer sanity checks are
 * performed as they are mostly guaranteed within ft_search_obj.h or
 * ::ft_browse_response.
 */
void ft_browse_reply (FTBrowse *browse, FileShare *file);

/*****************************************************************************/

/**
 * Handle search.  Please note that \em meta is currently unimplemented.
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
int openft_locate (Protocol *p, IFEvent *event, char *htype, char *hash);

/**
 * Handle search/browse/locate cancellation.
 */
void openft_search_cancel (Protocol *p, IFEvent *event);

/*****************************************************************************/

#endif /* __FT_SEARCH_H */
