/*
 * $Id: as_node_man.c,v 1.16 2005/09/15 21:13:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* Log node updates. */
/* #define NODE_DEBUG */

/*****************************************************************************/

/* Used to determine if a node should be removed. */
static as_bool node_useless (ASNode *node)
{
	/* Do nothing for now. Nodes will be clipped when saving to nodes file. */
	return FALSE;
}

/* Calculate weight for a node. Used for sorting. */
static float node_weight (ASNodeMan *man, ASNode *node)
{
	double weight;

	/* I am just making this up... */
	weight = + node->connects * 50.0
	         + node->reports * 10.0
	         + ((node->connects + 1) / (node->attempts + 1)) * 200.0
	         + ((node->last_seen - node->first_seen) / (24*EHOURS)) * 10.0
	         + ((node->last_seen - man->oldest_last_seen) / (10*EMINUTES)) * 1.0;

	return (float) weight;
}

/* Used to sort list for connecting. Nodes to connect to will be taken from
 * the front.
 */
static int node_connect_cmp (ASNode *a, ASNode *b)
{
	/* Put currently used nodes at the back */
	if (a->in_use != b->in_use)
		return a->in_use ? 1 : -1;

	/* Compare rest by weight */
	return (a->weight == b->weight) ? 0 : ((a->weight < b->weight) ? 1 : -1);
}

/* Used to sort list for saving in node file. The first AS_MAX_NODEFILE_SIZE
 * nodes will be saved.
 */
static int node_save_cmp (ASNode *a, ASNode *b)
{
	/* Compare by weight. Use doesn't matter for saving */
	return (a->weight == b->weight) ? 0 : ((a->weight < b->weight) ? 1 : -1);
}

/*****************************************************************************/

/* Create new node manager. */
ASNodeMan *as_nodeman_create ()
{
	ASNodeMan *man;

	if (!(man = malloc (sizeof (ASNodeMan))))
		return NULL;

	man->nodes = NULL;

	if (!(man->index = as_hashtable_create_int ()))
	{
		free (man);
		return NULL;
	}

	man->oldest_first_seen = time (NULL);
	man->oldest_last_seen = time (NULL);

	return man;
}

/* Free node manager. */
void as_nodeman_free (ASNodeMan *man)
{
	if (!man)
		return;

	as_nodeman_empty (man);

	/* free hashtable */
	as_hashtable_free (man->index, FALSE);

	free (man);
}

static int node_free_itr (ASNode *node, int *used)
{
	if (node->in_use)
		(*used)++;
	as_node_free (node);
	return TRUE;
}

/* Free all nodes. */
void as_nodeman_empty (ASNodeMan *man)
{
	int used = 0;

	/* remove index */
	as_hashtable_free (man->index, FALSE);

	/* recreate emtpy index */
	man->index = as_hashtable_create_int ();
	assert (man->index); /* hmm */

	/* free nodes */
	man->nodes = list_foreach_remove (man->nodes,
	                                  (ListForeachFunc)node_free_itr, &used);

	man->oldest_first_seen = time (NULL);
	man->oldest_last_seen = time (NULL);

	if (used > 0)
		AS_WARN_1 ("%d nodes still in use when emptying node cache", used);
}

/* Print all nodes to log. */
void as_nodeman_dump (ASNodeMan *man)
{
	List *link;
	ASNode *node;

	AS_DBG ("Dumping node cache:");
	for (link = man->nodes; link; link = link->next)
	{
		node = (ASNode *)link->data;
		AS_DBG_5 ("%s:%d, reports: %u, attempts: %u, connects: %u",
		          net_ip_str (node->host), node->port,
		          node->reports, node->attempts, node->connects);
	}
}

/*****************************************************************************/

/* Get next best node for connecting. Sets node->last_attempt to now and
 * increments node->attempts. Reinserts node at the correct position so the
 * next call will not return the same node. Sets node->in_use to TRUE and
 * won't return nodes which are in use. If there are no unused, eligible nodes
 * NULL is returned.
 */
ASNode *as_nodeman_next (ASNodeMan *man)
{
	List *link;
	ASNode *node;
	time_t now = time (NULL);

	/* Find first unused node which hasn't been tried recently. */
	for (link = man->nodes; link; link = link->next)
	{
		node = (ASNode *)link->data;

		if (!node->in_use && (now - node->last_attempt) > 10*EMINUTES)
			break;
	}

	if(!link)
		return NULL;

	/* Remove this link from current position */
	man->nodes = list_unlink_link (man->nodes, link);

	/* Update data */
	node = (ASNode *)link->data;

	node->last_attempt = time (NULL);
	node->attempts++;
	node->in_use = TRUE;
	
	/* Reinsert at new correct position */
	man->nodes = list_insert_link_sorted (man->nodes,
	                                      (CompareFunc)node_connect_cmp, link);

	return node;
}

/* Update node with specified ip when connection succeeded. Updates
 * node->last_seen and node->connects. Returns FALSE if node is not in list.
 */
