/*
 * $Id: gt_node_list.c,v 1.12 2004/01/29 07:50:25 hipnod Exp $
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

#include "gt_gnutella.h"

#include "gt_node.h"
#include "gt_node_list.h"

/*
 * TODO: rename gt_conn -> gt_node_list
 */

/*****************************************************************************/

/* list of all nodes -- NOTE: duplicated info in gt_node.c */
static List      *node_list;

/* last place in node_list for gt_conn_foreach */
static List      *iterator;

/* cache of length of connected portion of node list by class (GtNodeLeaf,
 * first, then GtNodeUltra) */
static int        len_cache[2] = { 0 };

/*****************************************************************************/

static void move_iterator (GtNode *mv)
{
	if (list_nth_data (iterator, 0) == mv)
		iterator = list_next (iterator);
}

void gt_conn_add (GtNode *node)
{
	if (!node)
	{
		GIFT_ERROR (("adding null node to node list"));
		return;
	}

	node_list = list_append (node_list, node);
}

void gt_conn_remove (GtNode *node)
{
	if (!list_find (node_list, node))
		return;

	/* move the iterator if it's pointing at the node we're removing */
	move_iterator (node);

	node_list = list_remove (node_list, node);
}

static void trace_list (List *nodes)
{
	GtNode *node;

	if (!nodes)
		return;

	node = list_nth_data (nodes, 0);

	assert (node != NULL);
	assert (GT_CONN(node) != NULL);

	GT->DBGFN (GT, "%s:%hu", net_ip_str (node->ip), node->gt_port);

	if (list_next (nodes))
		trace_list (list_next (nodes));
}

/*****************************************************************************/

GtNode *gt_conn_foreach (GtConnForeachFunc func, void *udata,
                         gt_node_class_t klass, gt_node_state_t state,
                         int iter)
{
	GtNode      *node;
	TCPC        *c;
	GtNode      *ret       = NULL;
	List        *ptr;
	List        *start;
	List        *next;
	unsigned int i, count;
	int          looped    = FALSE;
	int          iterating = FALSE;

	assert (func != NULL);

#if 0
	GT->DBGFN (GT, "length of conn list: %u", list_length (connections));
#endif

	if (iter)
		iterating = TRUE;

	if (!iterator)
		iterator = node_list;

	start = ptr = (iterating) ? iterator : node_list;

	/* having count be the static list length should keep
	 * concurrent conn_adds from making us never stop */
	count = list_length (node_list);

	/* hack for backward-compatible interface */
	if (state == (gt_node_state_t) -1)
		state = GT_NODE_ANY;

	for (i = 0; i < count; i++)
	{
		if (iter && !ptr && !looped)
		{
			/* data only gets appended to connection list:
			 * safe to use head of connection list (connections) */
			ptr = node_list;
			looped = TRUE;
		}

		if (!ptr)
			break;

		/* we wrapped to the beginning, but have just reached the original
		 * start so we should bail out */
		if (looped && ptr == start)
			break;

		node = ptr->data;
		c = GT_CONN(node);

		assert (node != NULL);

		if (klass && !(node->klass & klass))
		{
			ptr = list_next (ptr);
			continue;
		}

		if (state != GT_NODE_ANY && node->state != state)
		{
			ptr = list_next (ptr);
			continue;
		}

		/* grab the next item. this allows the callback to free this item */
		next = list_next (ptr);

		ret = (*func) (c, node, udata);

		ptr = next;

		if (ret)
			break;

		if (iterating && --iter == 0)
			break;
	}

	/* save the position for next time */
	if (iterating)
		iterator = ptr;

	return ret;
}

/*****************************************************************************/

static void add_connected (gt_node_class_t klass)
{
	int add;

	if (klass != GT_NODE_LEAF && klass != GT_NODE_ULTRA)
		return;

	add = (klass == GT_NODE_LEAF ? 0 : 1);
	len_cache[add]++;
}

static void del_connected (gt_node_class_t klass)
{
	int del;

	if (klass != GT_NODE_LEAF && klass != GT_NODE_ULTRA)
		return;

	del = (klass == GT_NODE_LEAF ? 0 : 1);
	len_cache[del]--;
}

void gt_conn_set_state (GtNode *node, gt_node_state_t old_state,
                        gt_node_state_t new_state)
{
	if (new_state == GT_NODE_CONNECTED && old_state != GT_NODE_CONNECTED)
		add_connected (node->klass);

	if (new_state != GT_NODE_CONNECTED && old_state == GT_NODE_CONNECTED)
		del_connected (node->klass);
}

