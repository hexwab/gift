/*
 * $Id: fst_node.c,v 1.24 2004/11/29 14:46:57 mkern Exp $
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

#include "fst_fasttrack.h"
#include "fst_node.h"

/* #define NODECACHE_DEBUG */

/*****************************************************************************/

/* This is the magic function which sorts our node cache.
 * The current strategy is to first sort by last_seen with 5 minute precision
 * and then by load.
 */
static int nodecache_cmp_nodes (FSTNode *a, FSTNode *b)
{
	/* compare with 5 minute accuracy */
	if ( (a->last_seen / 300) == (b->last_seen / 300))
	/* IMPORTANT: We are sorting for the highest load! It seems load is 
	 * misslabeled and is actually the remaining capacity of the node. This
	 * needs verification!
	 */
	{
		int aa = a->load * (100 - a->load), bb = b->load * (100 - b->load);
		return (aa > bb) ? -1 : (aa < bb);
	}

	else if (a->last_seen > b->last_seen)
		return -1;

	else
		return 1;
}

/*****************************************************************************/

/* Alloc new node with refcount 1 */
FSTNode *fst_node_create (void)
{
	FSTNode *node;

	if (!(node = malloc (sizeof(FSTNode))))
		return NULL;
	
	node->host = NULL;
	node->session = NULL;
	node->link = NULL;
	node->ref = 1;

	return node;
}

/* init/update node info */
void fst_node_init (FSTNode *node, FSTNodeKlass klass, char *host,
			unsigned short port, unsigned int load,
			unsigned int last_seen)
{
	node->klass = klass;
	free (node->host);
	node->host = strdup (host);
	node->port = port;
	node->load = load;
	node->last_seen = last_seen;
}

/* Increment node's reference count and return it. */
int fst_node_addref (FSTNode *node)
{
	if (!node)
		return 0;

	assert (node->ref > 0);
	node->ref++;

#ifdef NODECACHE_DEBUG
	FST_DBG_3("addref'd %p, ref=%d, link=%p", node, node->ref, node->link);
#endif

	return node->ref;
}

/* Decrement node's reference count and free if it goes to zero. Returns new
 * reference count.
 */
int fst_node_release (FSTNode *node)
{
	if (!node)
		return 0;

	assert (node->ref > 0);

	if (--node->ref == 0)
	{
		assert (node->link == NULL);
		assert (node->session == NULL);

#ifdef NODECACHE_DEBUG
	FST_DBG_3("freed %p, ref=%d, link=%p", node, node->ref, node->link);
#endif

		if (node->host)
			free (node->host);
		free (node);
		return 0;
	}

#ifdef NODECACHE_DEBUG
	FST_DBG_3("released %p, ref=%d, link=%p", node, node->ref, node->link);
#endif
	
	return node->ref;
}

/*****************************************************************************/

/* alloc and init node cache */
FSTNodeCache *fst_nodecache_create ()
{
	FSTNodeCache *cache = malloc (sizeof (FSTNodeCache));

	cache->list = NULL;
	cache->last = NULL;
	cache->hash = dataset_new (DATASET_HASH);

	return cache;
}

/* remove node */
static int nodecache_free_node (FSTNode *node, void *udata)
{
	node->link = NULL;

	fst_node_release (node);
	return TRUE;
}

/* free node cache */
void fst_nodecache_free (FSTNodeCache *cache)
{
	if (!cache)
		return;

	cache->list = list_foreach_remove (cache->list,
	                                   (ListForeachFunc)nodecache_free_node,
	                                   NULL);
	dataset_clear (cache->hash);
	free (cache);
}

/*****************************************************************************/

/* Create and add node to front of cache. If node with the same ip already
 * present it is updated and returned. Refcount is not incremented.
 */
