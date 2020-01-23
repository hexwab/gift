/*
 * netorg.c - core OpenFT network organization (yes, this file is important)
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include "ft_openft.h"

#include "ft_netorg.h"

/*****************************************************************************/

/* do I smell a lan party? */
#define OPENFT_LOCAL_MODE \
    config_get_int (OPENFT->conf, "local/lan_mode=0")
#define OPENFT_LOCAL_ALLOW \
    config_get_str (OPENFT->conf, "local/hosts_allow=LOCAL")

/*****************************************************************************/

/* conn_spread is used to improve the efficiency of conn_foreach (and
 * subsequently conn_length) by dividing the lists based on state
 *
 * NOTE: these divided lists are maintained separately from the actual
 * connection list so that they may be conditionally used...as soon as I can
 * verify this optimization works perfectly I will remove the conditional */
#define USE_CONN_SPREAD

#ifdef USE_CONN_SPREAD
static ListLock *spread_final = NULL;  /* connected    */
static List     *spread_final_iter = NULL;
static ListLock *spread_limbo = NULL;  /* connecting   */
static List     *spread_limbo_iter = NULL;
static ListLock *spread_disco = NULL;  /* disconnected */
static List     *spread_disco_iter = NULL;
#endif /* USE_CONN_SPREAD */

/* define when you wish to check the conn_spread result versus the old method
 * in order to verify correctness
 * NOTE: has no effect w/o USE_CONN_SPREAD */
/* #define CHECK_CONN_SPREAD */

#if defined (USE_CONN_SPREAD) && defined (CHECK_CONN_SPREAD)
/* this is set to true to explain to conn_foreach that it should use
 * the main connections list instead of the spread list */
static int spread_checking = FALSE;
#endif /* USE_CONN_SPREAD && CHECK_CONN_SPREAD */

/*****************************************************************************/

/* use both the hash table and list for sortability and fast lookup */
#define USE_CONN_HASH

#ifdef USE_CONN_HASH
static Dataset *conn = NULL;
#endif /* USE_CONN_HASH */

/*****************************************************************************/

static ListLock *connections = NULL;
static List     *iterator    = NULL;   /* current "position" in connections */

/*****************************************************************************/

/* maximum number of connections allowed to be in NODE_CONNECTING state when
 * connecting to nodelist responses */
#define MAX_PENDING_CONNECTIONS \
    config_get_int (OPENFT->conf, "main/pending_connections=20")

#define MAX_USER_CONNECTIONS    10

/* amount of times a node is allowed to disconnect in a 2 minute period */
#define MAX_DISCONNECT_COUNT    3

/*****************************************************************************/

#ifdef USE_CONN_SPREAD
static ListLock **spread_op_list (unsigned short state, List ***r_op_iter)
{
	ListLock **op_list = NULL;
	List     **op_iter = NULL;

	switch (state)
	{
	 case NODE_CONNECTED:
		op_list = &spread_final;
		op_iter = &spread_final_iter;
		break;
	 case NODE_CONNECTING:
		op_list = &spread_limbo;
		op_iter = &spread_limbo_iter;
		break;
	 case NODE_DISCONNECTED:
		op_list = &spread_disco;
		op_iter = &spread_disco_iter;
		break;
	 case (unsigned short) -1:
		op_list = &connections;
		op_iter = &iterator;
		break;
	 default:
		break;
	}

	assert (op_list != NULL);
	assert (op_iter != NULL);

	if (r_op_iter)
		*r_op_iter = op_iter;

	return op_list;
}

static void conn_spread_add (Connection *c,
                             unsigned short state)
{
	ListLock **list;
	List     **iter;

	list = spread_op_list (state, &iter);

	if (!*list)
		*list = list_lock_new ();

	list_lock_prepend (*list, c);

	if (!*iter)
		*iter = (*list)->list;
}

static void conn_spread_remove (Connection *c,
                                unsigned short state)
{
	ListLock **list;
	List     **iter;

	list = spread_op_list (state, &iter);

	if (*iter && (*iter)->data == c)
		*iter = NULL;

	list_lock_remove (*list, c);

	if (!*iter)
		*iter = (*list)->list;
}

