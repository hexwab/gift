/*
 * $Id: ft_netorg.h,v 1.14 2004/08/02 23:59:27 hexwab Exp $
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

#ifndef __FT_NETORG_H
#define __FT_NETORG_H

/*****************************************************************************/

/**
 * @file ft_netorg.h
 *
 * @brief Core node organization model.
 *
 * Maintains the internal node data structures so that they may be traversed,
 * manipulated, etc.  Also contains a great deal of actual network
 * organization code utilizing the function set defined here.
 */

/*****************************************************************************/

/**
 * Callback routine for ::ft_netorg_foreach.
 *
 * @param node   Currently iterating node.
 * @param udata  Arbitrary user data.
 *
 * @return Boolean value indicating whether or not the iterated node was
 *         processed.  This matters when specifying an \em iter count to the
 *         foreach function, as it will be decremented only when this
 *         callback returns TRUE.
 */
typedef BOOL (*FTNetorgForeach) (FTNode *node, void *udata);
#define FT_NETORG_FOREACH(func) ((FTNetorgForeach)func)

/*****************************************************************************/

/**
 * Adds a new node structure to the node organization tables.  This function
 * implicitly reads the current class and state when selecting which table to
 * be used.
 *
 * @note Sorting by "priority" (the basic usefulness of this node to us,
 *       generally determined by total uptime and last connection time index)
 *       occurs at insertion.  This is done so that iteration calls produce
 *       the most useful results first.
 */
void ft_netorg_add (FTNode *node);

/**
 * Remove a node from all associated node organization tables.  This does not
 * apply any logic to the node structure itself, including memory management.
 */
void ft_netorg_remove (FTNode *node);

/**
 * Register a change in classification and/or state.  This will effectively
 * "move" the node structure from one table to another rather inefficiently
 * as \em node is unaware of it's location until a lookup is performed.  If
 * you do not use this function, or call it in a laggy fashion,
 * ::ft_netorg_foreach will not filter nodes properly.
 */
void ft_netorg_change (FTNode *node,
                       ft_class_t class_orig,
                       ft_state_t state_orig);

/**
 * Retrieve an organized node structure given the ip address.  Please note
 * that it is quite possible that this function is not well optimized and
 * should be considered expensive.  This may change in the future, but you
 * should not rely on that.
 */
FTNode *ft_netorg_lookup (in_addr_t ip);

/**
 * Loop through each node (filtered by \em klass and \em state) calling \em
 * func for each iteration.  You may apply additional filtering by having the
 * appropriate logic exist within the function callback, and returning a
 * meaningful value from your iterator function (\em func).
 *
 * @param klass  Basic class filter.
 * @param state  Basic state filter.  This actually selects which lists
 *               to operate on to improve search times when a large number
 *               of disconnected nodes exist in the current table.
 * @param iter   Maximum number of successfully matched nodes to iterate.
 * @param func
 * @param udata
 *
 * @return Number of nodes processed.  If \em iter is supplied, the return
 *         value will never exceed the supplied value.
 */
int ft_netorg_foreach (ft_class_t klass, ft_state_t state, int iter,
                       FTNetorgForeach func, void *udata);

/**
 * Does the same as ft_netorg_foreach but starts at a random position in each
 * list instead of the last access / beginning.
 */
int ft_netorg_random (ft_class_t klass, ft_state_t state, int iter,
                      FTNetorgForeach func, void *udata);


/**
 * Efficient means of accessing the total length of the list that would be
 * iterated over using a similar call to ::ft_netorg_foreach.  This routine
 * accesses a separately maintained data structure which has the length
 * readily available (with a small amount of constant iteration).
 *
 * @return Number of nodes matched in the selected set.
 */
int ft_netorg_length (ft_class_t klass, ft_state_t state);

/**
 * Wrapper around ::ft_netorg_foreach used for destroying every node
 * currently in the organization tables.
 *
 * @note If \em func returns FALSE, the node is not destroyed.  It is
 *       generally assumed that you will not need this functionality, however.
 *
 * @param func
 * @param udata
 */
void ft_netorg_clear (FTNetorgForeach func, void *udata);

/**
 * Debugging function which will dump all nodes to the log file.
 */
void ft_netorg_dump (void);

/*****************************************************************************/

#endif /* __FT_NETORG_H */