FSTNode *fst_nodecache_add (FSTNodeCache *cache, FSTNodeKlass klass, char *host,
                            unsigned short port, unsigned int load,
                            unsigned int last_seen)
{
	FSTNode *node;

	node = dataset_lookupstr (cache->hash, host);

	if (node)
	{
		/* update old node */
		fst_node_init (node, klass, host, port, load, last_seen);

		/* move to new position */
		fst_nodecache_move (cache, node, NodeInsertFront);
	}
	else
	{
		/* create new node */
		if (!(node = fst_node_create ()))
			return NULL;
	
		fst_node_init (node, klass, host, port, load, last_seen);

		/* insert node at front */
		cache->list = list_prepend (cache->list, node);
		node->link = cache->list;

		if (!cache->last)
			cache->last = cache->list;	

		/* insert node into hash table */
		dataset_insert (&cache->hash, node->host, strlen (node->host) + 1,
		                node, 0);

	}

#ifdef NODECACHE_DEBUG
	FST_DBG_5("cache_add: %s:%d node=%p, link=%p, ref=%p", host, port, node,
	          node->link, node->ref);
#endif

	return node;
}

#ifdef NODECACHE_DEBUG
static int cmp_hosts (FSTNode *a, FSTNode *b)
{
	if (a->host == b->host)
		return 0;
	return -1;
}
#endif

/* Move node already in cache to new pos. Returns FALSE if node is not in
 * cache. Refcount remains unchanged.
 */
BOOL fst_nodecache_move (FSTNodeCache *cache, FSTNode *node,
                         FSTNodeInsertPos pos)
{
#ifdef NODECACHE_DEBUG
	FSTNode *org_node;
	List *l;
#endif

	if (!node->link)
		return FALSE;

#ifdef NODECACHE_DEBUG
	org_node = dataset_lookupstr (cache->hash, node->host);
	assert (org_node);
	assert (node == org_node);
	l = list_find_custom (cache->list, node, (CompareFunc)cmp_hosts);
	assert (l && org_node == l->data);
#endif

	fst_node_addref (node); /* still need it after fst_nodecache_remove */
	fst_nodecache_remove (cache, node);

	/* ickiness */
	if (!cache->list)
		pos = NodeInsertFront;
	else
		assert (cache->last);

	/* insert into linked list */
	switch (pos)
	{
	case NodeInsertFront:
		cache->list = list_prepend (cache->list, node);

		if (!cache->last)
			cache->last = cache->list;
		
		node->link = cache->list;
		break;

	case NodeInsertBack:
		/* ickiness to avoid traversing the entire list */
		list_append (cache->last, node);

		cache->last = list_last (cache->last);
		assert (cache->last);
		node->link = cache->last;
		break;

	case NodeInsertSorted:
		cache->list = list_insert_sorted (cache->list,
		                                  (CompareFunc) nodecache_cmp_nodes,
		                                  node);

		/* this is insane... despite having just inserted it, we have
		 * no way of getting the link without searching the entire
		 * list again! */
		node->link = list_find (cache->list, node);
		assert (node->link);

		if (!node->link->next)
			cache->last = node->link;
		break;
	}
	
#ifdef NODECACHE_DEBUG
	assert (node->link->data == node);
	assert (!cache->last->next);
	assert (!cache->list || !cache->last ||
	        list_find (cache->list, cache->last->data));

	FST_DBG_2("set %p->link to %p", node, node->link);
#endif

	/* insert node into hash table */
	dataset_insert (&cache->hash, node->host, strlen (node->host) + 1,
	                node, 0);

	assert (node->ref > 0);

	return TRUE;
}

/* Remove node from node cache and release it. */
void fst_nodecache_remove (FSTNodeCache *cache, FSTNode *node)
{
	if (!node)
		return;

	if (node->link)
	{
		/* bleah, we don't get to know if this worked or not */
		dataset_removestr (cache->hash, node->host);

		/* more ickiness */
		if (node->link == cache->last)
			cache->last = node->link->prev;

		cache->list = list_remove_link (cache->list, node->link);
		assert (cache->last || !cache->list);

#ifdef NODECACHE_DEBUG
		FST_DBG_2("nullified %p->link (was %p)", node, node->link);
#endif
		node->link = NULL;

		fst_node_release (node);
	}
}