static void conn_spread_change (Connection *c,
                                unsigned short orig,
                                unsigned short now)
{
	if (orig == now)
		return;

	/* do not track the spread of connections not being tracked anyway :) */
	if (!conn_lookup (FT_NODE(c)->ip))
		return;

	conn_spread_remove (c, orig);
	conn_spread_add (c, now);
}
#endif /* USE_CONN_SPREAD */

/*****************************************************************************/

void conn_add (Connection *c)
{
	if (!c)
		return;

	/* TODO -- cap the size of this structure ... */

#ifdef USE_CONN_SPREAD
	conn_spread_add (c, FT_NODE(c)->state);
#endif

	/* add the data */
#ifdef USE_CONN_HASH
	if (!conn)
		conn = dataset_new (DATASET_HASH);

	dataset_insert (&conn, &FT_NODE(c)->ip, sizeof (FT_NODE(c)->ip), c, 0);
#endif /* USE_CONN_HASH */

	if (!connections)
		connections = list_lock_new ();

	list_lock_prepend (connections, c);

	if (!iterator)
		iterator = connections->list;
}

void conn_change (Connection *c, unsigned short prev)
{
#ifdef USE_CONN_SPREAD
	conn_spread_change (c, prev, FT_NODE(c)->state);
#endif /* USE_CONN_SPREAD */
}

void conn_remove (Connection *c)
{
	if (!c)
		return;

#ifdef USE_CONN_SPREAD
	conn_spread_remove (c, FT_NODE(c)->state);
#endif

#ifdef USE_CONN_HASH
	dataset_remove (conn, &FT_NODE(c)->ip, sizeof (FT_NODE(c)->ip));
#endif /* USE_CONN_HASH */

	if (iterator && iterator->data == c)
		iterator = NULL;

	list_lock_remove (connections, c);

	if (!iterator)
		iterator = connections->list;
}

/*****************************************************************************/

#ifndef USE_CONN_HASH
static Connection *locate_FT_NODE(Connection *c, Node *node,
								in_addr_t *ip)
{
	if (node->ip == *ip)
		return c;

	return NULL;
}
#endif /* !USE_CONN_HASH */

Connection *conn_lookup (in_addr_t ip)
{
	Connection *c;

#ifdef USE_CONN_HASH

	c = dataset_lookup (conn, &ip, sizeof (ip));

#else /* !USE_CONN_HASH */

	c = conn_foreach ((ConnForeachFunc) locate_node, &ip,
	                  NODE_NONE, -1, 0);

#endif /* USE_CONN_HASH */

	return c;
}

/*****************************************************************************/

static void sort_list (ListLock *lock, List **iterator, CompareFunc func)
{
	if (!lock)
		return;

	lock->list = list_sort (lock->list, func);
	*iterator  = lock->list;
}

void conn_sort (CompareFunc func)
{
	sort_list (connections, &iterator, func);

#ifdef USE_CONN_SPREAD
	sort_list (spread_final, &spread_final_iter, func);
	sort_list (spread_limbo, &spread_limbo_iter, func);
	sort_list (spread_disco, &spread_disco_iter, func);
#endif /* USE_CONN_SPREAD */
}

/*****************************************************************************/

static Connection *node_reconnect (Connection *c, Node *node, int *processed)
{
	if (!FT_NODE(c)->port)
		return NULL;

	/* do not connect to a node you know isn't the right version */
	if (node->version && !FT_VERSION_EQ(node->version, FT_VERSION_LOCAL))
		return NULL;

	/* only count processed if conn_auth succeeded...skip "bad" nodes :) */
	if (ft_session_connect (c) < 0 && processed)
		(*processed)++;

	return NULL;
}

static Connection *node_disconnect_idle (Connection *c, Node *node,
                                         int *processed)
{
	if (node->class != NODE_USER)
		return NULL;

	if (node->sent_list && node->recv_list)
	{
		/* don't let this node come back for a while */
		FT_NODE(c)->disconnect_cnt = 60;

		ft_session_stop (c);

		if (processed)
			(*processed)++;
	}

	return NULL;
}