as_bool as_nodeman_update_connected (ASNodeMan *man, in_addr_t host)
{
	List *link;
	ASNode *node;

	/* Find node */
	if (!(link = as_hashtable_lookup_int (man->index, (as_uint32)host)))
	{
		AS_ERR ("Tried to update nonexistent node.");
		return FALSE;
	}

	/* Unlink from current position */
	man->nodes = list_unlink_link (man->nodes, link);

	/* Update node */
	node = (ASNode *)link->data;
	node->last_seen = time (NULL);
	node->connects++;
	/* Recalculate node weight */
	node->weight = node_weight (man, node);

	/* Reinsert at correct position */
	man->nodes = list_insert_link_sorted (man->nodes,
	                                      (CompareFunc)node_connect_cmp, link);

#if NODE_DEBUG
	AS_HEAVY_DBG_5 ("Node connected: %s:%d, connects: %d, attempts: %d, weigth: %.02f",
	                net_ip_str (node->host), node->port, node->connects,
	                node->attempts, node->weight);
#endif

	return TRUE;
}

/* Update node with specified ip when connection failed. May remove node from
 * internal list if it is deemed useless. Returns FALSE if node is not in
 * list. Sets node->in_use to FALSE.
 */
as_bool as_nodeman_update_failed (ASNodeMan *man, in_addr_t host)
{
	List *link;
	ASNode *node;

	/* Find node */
	if (!(link = as_hashtable_lookup_int (man->index, (as_uint32)host)))
	{
		AS_ERR ("Tried to update nonexistent node.");
		return FALSE;
	}

	/* Unlink from current position */
	man->nodes = list_unlink_link (man->nodes, link);

	/* Update node */
	node = (ASNode *)link->data;
	node->in_use = FALSE;
	/* Recalculate node weight */
	node->weight = node_weight (man, node);

#if NODE_DEBUG
	AS_HEAVY_DBG_5 ("Node failed: %s:%d, connects: %d, attempts: %d, weigth: %.02f",
	                net_ip_str (node->host), node->port, node->connects,
	                node->attempts, node->weight);
#endif

	/* Remove node if has become useless */
	if (node_useless (node))
	{
		/* free link */
		assert (link->prev == NULL && link->next == NULL);
		list_free (link);
		/* remove from hash table */
		as_hashtable_remove_int (man->index, (as_uint32)node->host);
		/* free data */
		as_node_free (node);

		return TRUE;
	}

	/* Reinsert at correct position */
	man->nodes = list_insert_link_sorted (man->nodes,
	                                      (CompareFunc)node_connect_cmp, link);

	return TRUE;
}

/* Update node with specified ip on disconnect. Updates node->last_seen. May
 * remove node if it is deemed useless. Returns FALSE if node is not in list.
 * Sets node->in_use to FALSE.
 */
as_bool as_nodeman_update_disconnected (ASNodeMan *man, in_addr_t host)
{
	List *link;
	ASNode *node;

	/* Find node */
	if (!(link = as_hashtable_lookup_int (man->index, (as_uint32)host)))
	{
		AS_ERR ("Tried to update nonexistent node.");
		return FALSE;
	}

	/* Unlink from current position */
	man->nodes = list_unlink_link (man->nodes, link);

	/* Update node */
	node = (ASNode *)link->data;
	node->last_seen = time (NULL);
	node->in_use = FALSE;
	/* Recalculate node weight */
	node->weight = node_weight (man, node);

#if NODE_DEBUG
	AS_HEAVY_DBG_5 ("Node disconnected: %s:%d, connects: %d, attempts: %d, weigth: %.02f",
	                net_ip_str (node->host), node->port, node->connects,
	                node->attempts, node->weight);
#endif

	/* Remove node if has become useless */
	if (node_useless (node))
	{
		/* free link */
		assert (link->prev == NULL && link->next == NULL);
		list_free (link);
		/* remove from hash table */
		as_hashtable_remove_int (man->index, (as_uint32)node->host);
		/* free data */
		as_node_free (node);

		return TRUE;
	}

	/* Reinsert at correct position */
	man->nodes = list_insert_link_sorted (man->nodes,
	                                      (CompareFunc)node_connect_cmp, link);

	return TRUE;
}

/* Update node with specified ip when it is reported by other supernodes. If 
 * node is not in list it is added. Updates node->last_seen, node->reports and
 * node->port.
 */
