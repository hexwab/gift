/*
 * $Id: ft_conn.c,v 1.20 2003/06/24 19:57:20 jasta Exp $
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

#include "ft_openft.h"

#include "ft_netorg.h"
#include "ft_node_cache.h"
#include "ft_conn.h"

/*****************************************************************************/

/*
 * Number of times ft_conn_maintain has been called in our lifetime.
 * Primarily used for heartbeat checks, but may be useful for some purpose
 * later.
 */
static unsigned int timer_cnt = 0;

/*****************************************************************************/

static int gather_stats (FTNode *node, void *udata)
{
	ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, NULL);
	return TRUE;
}

static int drop_heartbeat (FTNode *node, void *udata)
{
	/* this user has sufficiently responded (or they were nevere given the
	 * chance), reset the value for the next interval update */
	if (node->session->keep &&
	    (node->session->heartbeat == 0 || node->session->heartbeat > 1))
	{
		node->session->heartbeat = 1;
		node->session->keep      = FALSE;

		return FALSE;
	}

	ft_node_err (node, FT_ERROR_UNKNOWN, "heartbeat timeout");
	ft_session_stop (FT_CONN(node));

	return TRUE;
}

static int set_keep (FTNode *node, void *udata)
{
	node->session->keep = TRUE;
	return TRUE;
}

static int send_heartbeat (FTNode *node, Dataset *sent)
{
	/* already sent a heartbeat here */
	if (dataset_lookup (sent, &node->ip, sizeof (node->ip)))
		return FALSE;

	ft_packet_sendva (FT_CONN(node), FT_PING_REQUEST, 0, NULL);
	set_keep (node, NULL);

	dataset_insert (&sent, &node->ip, sizeof (node->ip), "in_addr_t", 0);

	return TRUE;
}

int ft_conn_need_parents (void)
{
	int n;

	n = ft_netorg_length (FT_NODE_PARENT, FT_NODE_CONNECTED);

	return (n < FT_SEARCH_PARENTS);
}

int ft_conn_need_peers (void)
{
	int n;

	/* the new design says that peers are required only by the search class */
	if (!(FT_SELF->klass & FT_NODE_SEARCH))
		return FALSE;

	n = ft_netorg_length (FT_NODE_SEARCH, FT_NODE_CONNECTED);

	return (n < FT_SEARCH_PEERS);
}

int ft_conn_need_index (void)
{
	int n;

	n = ft_netorg_length (FT_NODE_INDEX, FT_NODE_CONNECTED);

	if (FT_SELF->klass & FT_NODE_INDEX)
		n++;

	return (n < 1);
}

static int get_nodes (FTNode *node, void *udata)
{
	ft_packet_sendva (FT_CONN(node), FT_NODELIST_REQUEST, 0, NULL);
	return TRUE;
}

static int new_parents (FTNode *node, void *udata)
{
	if (node->klass & FT_NODE_PARENT)
		return FALSE;

	if (node->version >= OPENFT_0_0_9_6)
		ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);

	return TRUE;
}

static int make_conn (FTNode *node, void *udata)
{
	return (ft_session_connect (node) >= 0);
}

static int keep_alive (void)
{
	Dataset *sent;
	int      n;                        /* total nodes pinged */
	int      p;                        /* number of parents pinged */

	/* avoid sending dupe heartbeats */
	if (!(sent = dataset_new (DATASET_HASH)))
		return 0;

	/* set the keep flag but dont send a heartbeat (this user will be
	 * pinging us, no need for a bidirectional keep-alive) */
	ft_netorg_foreach (FT_NODE_CHILD, FT_NODE_CONNECTED, FT_MAX_CHILDREN,
	                   FT_NETORG_FOREACH(set_keep), NULL);

#if 0
	/* temporary hack to maintain every connection on INDEX nodes so that
	 * we can assure stats are reported back */
	if (FT_SELF->klass & FT_NODE_INDEX)
	{
		ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(set_keep), NULL);
	}
#endif

	/* all INDEX node connections should be maintained all the time
	 * (THIS WILL CHANGE SOON!) */
	n = ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 0,
	                       FT_NETORG_FOREACH(send_heartbeat), sent);

	/* keep alive all parent connections, assume that FT_SEARCH_PARENTS is
	 * also a minimum value, not a restrictive one */
	p = ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, FT_SEARCH_PARENTS,
	                       FT_NETORG_FOREACH(send_heartbeat), sent);
	n += p;

	if (p < FT_SEARCH_PEERS && FT_SELF->klass & FT_NODE_SEARCH)
	{
		int iter = FT_SEARCH_PEERS - p;

		/* keep alive all peer connections used to satisfy our childrens
		 * searches (as well as our own) */
		n += ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, iter,
		                        FT_NETORG_FOREACH(send_heartbeat), sent);
	}

	FT->DBGFN (FT, "kept %i connections alive", n);

	dataset_clear (sent);

	return n;
}

/*
 * Main connection maintenace timer.  Eventually this function should take
 * advantage of the node weighting/distribution code that ft_conn_initial
 * uses, but for the time being it is just using crappy hardcoded logic to
 * determine how many nodes are ideal to connect to.
 */
