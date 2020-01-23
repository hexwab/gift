/*
 * $Id: fst_node.c,v 1.5 2003/07/04 03:54:45 beren12 Exp $
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
}

/*****************************************************************************/

/* add node to node cache */
void fst_nodecache_add (FSTNodeCache *cache, FSTNodeKlass klass, char *host,
						unsigned short port, unsigned int load,
						unsigned int last_seen)
{
	FSTNode *node;

	if ( (node = dataset_lookupstr (cache->hash, host)))
	{
		/* update node */
		node->klass = klass;
		node->port = port;
		node->load = load;
		node->last_seen = last_seen;
	}
	else
	{	
		/* create new node */
		node = fst_node_create (klass, host, port, load, last_seen);
		/* insert node into list and hash table */
		cache->list = list_prepend (cache->list, node);
		dataset_insert (&cache->hash, node->host, strlen (node->host) + 1, node, 0);	
	}
}

/* remove node from node cache by host and free it */
void fst_nodecache_remove (FSTNodeCache *cache, char *host)
{
	FSTNode *node;

	if ( (node = dataset_lookupstr (cache->hash, host)))
	{
		/* remove node */
		cache->list = list_remove (cache->list, node);
		dataset_removestr (cache->hash, node->host);
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

static int nodecache_cmp_nodes (FSTNode *a, FSTNode *b)
{
	/* compare with 3 minute accuracy */
	if ( (a->last_seen / 180) == (b->last_seen / 180))
		return (a->load < b->load) ? -1 : (a->load > b->load);

	else if (a->last_seen < b->last_seen)
		return -1;

	else
		return 1;
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
		/* remove node from hash table */
		dataset_removestr (cache->hash, ( (FSTNode*)list->next->data)->host);
		/* free node */
		fst_node_free ( (FSTNode*)list->next->data);
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
	char	*ptr;

	if ( (f = fopen (filename, "r")) == NULL)
		return -1;

	while (file_read_line (f, &buf))
	{
		char			*host;
		unsigned short	port;
		FSTNodeKlass	klass;
		unsigned int	load, last_seen;

		ptr = buf;

		if (strchr (ptr,'#')) /* ingore comments */
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