void gt_conn_set_class (GtNode *node, gt_node_class_t old_class,
                        gt_node_class_t new_class)
{
	if (node->state != GT_NODE_CONNECTED)
		return;

	del_connected (old_class);
	add_connected (new_class);

	assert (len_cache[0] >= 0);
	assert (len_cache[1] >= 0);
}

static int check_len_cache (gt_node_class_t klass)
{
	int len = 0;

	if (klass == GT_NODE_NONE)
		klass = GT_NODE_LEAF | GT_NODE_ULTRA;

	if (klass & GT_NODE_LEAF)
		len += len_cache[0];

	if (klass & GT_NODE_ULTRA)
		len += len_cache[1];

	return len;
}

static GtNode *conn_counter (TCPC *c, GtNode *node, int *length)
{
	(*length)++;
	return NULL;
}

int gt_conn_length (gt_node_class_t klass, gt_node_state_t state)
{
	int ret = 0;
	int cached_len;

	/*
	 * We keep a small cache of the length of connected ultrapeers+leaves
	 * so we don't have to traverse the list most of the time here.
	 *
	 * Traversal still happens now as a sanity check.
	 */
	if (state == GT_NODE_CONNECTED &&
	    (klass == GT_NODE_NONE || klass == GT_NODE_LEAF ||
		 klass == GT_NODE_ULTRA))
	{
		cached_len = check_len_cache (klass);

		/* make sure the cache length is the same as the real one */
		gt_conn_foreach (GT_CONN_FOREACH(conn_counter), &ret, klass, state, 0);
		assert (ret == cached_len);

		return cached_len;
	}

	gt_conn_foreach (GT_CONN_FOREACH(conn_counter), &ret,
	                 klass, state, 0);

	return ret;
}

/*****************************************************************************/

static GtNode *select_rand (TCPC *c, GtNode *node, void **cmp)
{
	int     *nr    = cmp[0];
	GtNode **ret   = cmp[1];
	float    range = *nr;
	float    prob;

	/* make sure we pick at least one */
	if (!*ret)
		*ret = node;

	/* set the probability of selecting this node */
	prob = range * rand() / (RAND_MAX + 1.0);

	if (prob < 1)
		*ret = node;

	(*nr)++;

	/* we dont use the return value here, because we need to try
	 * all the nodes, and returning non-null here short-circuits */
	return NULL;
}

/*
 * Pick a node at random that is also of the specified 
 * class and state.
 */
GtNode *gt_conn_random (gt_node_class_t klass, gt_node_state_t state)
{
	void   *cmp[2];
	int     nr;   
	GtNode *ret = NULL;

	/* initial probability */
	nr = 1;

	cmp[0] = &nr;
	cmp[1] = &ret;

	gt_conn_foreach (GT_CONN_FOREACH(select_rand), cmp,
	                 klass, state, 0);

	return ret;
}

/*****************************************************************************/

static BOOL collect_old (GtNode *node, void **data)
{
	List   **to_free    = data[0];
	int     *max_tofree = data[1];

	/* don't make the node list too small */
	if (*max_tofree == 0)
		return FALSE;

	if (gt_node_freeable (node))
	{
		/* protect the iterator because we're removing a node */
		move_iterator (node);

		(*max_tofree)--;
		*to_free = list_append (*to_free, node);

		return TRUE;
	}

	return FALSE;
}

static BOOL dump_node (GtNode *node, void *udata)
{
	gt_node_free (node);
	return TRUE;
}

/*
 * NOTE: We can't re-descend the node_list by calling gt_node_free [which
 * calls gt_conn_remove->list_remove] while iterating the node_list in
 * list_foreach_remove. So, we build a second a list of nodes to free while
 * removing those from node_list "by hand" and then free that list.
 */
void gt_conn_trim (void)
{
	List   *to_free     = NULL;
	int     max_tofree;
	int     len;
	void   *data[2];

	len = list_length (node_list);
	max_tofree = MAX (0, len - 500);

	data[0] = &to_free;
	data[1] = &max_tofree;

	/* get rid of the oldest nodes first */
	gt_conn_sort ((CompareFunc)gt_conn_sort_vit_neg);

	node_list = list_foreach_remove (node_list, (ListForeachFunc)collect_old,
	                                 data);

	GT->DBGFN (GT, "trimming %d/%d nodes", list_length (to_free), len);
	list_foreach_remove (to_free, (ListForeachFunc)dump_node, NULL);

	/* set the node list back to rights wrt vitality */
	gt_conn_sort ((CompareFunc)gt_conn_sort_vit);

	/* reset the iterator to some random node */
	iterator = list_nth (node_list,
	                     (float)rand() * len / (RAND_MAX + 1.0));
}

