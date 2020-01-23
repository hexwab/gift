/*
 * ft_node_cache.h
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

#ifndef __FT_NODE_CACHE_H
#define __FT_NODE_CACHE_H

/*****************************************************************************/

/**
 * @file ft_node_cache.h
 *
 * @brief Handle the nodes cache file used for network reentry each time the
 *        protocol starts.
 */

/*****************************************************************************/

/**
 * Update the ~/.giFT/OpenFT/nodes cache file.  This function both handles
 * re-reading changes made to the file as well as writing changes made in
 * memory.  Call it often.
 *
 * @return Number of nodes successfully written to the cache.
 */
int ft_node_cache_update ();

/*****************************************************************************/

#endif /* FT_NODE_CACHE_H */