/*
 * $Id: fst_node.h,v 1.13 2004/11/11 14:31:56 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"

/*****************************************************************************/

typedef enum
{
	NodeKlassUser = 0x00,   /* simple user node not acting as supernode */
	NodeKlassSuper = 0x01,  /* supernode we use for searches */
	NodeKlassIndex = 0x02   /* index node we use to get supernode ips */
} FSTNodeKlass;

typedef struct
{
	FSTNodeKlass klass;

	char *host;
	unsigned short port;

	unsigned int load;        /* Load of node in percent.
	                             UPDATE: I now believe this is the remaining
	                             capacity of the node. Needs further
	                             investigation! */
	unsigned int last_seen;   /* time in seconds since the epoch nodes was
	                             last seen */

	List *link;               /* where this node is in the list */

	struct _FSTSession *session;      /* active session to this node, if any */

	unsigned int ref;         /* reference count */
} FSTNode;

typedef struct
{
	List *list;      /* The list which holds all nodes. When connecting the
	                  * first node is used.
	                  */

	Dataset *hash;   /* A hash table keyed by host and pointing to the same
					  * node the list contains to efficiently prevent
					  * duplicates.
	                  */

	List *last;      /* last entry of the list, to work around deficiencies
	                  * in giFT's linked list implementation */
} FSTNodeCache;

typedef enum
{
	NodeInsertFront,
	NodeInsertBack,
	NodeInsertSorted
} FSTNodeInsertPos;

/*****************************************************************************/

/* Alloc new node with refcount 1 */
FSTNode *fst_node_create (void);

/* Init node. Does not change ref count. */
void fst_node_init (FSTNode *node, FSTNodeKlass klass, char *host,
		    unsigned short port, unsigned int load,
		    unsigned int last_seen);

/* Increment node's reference count and return it. */
int fst_node_addref (FSTNode *node);

/* Decrement node's reference count and free if it goes to zero. Returns new
 * reference count.
 */
int fst_node_release (FSTNode *node);

/*****************************************************************************/

/* alloc and init node cache */
FSTNodeCache *fst_nodecache_create ();

/* free node cache */
void fst_nodecache_free (FSTNodeCache *cache);

/*****************************************************************************/

/* Create and add node to front of cache. If node with the same ip already
 * present it is updated and returned. Refcount is not incremented.
 */
FSTNode *fst_nodecache_add (FSTNodeCache *cache, FSTNodeKlass klass, char *host,
                            unsigned short port, unsigned int load,
                            unsigned int last_seen);

/* Move node already in cache to new pos. Returns FALSE if node is not in
 * cache. Refcount remains unchanged.
 */
BOOL fst_nodecache_move (FSTNodeCache *cache, FSTNode *node,
                         FSTNodeInsertPos pos);

/* Remove node from node cache and release it. */
void fst_nodecache_remove (FSTNodeCache *cache, FSTNode *node);

/* Increment ref count of front node and return it. */
FSTNode *fst_nodecache_get_front (FSTNodeCache *cache);

/*
 * sort nodecache moving best nodes to the front and
 * clipping to FST_MAX_NODESFILE_SIZE
 */
unsigned int fst_nodecache_sort (FSTNodeCache *cache);

/* returns number of nodes currently in node cache */
unsigned int fst_nodecache_size (FSTNodeCache *cache);

/*****************************************************************************/

/* load nodes from file, returns number of loaded nodes or -1 on failure */
int fst_nodecache_load (FSTNodeCache *cache, const char *filename);

/* save nodes to file, returns number of saved nodes or -1 on failure */
int fst_nodecache_save (FSTNodeCache *cache, const char *filename);

/*****************************************************************************/

#endif /* __FST_NODE_H__ */
