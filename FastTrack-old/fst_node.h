/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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

#ifndef __FST_NODE_H__
#define __FST_NODE_H__

#include "list.h"
#include "dataset.h"

/**************************************************************************/

typedef enum
{
	NodeKlassUser = 0x00,		// simple user node not acting as supernode
	NodeKlassSuper = 0x01,		// supernode we use for searches
	NodeKlassIndex = 0x02,		// index node we use to get supernode ips
} FSTNodeKlass;

typedef struct
{
	char *host;
	unsigned short port;
	
	FSTNodeKlass klass;

} FSTNode;


typedef struct
{
	List *list;			// we keep a list in which we instert and remove modes from the beginning...
	Dataset *hash;		// ...and a hash table keyed by ip to prevent duplicates efficiently

} FSTNodeCache;

/*****************************************************************************/

// alloc and init node
FSTNode *fst_node_create(char* host, unsigned short port, FSTNodeKlass klass);

// alloc and create copy of node
FSTNode *fst_node_create_copy(FSTNode *org_node);

// free node
void fst_node_free(FSTNode *node);

/*****************************************************************************/

// alloc and init node cache
FSTNodeCache *fst_nodecache_create();

// free node cache
void fst_nodecache_free(FSTNodeCache *cache);

/*****************************************************************************/

// add node to node cache
void fst_nodecache_add(FSTNodeCache *cache, char* host, unsigned short port, FSTNodeKlass klass);

// remove node from node cache by host and free it
void fst_nodecache_remove(FSTNodeCache *cache, char* host);

// returns _copy_ of the newest node, caller must free returned copy
FSTNode *fst_nodecache_get_freshest(FSTNodeCache *cache);

// returns number of nodes currently in node cachen
int fst_nodecache_size(FSTNodeCache *cache);

/*****************************************************************************/

// load nodes from file, returns number of loaded nodes or -1 on failure
int fst_nodecache_load (FSTNodeCache *cache, const char *filename);

// save nodes to file, returns number of saved nodes or -1 on failure
int fst_nodecache_save (FSTNodeCache *cache, const char *filename);

/*****************************************************************************/

#endif /* __FST_NODE_H__ */