/* core logic used to determine how frequent to make new connections */
void conn_maintain ()
{
	/* disconnect idle user <-> user connections */
	if (FT_SELF->class == NODE_USER)
	{
		conn_foreach ((ConnForeachFunc) node_disconnect_idle, NULL,
		              NODE_USER, NODE_CONNECTED, 0);
	}

	/* constantly retry a small amount of user nodes to robust the list */
	conn_foreach ((ConnForeachFunc) node_reconnect, NULL,
	              NODE_NONE, NODE_DISCONNECTED, 1);

	/* we aren't satisfied */
	if (conn_length (NODE_SEARCH, NODE_CONNECTED) < 10)
	{
		int processed = 0;

		conn_foreach ((ConnForeachFunc) node_reconnect, &processed,
		              NODE_SEARCH, NODE_DISCONNECTED, 5);

		/* we still aren't satisfied */
		if (processed < 3)
		{
			/* try 10 more users */
			conn_foreach ((ConnForeachFunc) node_reconnect, NULL,
			              NODE_NONE, NODE_DISCONNECTED, 10);
		}
	}

	/* if we're desperate, retry a lot of nodes */
	if (conn_length (NODE_USER, NODE_CONNECTED) < 5)
	{
		conn_foreach ((ConnForeachFunc) node_reconnect, NULL,
					  NODE_NONE, NODE_DISCONNECTED, 30);
	}
}

/*****************************************************************************/

#if 0
/* i deserve to be shot for this */
static int foreach_oplist (unsigned short state, ListLock ***list, List ***iter)
{
#ifdef USE_CONN_SPREAD
	*list = spread_op_list (state, iter);
# ifdef CHECK_CONN_SPREAD
	if (spread_checking)
		*list = spread_op_list (-1, iter);
# endif /* CHECK_CONN_SPREAD */
#else /* !USE_CONN_SPREAD */
	*list = &connections;
	*iter = &iterator;
#endif /* USE_CONN_SPREAD */

	return TRUE;
}

static unsigned int conn_iter (unsigned short klass, unsigned short state,
                               unsigned int iter, ConnForeach Func, void *udata)
{
	List *ptr;

	/* if iter is non-zero, we were asked to iterate a set number of nodes
	 * and we should therefore begin from the place we last left off.
	 * otherwise, we start from the very beginning of the current list */
	if (iter)
		ptr = *op_iter;
	else
		ptr = (*op_list) ? (*op_list)->list : NULL;
}

/*
 * Connection iterator.  Loop through all connections and/or cached nodes
 * entries and perform arbitrary operations on the requested set.  This
 * system keeps its last known "place" in the list(s) so that each
 * subsequent call picks up where you left off, so that we may gaurantee
 * no nodes in the list are "forgotten".
 */
unsigned int conn_foreach (unsigned short klass, unsigned short state,
                           unsigned int iter, ConnForeach func, void *udata)
{
	ListLock   **list;
	List       **iter;
	unsigned int ret;

	if (!func)
		return 0;

	if (!foreach_oplist (state, &op_list, &op_iter))
		return 0;

	/* ensure that `func' makes no direct write access to this list while
	 * we're iterating, otherwise Very Bad Things (TM) will happen */
	list_lock (*op_list);

	/* actually perform the iteration now */
	ret = conn_iter (klass, state, iter, func, udata);

	/* check to see if the current iterator position is being queued for
	 * removal by the locked list, and if so, reset the iterator to the
	 * very beginning of the list.  TODO -- there is obviously a better way
	 * to do this...im just lazy */
	if (*op_iter)
	{
		if (*op_list && list_find ((*op_list)->lock_remove, (*op_iter)->data))
			*op_iter = NULL;
	}

	/* merge all the writes back into the list */
	list_unlock (*op_list);

	return ret;
}
#endif

/* connection iterator
 * TODO -- clean this mess up */
