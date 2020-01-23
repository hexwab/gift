/*
 * $Id: fst_node.c,v 1.16 2004/03/20 13:44:42 mkern Exp $
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
		return (a->load > b->load) ? -1 : (a->load < b->load);

	else if (a->last_seen > b->last_seen)
		return -1;

	else
		return 1;
}

/*****************************************************************************/

/* alloc and init node */
FSTNode *fst_node_create (FSTNodeKlass klass, char *host, unsigned short port,
						  unsigned int load, unsigned int last_seen)
{
	FSTNode *node = malloc (sizeof(FSTNode));

	node->klass = klass;
	node->host = strdup (host);
	node->port = port;
	node->load = load;
	node->last_seen = last_seen;

	return node;
}

/* alloc and create copy of node */
FSTNode *fst_node_create_copy (FSTNode *org_node)
{
	FSTNode *node = malloc (sizeof(FSTNode));

	node->klass = org_node->klass;
	node->host = strdup (org_node->host);
	node->port = org_node->port;
	node->load = org_node->load;
	node->last_seen = org_node->last_seen;

	return node;
}

/* free node */
void fst_node_free (FSTNode *node)
{
	if (!node)
		return;

	if (node->host)
		free (node->host);
	free (node);
}

/*****************************************************************************/

/* alloc and init node cache */
FSTNodeCache *fst_nodecache_create ()
{
	FSTNodeCache *cache = malloc (sizeof (FSTNodeCache));

	cache->list = NULL;
	cache->hash = dataset_new (DATASET_HASH);

	return cache;
}

/* remove node */
static int nodecache_free_node (FSTNode *node, void *udata)
{
	fst_node_free (node);
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

/* create and add node to front of cache */
void fst_nodecache_add (FSTNodeCache *cache, FSTNodeKlass klass, char *host,
						unsigned short port, unsigned int load,
						unsigned int last_seen)
{
	FSTNode *node;

	/* create node*/
	node = fst_node_create (klass, host, port, load, last_seen);

	/* insert at front */
	fst_nodecache_insert (cache, node, NodeInsertFront);

	/* free node */
	fst_node_free (node);
}

/* Insert copy of node at pos. If node is already in the cache it is moved. */
void fst_nodecache_insert (FSTNodeCache *cache, FSTNode *node,
                           FSTNodeInsertPos pos)
{
	FSTNode *node_cpy;

	/* remove old node if present */
	fst_nodecache_remove (cache, node->host);

	/* drop nodes with too high/low a load */
	if (node->load < FST_NODE_MIN_LOAD || node->load > FST_NODE_MAX_LOAD)
		return;

	/* make copy of node */
	node_cpy = fst_node_create_copy (node);

	/* insert into linked list */
	switch (pos)
	{
	case NodeInsertFront:
		cache->list = list_prepend (cache->list, node_cpy);
		break;

	case NodeInsertBack:
		/* this involves traversing the entire list! */
		cache->list = list_append (cache->list, node_cpy);
		break;

	case NodeInsertSorted:
		cache->list = list_insert_sorted (cache->list,
		                                  (CompareFunc) nodecache_cmp_nodes,
		                                  node_cpy);
		break;
	}

	/* insert link into hash table */
	dataset_insert (&cache->hash, node_cpy->host, strlen (node_cpy->host) + 1,
	                node_cpy, 0);	
}


/* remove node from node cache by host and free it */
void fst_nodecache_remove (FSTNodeCache *cache, char *host)
{
	FSTNode *node;

	if ( (node = dataset_lookupstr (cache->hash, host)))
	{
		/* remove node */
		dataset_removestr (cache->hash, node->host);
		cache->list = list_remove (cache->list, node);
		fst_node_free (node);
	}
}

/* returns _copy_ of the first node, caller must free returned copy */
FSTNode *fst_nodecache_get_front (FSTNodeCache *cache)
{
	/* return copy of the front node */
	if (cache->list)
		return fst_node_create_copy ( (FSTNode*)cache->list->data);
	else
		return NULL;
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
		/* free node */
		fst_node_free (node);
		/* remove list entry */
		cache->list = list_remove_link (cache->list, list->next);
	}

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