int ft_conn_maintain (void *udata)
{
	int n;                             /* this variable is re-used here */

	/* make sure the nodes cache is read in and written out properly */
	ft_node_cache_update ();

	/* gather stats, either from our index nodes if we are a search node or
	 * our parent if we are a child */
	n = ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 10,
	                       FT_NETORG_FOREACH(gather_stats), NULL);

	if (n < 1)
	{
		ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(gather_stats), NULL);
	}

	/* send out our heartbeat to all nodes we wish to maintain active
	 * connections with (search and index nodes) */
	keep_alive ();

	/* called every interval * 2 (4 minutes) */
	if (timer_cnt & 1)
	{
		/* disconnect all users which do not have at least one heartbeat count
		 * and then reset all heartbeat values */
		ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(drop_heartbeat), NULL);
	}

	/* if we desire any kind of new OpenFT connection, randomly fire off
	 * connections and let the logic here next call handle whether or not we
	 * keep them around */
	if (ft_conn_need_parents () || ft_conn_need_index () ||
	    ft_conn_need_peers ())
	{
		FT->DBGFN (FT, "seeking more parent/index/peers...");

		/* ask for more nodelist information just in case */
		ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 10,
		                   FT_NETORG_FOREACH(get_nodes), NULL);

		/* attempt to become a child of FT_SEARCH_PARENTS currently non-parent
		 * search nodes */
		ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, FT_SEARCH_PARENTS,
		                   FT_NETORG_FOREACH(new_parents), NULL);

		/* establish some new search node connections */
		n = ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_DISCONNECTED, FT_SEARCH_PARENTS * 10,
		                       FT_NETORG_FOREACH(make_conn), NULL);

		/* we're getting desperate for parents, start making connections
		 * to user nodes */
		if (n < (FT_SEARCH_PARENTS * 5))
		{
			FT->DBGFN (FT, "desperately seeking more parent/index/peers...");

			ft_netorg_foreach (FT_NODE_USER, FT_NODE_DISCONNECTED, 25,
			                   FT_NETORG_FOREACH(make_conn), NULL);
		}
	}

	timer_cnt++;
	return TRUE;
}

/*****************************************************************************/

/* attempts to classify the value given the distribution set defined by the
 * variable arguments */
static int math_dist (long value, int nargs, ...)
{
	va_list args;
	int     i;

	va_start (args, nargs);

	for (i = 0; i < nargs; i++)
	{
		int point = va_arg (args, long);

		if (value > point)
			continue;

		break;
	}

	va_end (args);

	return i;
}

static int get_cost (FTNode *node, time_t current)
{
	int n, x;

	n = math_dist ((long)node->uptime, 8,
	               0,          2 * EDAYS,  4 * EDAYS,   8 * EDAYS, 16 * EDAYS,
	               32 * EDAYS, 64 * EDAYS, 128 * EDAYS);

	x = math_dist ((long)(current - node->last_session), 7,
	               0,          1 * EHOURS, 2 * EHOURS, 3 * EHOURS, 4 * EHOURS,
	               5 * EHOURS, 6 * EHOURS);

	n += 7 - x;

	return ((n + 1) * 2);
}

static int start_connection (FTNode *node, int *weight)
{
	int cost;

	/* not enough weight left, but we cant really abort the loop so we'll
	 * just stop counting iter */
	if (*weight == 0)
		return FALSE;

	/* this node is free, and therefore we don't want it ... low quality
	 * foreign crap! :) */
	if ((cost = get_cost (node, time (NULL))) <= 0)
		return FALSE;

	/* only subtract weight if the connection was actually made */
	if (ft_session_connect (node) < 0)
		return FALSE;

	FT->DBGFN (FT, "%s: costs %i", ft_node_fmt (node), cost);
	*weight -= CLAMP(cost,0,*weight);

	return TRUE;
}

BOOL ft_conn_initial ()
{
	int n;
	int weight = FT_INITIAL_WEIGHT;

	/* we need some nodes to work with! */
	ft_node_cache_update ();

	/*
	 * We are utilizing a node-weighting system to determine how many nodes
	 * to connect to.  In general, nodes with very high costs are going to be
	 * higher quality, and we therefore want to make fewer connections.  In
	 * the long run, this is done to minimize the need to create TCP
	 * connections that will not be desired.
	 */
	n = ft_netorg_foreach (FT_NODE_USER, FT_NODE_DISCONNECTED, 0,
	                       FT_NETORG_FOREACH(start_connection), &weight);

	FT->DBGFN (FT, "began %i connections (remaining weight: %i)", n, weight);
	return TRUE;
}

/*****************************************************************************/

static int check_local_allow (FTNode *node)
{
    /* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
    if (FT_LOCAL_MODE)
    {
		if (!net_match_host (node->ip, FT_LOCAL_ALLOW))
			return FALSE;
	}

	return TRUE;
}

BOOL ft_conn_auth (FTNode *node, int outgoing)
{
	if (!check_local_allow (node))
		return FALSE;

	/* we won't authorize a duplicate connection */
	if (node->state != FT_NODE_DISCONNECTED)
		return FALSE;

	/* avoid making outgoing connections to users that we believe are
	 * outdated */
	if (outgoing && node->version &&
	    FT_VERSION_LT(node->version, FT_VERSION_LOCAL))
		return FALSE;

#if 0
	FT->DBGFN (FT, "%s(%i): authorized connection",
	           ft_node_fmt (node), outgoing);
#endif

	return TRUE;
}