/* Increment ref count of front node and return it. */
FSTNode *fst_nodecache_get_front (FSTNodeCache *cache)
{
	if (!cache->list)
		return NULL;
	
	fst_node_addref ((FSTNode*)cache->list->data);
	return (FSTNode*)cache->list->data;
}

/*
 * sort nodecache moving best nodes to the front and
 * clipping to FST_MAX_NODESFILE_SIZE
 */
unsigned int fst_nodecache_sort (FSTNodeCache *cache)
{
	List *list;

	if (!cache->list)
		return 0;

	/* sort list */
	cache->list = list_sort (cache->list, (CompareFunc)nodecache_cmp_nodes);

	/* clip everything below FST_MAX_NODESFILE_SIZE */
	list = list_nth (cache->list, FST_MAX_NODESFILE_SIZE - 1);
	while (list && list->next)
	{
		FSTNode *node = (FSTNode*)list->next->data;

		/* don't remove index nodes */
		if (node->klass == NodeKlassIndex)
		{
			list = list->next;
			continue;
		}

		/* remove node from hash table */
		dataset_removestr (cache->hash, node->host);

		/* remove list entry */
		assert (node->link == list->next);
		cache->list = list_remove_link (cache->list, list->next);

#ifdef NODECACHE_DEBUG
		FST_DBG_2("nullified %p->link (was %p)", node, node->link);
#endif
		node->link = NULL;

		/* free node */
		fst_node_release (node);
	}

	cache->last = list ? list : list_last (cache->list);
	assert (cache->last && !cache->last->next);

	return list_length (cache->list);
}

/* returns number of nodes currently in node cache */
unsigned int fst_nodecache_size(FSTNodeCache *cache)
{
	return list_length (cache->list);
}

/*****************************************************************************/

/* load nodes from file, returns number of loaded nodes or -1 on failure */
int fst_nodecache_load (FSTNodeCache *cache, const char *filename)
{
	FILE	*f;
	char	*buf = NULL;

	if ( (f = fopen (filename, "r")) == NULL)
		return -1;

	while (file_read_line (f, &buf))
	{
		char			*host;
		unsigned short	port;
		FSTNodeKlass	klass;
		unsigned int	load, last_seen;
		char			*ptr = buf;

		string_trim (ptr);

		/* ingore comments */
		if (*ptr == '#')
			continue;

		/* format: <host> <port> <klass> <load> <last_seen> */

		host		=					string_sep (&ptr, " ");
		port		=	(unsigned short)ATOUL(string_sep (&ptr, " "));
		klass		=					ATOUL(string_sep (&ptr, " "));
		load		=					ATOUL(string_sep (&ptr, " "));
		last_seen	=					ATOUL(string_sep (&ptr, " "));

		if (!host || !port)
			continue;

		fst_nodecache_add (cache, klass, host, port, load, last_seen);
	}

	fclose (f);

	/* sort and return number of loaded nodes */
	return fst_nodecache_sort (cache);
}

/* save nodes to file, returns number of saved nodes or -1 on failure */
int fst_nodecache_save (FSTNodeCache *cache, const char *filename)
{
	FILE	*f;
	List	*list;
	int		i;

	if ( (f = fopen (filename, "w")) == NULL)
		return -1;

	/* sort and clip to max FST_MAX_NODESFILE_SIZE nodes */
	i = fst_nodecache_sort (cache);

	/* save nodes */
	fprintf (f, "# <host> <port> <klass> <load> <last_seen>\n");

	for (list = cache->list; list; list = list_next (list))
	{
		fprintf (f, "%s %d %d %d %d\n", ( (FSTNode*)list->data)->host,
										( (FSTNode*)list->data)->port,
										( (FSTNode*)list->data)->klass,
										( (FSTNode*)list->data)->load,
										( (FSTNode*)list->data)->last_seen);
	}

	fclose (f);

	return i;
}

/*****************************************************************************/