void as_nodeman_update_reported (ASNodeMan *man, in_addr_t host,
                                 in_port_t port)
{
	List *link;
	ASNode *node;

	/* Find node if it exists */
	if ((link = as_hashtable_lookup_int (man->index, (as_uint32)host)))
	{
		/* Unlink from current position */
		man->nodes = list_unlink_link (man->nodes, link);	
		node = (ASNode *)link->data;
	}
	else
	{
		/* Create new node */
		if (!(node = as_node_create (host, port)))
		{
			AS_ERR ("Insufficient memory.");
			return;
		}

		node->last_seen = node->first_seen = time (NULL);

		/* Create new link */
		link = list_prepend (NULL, node);

		/* Insert link into hash table */
		if (!as_hashtable_insert_int (man->index, (as_uint32)node->host, link))
		{
			/* eep */
			AS_ERR ("Hash table insert failed while registering node.");
			list_free (link);
			as_node_free (node);

			assert (0);
			return;
		}
	}

	/* Update node */
	node->last_seen = time (NULL);
	node->reports++;
	node->port = port;
	/* Recalculate node weight */
	node->weight = node_weight (man, node);

	/* (Re)insert at correct position */
	man->nodes = list_insert_link_sorted (man->nodes,
	                                      (CompareFunc)node_connect_cmp, link);

#if NODE_DEBUG
	AS_HEAVY_DBG_4 ("Node reported: %s:%d, reports: %d, weigth: %.02f",
	                net_ip_str (node->host), node->port, node->reports,
	                node->weight);
#endif

	return;
}

/*****************************************************************************/

/* Load nodes from file. */
as_bool as_nodeman_load (ASNodeMan *man, const char *file)
{
	FILE *fp;
	List *link;
	ASNode *node;
	time_t now = time (NULL);
	int loaded = 0, ret, port;
	unsigned int reports, attempts, connects;
	unsigned int first_seen, last_seen, last_attempt;
	char ip_str[32];
	in_addr_t host;
	char buf[1024];

	if (!(fp = fopen (file, "r")))
		return FALSE;

	while (fgets (buf, sizeof (buf), fp))
	{
		if (strlen (buf) >= sizeof (buf) - 1)
		{
			AS_ERR ("Aborting node file read. Line too long.");
			break;
		}

		ret = sscanf (buf, "%31s %u %d %d %d %u %u %u\n",
		              ip_str, &port,
		              &reports, &attempts, &connects,
		              &first_seen, &last_seen, &last_attempt);

		if (ret != 8)
			continue;

		host = net_ip (ip_str);

		if (host == 0 || host == INADDR_NONE)
			continue;

#if 0
		AS_HEAVY_DBG_5 ("Loaded node: %s %u %d %d %d",
		                net_ip_str (host), port, reports, attempts, connects);
#endif

		if (!(node = as_node_create (host, (in_port_t)port)))
			continue;

		node->reports      = reports;
		node->attempts     = attempts;
		node->connects     = connects;
		node->first_seen   = (time_t) first_seen;
		node->last_seen    = (time_t) last_seen;
		node->last_attempt = (time_t) last_attempt;

		/* Fake missing data so weighting works correctly */
		if (node->first_seen == 0 || node->last_seen == 0)
			node->first_seen = node->last_seen = now - 72 * EHOURS;

		/* maintain earliest first seen for weighting */
		if (node->first_seen < man->oldest_first_seen)
			man->oldest_first_seen = node->first_seen;

		/* maintain earliest last seen for weighting */
		if (node->last_seen < man->oldest_last_seen)
			man->oldest_last_seen = node->last_seen;

		/* Calculate node weight */
		node->weight = node_weight (man, node);
		
		/* Add node to list and index */
		man->nodes = list_insert_sorted (man->nodes,
		                                 (CompareFunc)node_connect_cmp, node);

		/* FIXME: we are traversing the list a second time! */
		link = list_find (man->nodes, node);
		assert (link);

		if (!as_hashtable_insert_int (man->index, (as_uint32)node->host, link))
		{
			/* eep */
			AS_ERR ("Hash table insert failed while loading nodes.");
			list_remove_link (link, link); /* free link */
			as_node_free (node);

			assert (0);
			continue;
		}

		loaded++;
	}

	fclose (fp);

	AS_DBG_1 ("Loaded %d nodes from node file", loaded);

	return TRUE;
}

/* Save nodes to file */
as_bool as_nodeman_save (ASNodeMan *man, const char *file)
{
	FILE *fp;
	List *list, *link;
	ASNode *node;
	int saved;

	if (!(fp = fopen (file, "w")))
		return FALSE;

	fprintf (fp, "<ip> <port> <reports> <attempts> <connects> <first_seen> "
	         "<last_seen> <last_attempt>\n");

	/* Make a copy of the nodes list and sort it differently for saving. */
	list = list_copy (man->nodes);
	list = list_sort (list, (CompareFunc) node_save_cmp);

	/* Save first AS_MAX_NODEFILE_SIZE nodes */
	saved = 0;
	link = list;

	while (link && saved < AS_MAX_NODEFILE_SIZE)
	{
		node = (ASNode *) link->data;

		fprintf (fp, "%s %u %d %d %d %u %u %u\n",
		         net_ip_str (node->host), (unsigned int)node->port,
		         node->reports, node->attempts, node->connects,
		         (unsigned int)node->first_seen, (unsigned int)node->last_seen,
		         (unsigned int)node->last_attempt);

		saved++;
		link = link->next;
	}

	/* Free list but keep data since it is still referenced by man->nodes. */
	list_free (list);

	fclose (fp);

	AS_DBG_1 ("Saved %d nodes in node file", saved);

	return TRUE;
}

/*****************************************************************************/
