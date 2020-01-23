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

#include "fst_fasttrack.h"
#include "fst_node.h"

/*****************************************************************************/

// alloc and init node
FSTNode *fst_node_create(char* host, unsigned short port, FSTNodeKlass klass)
{
	FSTNode *node = malloc (sizeof(FSTNode));

	node->host = strdup (host);
	node->port = port;
	node->klass = klass;

	return node;
}

// alloc and create copy of node
FSTNode *fst_node_create_copy(FSTNode *org_node)
{
	FSTNode *node = malloc (sizeof(FSTNode));

	node->host = strdup (org_node->host);
	node->port = org_node->port;
	node->klass = org_node->klass;

	return node;
}

// free node
void fst_node_free(FSTNode *node)
{
	if(!node)
		return;

	if(node->host)
		free (node->host);
	free (node);
}

/*****************************************************************************/

// alloc and init node cache
FSTNodeCache *fst_nodecache_create()
{
	FSTNodeCache *cache = malloc (sizeof(FSTNodeCache));

	cache->list = NULL;
	cache->hash = dataset_new (DATASET_HASH);

	return cache;
}

static int nodecache_free_node(FSTNode *node, void *udata)
{
	fst_node_free (node);
	return TRUE; // remove node
}

// free node cache
void fst_nodecache_free(FSTNodeCache *cache)
{
	if(!cache)
		return;

	cache->list = list_foreach_remove (cache->list, (ListForeachFunc)nodecache_free_node, NULL);
	dataset_clear (cache->hash);
}

/*****************************************************************************/

// add node to node cache
void fst_nodecache_add(FSTNodeCache *cache, char* host, unsigned short port, FSTNodeKlass klass)
{
	FSTNode* node;

	if((node = dataset_lookupstr (cache->hash, host)))
	{
		// remove node from current position in list
		cache->list = list_remove (cache->list, node);
		// update port and supernode status
		node->port = port;
		node->klass = klass;
		// insert node at beginning of list
		cache->list = list_prepend (cache->list, node);
	}
	else
	{	
		node = fst_node_create (host, port, klass);

		// insert node into list and hash table
		cache->list = list_prepend (cache->list, node);
		dataset_insert (&cache->hash, node->host, strlen(node->host) + 1, node, 0);	
	}

}

// remove node from node cache by host and free it
void fst_nodecache_remove(FSTNodeCache *cache, char* host)
{
	FSTNode *node;

	if((node = dataset_lookupstr (cache->hash, host)))
	{
		// remove node
		cache->list = list_remove (cache->list, node);
		dataset_removestr (cache->hash, node->host);
	}
}

// returns _copy_ of the newest node, caller must free returned copy
FSTNode *fst_nodecache_get_freshest(FSTNodeCache *cache)
{
	// return copy of most recent node
	if(cache->list)
		return fst_node_create_copy ((FSTNode*)cache->list->data);
	else
		return NULL;
}

// returns number of nodes currently in node cachen
int fst_nodecache_size(FSTNodeCache *cache)
{
	return list_length (cache->list);
}

/*****************************************************************************/

// load nodes from file, returns number of loaded nodes or -1 on failure
int fst_nodecache_load (FSTNodeCache *cache, const char *filename)
{
	FILE *f;
	char *buf = NULL;
	char *ptr;

	if ((f = fopen (filename, "r")) == NULL)
		return -1;

	while (file_read_line (f, &buf))
	{
		char *host;
		unsigned short port;
		FSTNodeKlass klass;

		ptr = buf;

		if(strchr (ptr,'#')) // ingore comments
			continue;

		/* [host] [port] [is_supernode] */

		host    =                       string_sep (&ptr, " ");
		port    = (unsigned short) ATOI(string_sep (&ptr, " "));
		klass	=                  ATOI(string_sep (&ptr, " "));

		if (!host || !port)
			continue;

		fst_nodecache_add (cache, host, port, klass);
	}

	fclose (f);

	return fst_nodecache_size (cache);
}

// save nodes to file, returns number of saved nodes or -1 on failure
int fst_nodecache_save (FSTNodeCache *cache, const char *filename)
{
	FILE *f;
	List *list;

	if ((f = fopen (filename, "w")) == NULL)
		return -1;

	// save in reverse order because loading reverses order too
	// this means most freshest node is at the end of the nodes file
	// also save a max of FST_MAX_NODESFILE_SIZE nodes
	if((list = list_nth (cache->list, FST_MAX_NODESFILE_SIZE)) == NULL)
		list = list_last (cache->list);

	for(; list; list = list->prev)
	{
		fprintf (f, "%s %d %d\n", ((FSTNode*)list->data)->host, ((FSTNode*)list->data)->port, ((FSTNode*)list->data)->klass);
	}

	fclose (f);

	return fst_nodecache_size (cache);
}

