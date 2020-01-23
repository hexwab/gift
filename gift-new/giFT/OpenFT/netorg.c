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

#include "openft.h"

#include "netorg.h"

/*****************************************************************************/

extern Config     *openft_conf;
extern Connection *ft_self;

/*****************************************************************************/

/* do I smell a lan party? */
#define OPENFT_LOCAL_MODE \
    config_get_int (openft_conf, "local/lan_mode=0")
#define OPENFT_LOCAL_ALLOW \
    config_get_str (openft_conf, "local/hosts_allow=LOCAL")

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
static HashTable *conn = NULL;

struct _netorg_cursor
{
	int       item; /* item index in the table */
	HashNode *node; /* node pointer */
};

/* emulate a pointer interface ;) */
static struct _netorg_cursor  curs;
static struct _netorg_cursor *cursor = NULL;
#endif /* USE_CONN_HASH */

/*****************************************************************************/

static ListLock *connections = NULL;
static List     *iterator    = NULL;   /* current "position" in connections */

/*****************************************************************************/

/* maximum number of connections allowed to be in NODE_CONNECTING state when
 * connecting to nodelist responses */
#define MAX_PENDING_CONNECTIONS \
    config_get_int (openft_conf, "main/pending_connections=20")

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
	if (!conn_lookup (NODE (c)->ip))
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
	conn_spread_add (c, NODE (c)->state);
#endif

	/* add the data */
#ifdef USE_CONN_HASH
	if (!conn)
		conn = hash_table_new ();

	hash_table_insert (conn, NODE (c)->ip, c);

	if (!cursor)
	{
		cursor = &curs;

		cursor->item = 0;
		cursor->node = conn->nodes[cursor->item];
	}
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
	conn_spread_change (c, prev, NODE (c)->state);
#endif /* USE_CONN_SPREAD */
}

void conn_remove (Connection *c)
{
	if (!c)
		return;

#ifdef USE_CONN_SPREAD
	conn_spread_remove (c, NODE (c)->state);
#endif

#ifdef USE_CONN_HASH
	if (cursor && cursor->node && cursor->node->value == c)
		cursor = NULL;

	hash_table_remove (conn, NODE (c)->ip);

	if (!cursor)
	{
		cursor = &curs;

		cursor->item = 0;
		cursor->node = conn->nodes[cursor->item];
	}
#endif /* USE_CONN_HASH */

	if (iterator && iterator->data == c)
		iterator = NULL;

	list_lock_remove (connections, c);

	if (!iterator)
		iterator = connections->list;
}

/*****************************************************************************/

#ifndef USE_CONN_HASH
static Connection *locate_node (Connection *c, Node *node,
								unsigned long *ip)
{
	if (node->ip == *ip)
		return c;

	return NULL;
}
#endif /* !USE_CONN_HASH */

Connection *conn_lookup (unsigned long ip)
{
	Connection *c;

#ifdef USE_CONN_HASH

	c = hash_table_lookup (conn, ip);

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
	ft_uint32 local_ver;

	if (!NODE (c)->port)
		return NULL;

	local_ver = ft_make_version (OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO);

	/* do not connect to a node you know isn't the right version */
	if (node->version && node->version != local_ver)
		return NULL;

	/* only count processed if conn_auth succeeded...skip "bad" nodes :) */
	if (ft_connect (c) < 0 && processed)
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
		NODE (c)->disconnect_cnt = 60;

		node_disconnect (c);

		if (processed)
			(*processed)++;
	}

	return NULL;
}

/* core logic used to determine how frequent to make new connections */
void conn_maintain ()
{
	/* disconnect idle user <-> user connections */
	if (NODE (ft_self)->class == NODE_USER)
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
}

/*****************************************************************************/

#if 0
static void conn_validate (List *list, List *iter)
{
	List *ptr;
	int   found = FALSE;

	if (!list || !iter)
		return;

	for (ptr = list; ptr; ptr = list_next (ptr))
	{
		if (ptr == iter)
		{
			found = TRUE;
			break;
		}
	}

	assert (found == TRUE);
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

#if 0
	/* TEMP -- DEBUGGING */
	conn_validate (*op_list, *op_iter);
#endif

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
		node = NODE (c);

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

	connections->list =
	    list_foreach_remove (connections->list,
	                         (ListForeachFunc) conn_clear_entry, NULL);

	list_lock_free (connections);

	/* TODO -- clear conn spread and the hash table */
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
	if (!NODE (c)->ip)
		return FALSE;

	/* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
	if (OPENFT_LOCAL_MODE)
	{
		if (!net_match_host (NODE (c)->ip, OPENFT_LOCAL_ALLOW))
			return FALSE;
	}

	/* this connection is firewalled, and therefore unavailable for outgoing
	 * connection */
	if (outgoing && NODE (c)->port == 0)
		return FALSE;

	/* if this node is flooding us, ignore it */
	if (NODE (c)->disconnect_cnt > MAX_DISCONNECT_COUNT)
		return FALSE;

	/* mmmm, powerful nodes */
	if (NODE (c)->class & (NODE_SEARCH | NODE_INDEX))
		return TRUE;

	if (NODE (ft_self)->class == NODE_USER)
	{
		if (conn_length (NODE_USER, NODE_CONNECTED) > MAX_USER_CONNECTIONS)
			return FALSE;
	}

	threshold = conn_length (NODE_NONE, NODE_CONNECTING);

	if (threshold < MAX_PENDING_CONNECTIONS)
		return TRUE;

#if 0
	TRACE (("%s rejected, threshold is %i",
	        net_ip_str (NODE (c)->ip), threshold));
#endif

	return FALSE;
}
