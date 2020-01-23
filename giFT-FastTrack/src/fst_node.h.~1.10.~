/*
 * $Id: fst_node.h,v 1.10 2004/07/23 19:26:52 hex Exp $
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
} FSTNodeCache;

typedef enum
{
	NodeInsertFront,
	NodeInsertBack,
	NodeInsertSorted
} FSTNodeInsertPos;

/*****************************************************************************/

/* alloc node */
FSTNode *fst_node_new (void);

/* init node */
void fst_node_init (FSTNode *node, FSTNodeKlass klass, char *host,
		    unsigned short port, unsigned int load,
		    unsigned int last_seen);

/* alloc and create copy of node */
FSTNode *fst_node_create_copy (FSTNode *org_node);

/* ref node */
void fst_node_ref (FSTNode *node);

/* free node */
BOOL fst_node_free (FSTNode *node);

/*****************************************************************************/

/* alloc and init node cache */
FSTNodeCache *fst_nodecache_create ();

/* free node cache */
void fst_nodecache_free (FSTNodeCache *cache);

/*****************************************************************************/

/* create and add node to front of cache */
FSTNode *fst_nodecache_add (FSTNodeCache *cache, FSTNodeKlass klass, char *host,
					    unsigned short port, unsigned int load,
					    unsigned int last_seen);

/* Insert copy of node at pos. If node is already in the cache it is moved. */
void fst_nodecache_insert (FSTNodeCache *cache, FSTNode *node,
                           FSTNodeInsertPos pos);

/* remove node from node cache and free it */
void fst_nodecache_remove (FSTNodeCache *cache, FSTNode *node);

/* returns _copy_ of the first node, caller must free returned copy */
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
