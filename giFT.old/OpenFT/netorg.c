/*
 * netorg.c - core OpenFT network organization
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

/**/extern Connection *ft_self;

static List *connections = NULL;
static List *iterator    = NULL;  /* current "position" in connections */

/*****************************************************************************/

/* maximum number of connections allowed to be in NODE_CONNECTING state when
 *  * connecting to nodelist responses */
#define MAX_PENDING_CONNECTIONS 20

/*****************************************************************************/

void conn_add (Connection *c)
{
	if (!c)
		return;

	connections = list_append (connections, c);

	if (!iterator)
		iterator = connections;
}

void conn_remove (Connection *c)
{
	if (!c)
		return;

	if (iterator && iterator->data == c)
		iterator = NULL;

	connections = list_remove (connections, c);

	if (!iterator)
		iterator = connections;
}

void conn_sort (CompareFunc func)
{
	connections = list_sort (connections, func);

	/* iterator should be reset to the head */
	iterator = connections;
}

/*****************************************************************************/

static Connection *node_reconnect (Connection *c, Node *node, int *processed)
{
	if (!NODE (c)->port)
		return NULL;

	ft_connect (c);

	if (processed)
		(*processed)++;

	return NULL;
}

static Connection *node_disconnect_idle (Connection *c, Node *node,
                                         void *udata)
{
	if (node->class != NODE_USER)
		return NULL;

	if (node->sent_list && node->recv_list)
	{
		TRACE_SOCK (("disconnecting..."));
		node_disconnect (c);
	}

	return NULL;
}

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
	              NODE_NONE, NODE_DISCONNECTED, 5);

	/* we aren't satisfied */
	if (conn_length (NODE_SEARCH, NODE_CONNECTED) < 15)
	{
		int processed = 0;

		conn_foreach ((ConnForeachFunc) node_reconnect, &processed,
		              NODE_SEARCH, NODE_DISCONNECTED, 10);

		/* we still aren't satisfied */
		if (processed < 10)
		{
			/* try 10 more users */
			conn_foreach ((ConnForeachFunc) node_reconnect, NULL,
			              NODE_NONE, NODE_DISCONNECTED, 10);
		}
	}
}

/*****************************************************************************/

#if 0
static void conn_dump ()
{
	List *ptr;
	int   found = FALSE;

	for (ptr = connections; ptr; ptr = list_next (ptr))
	{
		if (ptr == iterator)
			found = TRUE;
	}

	if (!found)
		printf ("ITERATOR NOT FOUND!!?!?!\n");
}
#endif

/* connection iterator */
Connection *conn_foreach (ConnForeachFunc func, void *user_data,
                          unsigned short klass, unsigned short state,
                          unsigned short iter)
{
	Connection *ret = NULL;
	Connection *conn;
	Node       *node;
	List       *ptr;
	int         looped = FALSE;

	assert (func != NULL);

	/* if iteration was supplied, use iterator, otherwise ignore it */
	ptr = (iter) ? iterator : connections;

	/* this is cute */
	for (;; ptr = list_next (ptr))
	{
		/* if iter, start over from the beginning of the list */
		if (iter && !ptr && !looped)
		{
			ptr = connections;
			looped = TRUE;
		}

		if (!ptr)
			break;

#if 0
		if (!ptr->data)
		{
			GIFT_ERROR (("fucking a, %p, %p", connections, iterator));
			conn_dump ();
			ptr = iterator = connections;
		}
#endif

		conn = ptr->data;
		node = NODE (conn);

#if 0
		printf ("*** evaluating %s:%hu (%hu, %hu, %i)\n",
				net_ip_str (node->ip), node->port,
				klass, state, iter);
#endif

		/* filter out the conditions supplied */
		if (klass && !(node->class & klass))
			continue;

		if (state != (unsigned short) -1 && node->state != state)
			continue;

		/* callback */
		ret = (*func) (conn, node, user_data);

#if 0
		printf ("*** %p (%p)\n", func, ret);
#endif

		/* iter may have been passed as 0, if that is the case, ignore its
		 * behavior completely */
		if (iter)
		{
			iterator = ptr->next;
			iter--;

			if (!iter)
				break;
		}

		if (ret)
			break;
	}

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

	connections =
	    list_foreach_remove (connections,
	                         (ListForeachFunc) conn_clear_entry, NULL);
}

/*****************************************************************************/

static Connection *conn_counter (Connection *c, Node *node, int *length)
{
	(*length)++;
	return NULL;
}

int conn_length (unsigned short klass, unsigned short state)
{
	int length = 0;

	conn_foreach ((ConnForeachFunc) conn_counter, &length, klass, state, 0);

	return length;
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

	/* this connection is firewalled, and therefore unavailable for outgoing
	 * connection */
	if (outgoing && NODE (c)->port == 0)
		return FALSE;

	/* mmmm, powerful nodes */
	if (NODE (c)->class & (NODE_SEARCH | NODE_INDEX))
		return TRUE;

	threshold = conn_length (NODE_NONE, NODE_CONNECTING);

	if (threshold < MAX_PENDING_CONNECTIONS)
		return TRUE;

#if 0
	TRACE (("%s rejected, threshold is %i",
	        net_ip_str (NODE (c)->ip), threshold));
#endif

	return FALSE;
}