Connection *conn_foreach (ConnForeachFunc func, void *user_data,
                          unsigned short klass, unsigned short state,
                          unsigned short iter)
{
	Connection *ret = NULL;
	Connection *c;
	Node       *node;
	List       *ptr;
	ListLock  **op_list;
	List      **op_iter;
	int         looped = FALSE;

	assert (func != NULL);

#ifdef USE_CONN_SPREAD
	op_list = spread_op_list (state, &op_iter);

# ifdef CHECK_CONN_SPREAD
	if (spread_checking)
		op_list = spread_op_list (-1, &op_iter);
# endif /* CHECK_CONN_SPREAD */
#else /* !USE_CONN_SPREAD */
	op_list = &connections;
	op_iter = &iterator;
#endif /* USE_CONN_SPREAD */

	/* if iteration was supplied, use it, otherwise start from the
	 * beginning */
	if (iter)
		ptr = *op_iter;
	else
		ptr = (*op_list) ? (*op_list)->list : NULL;

	list_lock (*op_list);

	/* this is cute */
	for (;; ptr = list_next (ptr))
	{
		/* if iter, start over from the beginning of the list */
		if (iter && !ptr && !looped)
		{
			ptr = (*op_list) ? (*op_list)->list : NULL;
			looped = TRUE;
		}

		if (!ptr)
			break;

		c = ptr->data;
		node = FT_NODE(c);

		assert (node);

		/* filter out the conditions supplied */
		if (klass && !(node->class & klass))
			continue;

		if (state != (unsigned short) -1 && node->state != state)
			continue;

		/* callback */
		ret = (*func) (c, node, user_data);

		/* iter may have been passed as 0, if that is the case, ignore its
		 * behavior completely */
		if (iter)
		{
			/* move the iterator along itself...we can't trust ptr as it is
			 * dupped/locked */
			if (*op_iter)
				*op_iter = (*op_iter)->next;

			iter--;

			if (!iter)
				break;
		}

		if (ret)
			break;
	}

	/* check to see if we need to move op_iter */
	if (*op_iter)
	{
		if (*op_list && list_find ((*op_list)->lock_remove, (*op_iter)->data))
			*op_iter = NULL;
	}

	list_unlock (*op_list);

	/* move op_iter back to something sane */
	if (!*op_iter)
		*op_iter = (*op_list) ? (*op_list)->list : NULL;

	return ret;
}

/*****************************************************************************/

static int conn_clear_entry (Connection *c, void *udata)
{
	node_free (c);
	return TRUE;
}

void conn_clear (ConnForeachFunc func)
{
	conn_foreach (func, NULL, NODE_NONE, -1, 0);

	if (connections)
	{
		connections->list =
		    list_foreach_remove (connections->list,
		                         (ListForeachFunc) conn_clear_entry, NULL);

		list_lock_free (connections);
	}

#ifdef USE_CONN_SPREAD
	list_lock_free (spread_final);
	list_lock_free (spread_limbo);
	list_lock_free (spread_disco);
#endif /* USE_CONN_SPREAD */

#ifdef USE_CONN_HASH
	dataset_clear (conn);
#endif /* USE_CONN_HASH */
}

/*****************************************************************************/

static Connection *conn_counter (Connection *c, Node *node, int *length)
{
	(*length)++;
	return NULL;
}

int conn_length (unsigned short klass, unsigned short state)
{
	int ret = 0;
#if defined (USE_CONN_SPREAD) && defined (CHECK_CONN_SPREAD)
	int vfy = 0;
#endif

	conn_foreach ((ConnForeachFunc) conn_counter,
				  &ret, klass, state, 0);

#if defined (USE_CONN_SPREAD) && defined (CHECK_CONN_SPREAD)
	spread_checking = TRUE;

	conn_foreach ((ConnForeachFunc) conn_counter,
				  &vfy, klass, state, 0);

	spread_checking = FALSE;

	assert (ret == vfy);
#endif

	return ret;
}

/*****************************************************************************/

/* determine whether or not a connection should be established */
int conn_auth (Connection *c, int outgoing)
{
	int threshold;

	assert (c != NULL);

	/* i wont connect to myself */
	if (!FT_NODE(c)->ip)
		return FALSE;

	/* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (OPENFT_LOCAL_MODE)
	{
		if (!net_match_host (FT_NODE(c)->ip, OPENFT_LOCAL_ALLOW))
			return FALSE;
	}

	/* this connection is firewalled, and therefore unavailable for outgoing
	 * connection */
	if (outgoing && FT_NODE(c)->port == 0)
		return FALSE;

	/* if this node is flooding us, ignore it */
	if (FT_NODE(c)->disconnect_cnt > MAX_DISCONNECT_COUNT)
		return FALSE;

	/* mmmm, powerful nodes */
	if (FT_NODE(c)->class & (NODE_SEARCH | NODE_INDEX))
		return TRUE;

	if (FT_SELF->class == NODE_USER)
	{
		if (conn_length (NODE_USER, NODE_CONNECTED) > MAX_USER_CONNECTIONS)
			return FALSE;
	}

	threshold = conn_length (NODE_NONE, NODE_CONNECTING);

	if (threshold < MAX_PENDING_CONNECTIONS)
		return TRUE;

	return FALSE;
}
