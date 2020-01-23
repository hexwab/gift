/*
 * $Id: gt_node_cache.c,v 1.11 2004/03/05 17:58:39 hipnod Exp $
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
#include "gt_node_cache.h"

#include "file_cache.h"

/*****************************************************************************/

#define MAX_RECENT          (150)
#define MAX_STABLE          (30)

#define MAX_STICKY_RECENT   (150)
#define MAX_STICKY_STABLE   (30)

/*****************************************************************************/

static List       *recent;
static List       *stable;

static List       *sticky_recent;   /* synced to disk */
static List       *sticky_stable;   /* like stable list, but nodes stay */

#if 0
static List       *compressible;
#endif

/*****************************************************************************/

static void ipv4_addr_init (struct ipv4_addr *addr, in_addr_t ip,
                            in_port_t port)
{
	memset (addr, 0, sizeof (struct ipv4_addr));

	addr->ip   = ip;
	addr->port = port;
}

static void cached_node_init (struct cached_node *node, in_addr_t ipv4,
                              in_port_t port, gt_node_class_t klass,
                              time_t timestamp, time_t uptime,
                              in_addr_t src_ip)
{
	memset (node, 0, sizeof (*node));

	ipv4_addr_init (&node->addr, ipv4, port);

	node->klass     = klass;
	node->timestamp = timestamp;
	node->uptime    = uptime;
	node->src_ip    = src_ip;
}

static int cmp_ipv4 (struct cached_node *a, struct cached_node *b)
{
	return memcmp (&a->addr, &b->addr, sizeof (a->addr));
}

static int cmp_recent (struct cached_node *a, struct cached_node *b)
{
	return INTCMP (b->timestamp, a->timestamp);
}

static int cmp_stable (struct cached_node *a, struct cached_node *b)
{
	time_t a_time, b_time;

	/*
	 * Assume the node will be up for as much time as it was up
	 * already, and convert this to a timestamp.
	 *
	 * This makes a difference than just comparing the uptime
	 * because the cache gets synced to disk.
	 */
	a_time = a->uptime * 2 + a->timestamp;
	b_time = b->uptime * 2 + b->timestamp;

	return INTCMP (b_time, a_time);
}

static List *add_list (List *list, size_t max_elements, CompareFunc func,
                       struct cached_node *node)
{
	struct cached_node  *new_node;
	struct cached_node  *rm;
	List                *dup;
	List                *link;

	if ((dup = list_find_custom (list, node, (CompareFunc)cmp_ipv4)))
	{
		free (dup->data);
		list = list_remove_link (list, dup);
	}

	if (!(new_node = gift_memdup (node, sizeof (struct cached_node))))
		return list;

	list = list_insert_sorted (list, func, new_node);

	/*
	 * Truncate list at max_elements.
	 */
	link = list_nth (list, max_elements);
	rm   = list_nth_data (link, 0);

	list = list_remove_link (list, link);
	free (rm);

	return list;
}

static void add_to_cache (struct cached_node *node)
{
	recent        = add_list (recent, MAX_RECENT,
	                          (CompareFunc)cmp_recent, node);
	sticky_recent = add_list (sticky_recent, MAX_STICKY_RECENT,
	                          (CompareFunc)cmp_recent, node);

	if (node->uptime > 0)
	{
		stable        = add_list (stable, MAX_STABLE,
		                          (CompareFunc)cmp_stable, node);
		sticky_stable = add_list (sticky_stable, MAX_STICKY_STABLE,
		                          (CompareFunc)cmp_stable, node);
	}
}

void gt_node_cache_add_ipv4 (in_addr_t ipv4, in_port_t port,
                             gt_node_class_t klass, time_t timestamp,
                             time_t uptime, in_addr_t src_ip)
{
	struct cached_node node;

	/* make sure we don't add nodes with class GT_NODE_NONE */
	if (klass == GT_NODE_NONE)
		klass = GT_NODE_LEAF;

	cached_node_init (&node, ipv4, port, klass, timestamp, uptime, src_ip);
	add_to_cache (&node);

	/* don't put nodes that are already in the node list in the cache */
	if (gt_node_lookup (ipv4, port))
		gt_node_cache_del_ipv4 (ipv4, port);
}

static List *del_list (List *list, struct cached_node *node,
                       in_addr_t ipv4, in_port_t port)
{
	List *link;

	if (!(link = list_find_custom (list, node, (CompareFunc)cmp_ipv4)))
		return list;

	free (link->data);
	return list_remove_link (list, link);
}

static void del_from_cache (in_addr_t ipv4, in_port_t port)
{
	struct cached_node node;

	cached_node_init (&node, ipv4, port, GT_NODE_NONE, 0, 0, 0);

	recent = del_list (recent, &node, ipv4, port);
	stable = del_list (stable, &node, ipv4, port);
}

void gt_node_cache_del_ipv4 (in_addr_t ipv4, in_port_t port)
{
	del_from_cache (ipv4, port);
}

/*****************************************************************************/

#if 0
static int print_node (struct cached_node *node, String *s)
{
	char *src;

	src = STRDUP (net_ip_str (node->src_ip));

	string_appendf (s, "[%s:%hu {%s}] ", net_ip_str (node->addr.ip),
	                node->addr.port, src);
	free (src);

	return FALSE;
}

static void print_list (char *prefix, List *list)
{
	String *s;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
	      return;

	string_append (s, prefix);

	list_foreach (list, (ListForeachFunc)print_node, s);
	GT->dbg (GT, "%s", s->str);

	string_free (s);
}
#endif

