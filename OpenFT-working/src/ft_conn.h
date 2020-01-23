/*
 * $Id: ft_conn.h,v 1.10 2003/12/23 04:54:45 jasta Exp $
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

#ifndef __FT_CONN_H
#define __FT_CONN_H

/*****************************************************************************/

/**
 * @file ft_conn.h
 *
 * @brief Network connection management.
 *
 * We need to write a great deal more documentation here.  Someone please
 * poke me to do this at some later date.
 */

/*****************************************************************************/

/**
 * Main connection maintenance timer used to keep the network connectivity
 * state up.  We will also manage most of the actual network organization
 * logic within this function as well.
 *
 * @param udata  Only here because this is a timer callback.  Not used.
 *
 * @return Always TRUE to satisfy the timer.
 */
int ft_conn_maintain (void *udata);

/**
 * One-time function called from within ft_openft.c to actually "start" the
 * connections once loaded into the netorg subsystem.
 *
 * @return Boolean success or failure.  If FALSE, OpenFT will be unable to
 *         connect to the network and giFT will die.
 */
BOOL ft_conn_initial (void);

/**
 * Authorize any connection (regardless of direction).  This takes into
 * account things like connection throttling, classification priority, etc.
 *
 * @param node
 * @param outgoing  Direction of the connection.
 *
 * @return Boolean success or failure.  If FALSE, the connection should not
 *         be allowed.  That is, it should be immediately closed.
 */
BOOL ft_conn_auth (FTNode *node, int outgoing);

/*****************************************************************************/

/**
 * Determines whether or not more parent nodes are required.
 */
BOOL ft_conn_need_parents (void);

/**
 * Same as ::ft_conn_need_parents, except checks search node peer connections
 * (this applies to search nodes only).
 */
BOOL ft_conn_need_peers (void);

/**
 * Same as ::ft_conn_need_parents, except checks FT_NODE_INDEX connections.
 */
BOOL ft_conn_need_index (void);

/**
 * Returns the number of open children slots on the current node.  Returns 0
 * if not running as a search node.
 */
int ft_conn_children_left (void);

/*****************************************************************************/

#endif /* __FT_CONN_H */