/*****************************************************************************/

int gt_conn_sort_vit (GtNode *a, GtNode *b)
{
	if (a->vitality > b->vitality)
		return -1;
	else if (a->vitality < b->vitality)
		return 1;
	else
		return 0;
}

int gt_conn_sort_vit_neg (GtNode *a, GtNode *b)
{
	return -gt_conn_sort_vit (a, b);
}

/* NOTE: this isnt safe to call at all times */
void gt_conn_sort (CompareFunc func)
{
	node_list = list_sort (node_list, func);

	/* reset the iterator */
	iterator = NULL;
}

/*****************************************************************************/

struct _sync_args
{
	time_t tm;
	FILE  *f;
};

static GtNode *sync_node (TCPC *c, GtNode *node, struct _sync_args *sync)
{
	size_t ret;

	/* node->vitality is updated lazily, to avoid a syscall for every
	 * packet.  Maybe this isnt worth it */
	if (node->state & GT_NODE_CONNECTED)
		node->vitality = sync->tm;

	/* only cache the node if we have connected to it before successfully */
	if (node->vitality > 0 &&
	    node->gt_port > 0)
	{
		ret = fprintf (sync->f, "%lu %s:%hu %lu %lu\n", (long)node->vitality,
		               net_ip_str (node->ip), node->gt_port,
		               (long)node->size_kb, (long)node->files);

		/* stop iterating if there was an error */
		if (ret <= 0)
			return node;
	}

	return NULL;
}

void gt_node_list_save (void)
{
	struct _sync_args sync;
	char  *tmp_path;

	time (&sync.tm);
	tmp_path = STRDUP (gift_conf_path ("Gnutella/nodes.tmp"));

	if (!(sync.f = fopen (gift_conf_path ("Gnutella/nodes.tmp"), "w")))
	{
		GT->DBGFN (GT, "error opening tmp file: %s", GIFT_STRERROR ());
		free (tmp_path);
		return;
	}

	/*
	 * The nodes file is fairly important. Check for errors when writing to
	 * the disk.
	 */
	if (gt_conn_foreach (GT_CONN_FOREACH(sync_node), &sync,
	                     GT_NODE_NONE, GT_NODE_ANY, 0) != NULL)
	{
		GT->warn (GT, "error writing nodes file: %s", GIFT_STRERROR());
		fclose (sync.f);
		free (tmp_path);
		return;
	}

	if (fclose (sync.f) != 0)
	{
		GT->warn (GT, "error closing nodes file: %s", GIFT_STRERROR());
		free (tmp_path);
		return;
	}

	file_mv (tmp_path, gift_conf_path ("Gnutella/nodes"));

	free (tmp_path);
}

static void load_nodes (/*const*/ char *conf_path, gt_node_class_t klass)
{
	GtNode *node;
	FILE   *f;
	char   *buf = NULL;
	char   *ptr;

	f = fopen (gift_conf_path (conf_path), "r");

	/* try the global nodes file */
	if (!f)
	{
		char *filename;

		if (!(filename = malloc (strlen (platform_data_dir ()) + 50)))
			return;

		sprintf (filename, "%s/%s", platform_data_dir (), conf_path);

		f = fopen (filename, "r");

		free (filename);
	}

	if (!f)
		return;

	while (file_read_line (f, &buf))
	{
		unsigned long  vitality;
		in_addr_t      ip;
		in_port_t      port;
		uint32_t       size_kb;
		uint32_t       files;

		ptr = buf;

		/* [vitality] [ip]:[port] [shares size(kB)] [file count] */

		vitality = ATOUL  (string_sep (&ptr, " "));
		ip       = net_ip (string_sep (&ptr, ":"));
		port     = ATOI   (string_sep (&ptr, " "));
		size_kb  = ATOI   (string_sep (&ptr, " "));
		files    = ATOI   (string_sep (&ptr, " "));

		if (!ip || ip == INADDR_NONE)
			continue;

		if (size_kb == (uint32_t)-1)
			size_kb = 0;

		if (files == (uint32_t)-1)
			files = 0;

		node = gt_node_register (ip, port, klass);

		if (!node)
			continue;

		node->vitality = vitality;

		node->size_kb  = size_kb;
		node->files    = files;
	}

	fclose (f);
}

void gt_node_list_load (void)
{
	load_nodes ("Gnutella/nodes", GT_NODE_ULTRA);

	/* sort the list so we contact the most recent nodes first */
	gt_conn_sort ((CompareFunc) gt_conn_sort_vit);
}