void gt_node_cache_trace (void)
{
#if 0
	print_list ("recent: ", recent);
	print_list ("stable: ", stable);
	print_list ("sticky_recent: ", sticky_recent);
	print_list ("sticky_stable: ", sticky_stable);
#endif
}

/*****************************************************************************/

/* return some nodes from the cache, and remove them */
size_t get_first (List **src_list, List **dst_list, size_t nr)
{
	struct cached_node *node;
	struct cached_node *dup;

	node = list_nth_data (*src_list, 0);

	if (!node || !(dup = gift_memdup (node, sizeof (*node))))
		return nr;

	*dst_list = list_prepend (*dst_list, dup);
	nr--;

	gt_node_cache_del_ipv4 (node->addr.ip, node->addr.port);
	return nr;
}

/*
 * Remove some elements from the cache and return them in a list .
 *
 * The nodes and the list returned SHOULD be freed.
 */
List *gt_node_cache_get_remove (size_t nr)
{
	List *list = NULL;

	/*
	 * We check the recent list first, and then if that's empty check the
	 * stable list, so we don't end up checking the stable list all the time.
	 */
	while (nr > 0 && recent != NULL)
		nr = get_first (&recent, &list, nr);

	while (nr > 0 && stable != NULL)
		nr = get_first (&stable, &list, nr);

	return list;
}

/*
 * Return some elements from the cache in a list
 *
 * NOTE: the data in the list returned SHOULD NOT be freed
 */
List *gt_node_cache_get (size_t nr)
{
	List    *list = NULL;
	size_t   len;
	int      index;

	len = list_length (sticky_recent);

	/*
	 * If we don't have more than twice the number of nodes, just return an
	 * offset in the list of recent nodes.
	 *
	 * This is done so we can use the simple (stupid) selection algorithm
	 * below that would be inefficient otherwise.
	 */
	if (len / 2 < nr)
		return list_copy (list_nth (sticky_recent, MAX (0, len - nr)));

	while (nr > 0)
	{
		struct cached_node *node;

		index = (float)len * rand() / (RAND_MAX + 1.0);

		node = list_nth_data (sticky_recent, index);
		assert (node != NULL);

		if (list_find (list, node))
			continue;

		list = list_append (list, node);
		nr--;
	}

	return list;
}

/*****************************************************************************/

static char *node_cache_file (const char *name)
{
	return gift_conf_path ("Gnutella/%s", name);
}

/*
 * Store the cache data in the dataset. The ip:port as a string
 * is used as a key.
 */
static BOOL write_line (struct cached_node *node, FileCache *cache)
{
	char *ip_port;
	char *line;

	ip_port = stringf_dup ("%s:%hu", net_ip_str (node->addr.ip),
	                       node->addr.port);

	if (!ip_port)
		return FALSE;

	line = stringf_dup ("%s %lu %lu %s", gt_node_class_str (node->klass),
	                    (long)node->timestamp, (long)node->uptime,
	                    net_ip_str (node->src_ip));

	if (!line)
	{
		free (ip_port);
		return FALSE;
	}

	file_cache_insert (cache, ip_port, line);

	free (ip_port);
	free (line);

	return FALSE;
}

/*
 * Read in a line from a cache file.
 *
 * The ip:port is the key in the Dataset. The value
 * is everything else that dsecribe an entry in the node
 * cache, as a string.
 */
static void parse_line (ds_data_t *key, ds_data_t *value, void *udata)
{
	char      *ip_port = key->data;
	char      *str     = value->data;
	in_addr_t  ipv4;
	in_port_t  port;
	time_t     timestamp;
	time_t     uptime;
	in_addr_t  src_ip;
	char      *klass;

	ipv4 = net_ip (string_sep (&ip_port, ":"));
	port = ATOUL  (ip_port);

	if (ipv4 == 0 || ipv4 == INADDR_NONE || port == 0)
		return;

	/* NOTE: we ignore the class string for now */
	klass     =         string_sep (&str, " ");
	timestamp = ATOUL  (string_sep (&str, " "));
	uptime    = ATOUL  (string_sep (&str, " "));
	src_ip    = net_ip (string_sep (&str, " "));

	if (!klass || timestamp == 0)
		return;

	/* add it to the cache */
	gt_node_cache_add_ipv4 (ipv4, port, GT_NODE_ULTRA, timestamp, uptime,
	                        src_ip);
}

static BOOL load_cache (char *name)
{
	FileCache  *cache;
	char       *file;

	file  = node_cache_file (name);
	cache = file_cache_new (file);

	if (!cache)
		return FALSE;

	dataset_foreach (cache->d, DS_FOREACH(parse_line), NULL);

	file_cache_free (cache);
	return TRUE;
}

static BOOL save_cache (char *name, List *list)
{
	FileCache  *cache;
	char       *file;

	file  = node_cache_file (name);
	cache = file_cache_new (file);

	/* flush the existing data (in memory, only) */
	file_cache_flush (cache);

	/* save each entry in the node cache to the file cache */
	list_foreach (list, (ListForeachFunc)write_line, cache);

	if (!file_cache_sync (cache))
	{
		GT->DBGFN (GT, "error saving cache \"%s\": %s", name, GIFT_STRERROR());
		return FALSE;
	}

	file_cache_free (cache);

	return TRUE;
}

void gt_node_cache_load (void)
{
	load_cache ("stable_nodes");
	load_cache ("recent_nodes");
}

void gt_node_cache_save (void)
{
	save_cache ("stable_nodes", sticky_stable);
	save_cache ("recent_nodes", sticky_recent);
}

/*****************************************************************************/

void gt_node_cache_init (void)
{
	gt_node_cache_load ();
}

void gt_node_cache_cleanup (void)
{
	gt_node_cache_save ();

	/* TODO: free node cache lists */
}
