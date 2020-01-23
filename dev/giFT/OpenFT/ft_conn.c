/*
 * ft_conn.c
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
#include "ft_node_cache.h"
#include "ft_conn.h"

/*****************************************************************************/

/*
 * Number of times ft_conn_maintain has been called in our lifetime, tracked
 * so that we may use a 2 minute timer to emulate any timer interval
 * divisible by 2.  It's a hack, I know.
 */
static unsigned int timer_cnt = 0;

/*****************************************************************************/

static int gather_stats (FTNode *node, void *udata)
{
	ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, "h",
	                  1 /* RETRIEVE INFO */);
	return TRUE;
}

static int drop_heartbeat (FTNode *node, void *udata)
{
	/* this user has sufficiently responded (or they were nevere given the
	 * chance), reset the value for the next interval update */
	if (node->session->heartbeat == 0 || node->session->heartbeat > 1)
	{
		node->session->heartbeat = 1;
		return FALSE;
	}

	TRACE (("%s: heartbeat failed", ft_node_fmt (node)));
	ft_session_stop (FT_CONN(node));

	return TRUE;
}

static int send_heartbeat (FTNode *node, void *udata)
{
	ft_packet_sendva (FT_CONN(node), FT_PING_REQUEST, 0, NULL);
	return TRUE;
}

static int need_parents ()
{
	int n;

	n = ft_netorg_length (NODE_PARENT, NODE_CONNECTED);

	return (n < FT_MAX_PARENTS);
}

static int get_nodes (FTNode *node, void *udata)
{
	ft_packet_sendva (FT_CONN(node), FT_NODELIST_REQUEST, 0, NULL);
	return TRUE;
}

static int new_parents (FTNode *node, void *udata)
{
	if (node->klass & NODE_PARENT)
		return FALSE;

	ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);
	return TRUE;
}

static int make_conn (FTNode *node, void *udata)
{
	return (ft_session_connect (node) >= 0);
}

/*
 * Main connection maintenace timer.  Eventually this function should take
 * advantage of the node weighting/distribution code that ft_conn_initial
 * uses, but for the time being it is just using crappy hardcoded logic to
 * determine how many nodes are ideal to connect to.
 */
int ft_conn_maintain (void *udata)
{
	int n;

	TRACE (("%p", udata));

	/* make sure the nodes cache is read in and written out properly */
	ft_node_cache_update ();

	ft_netorg_foreach (NODE_INDEX, NODE_CONNECTED, 10,
	                   FT_NETORG_FOREACH(gather_stats), NULL);

	/* send out our heartbeat to all nodes we wish to maintain active
	 * connections with (search and index nodes) */
	ft_netorg_foreach (NODE_SEARCH | NODE_INDEX, NODE_CONNECTED, 15,
	                   FT_NETORG_FOREACH(send_heartbeat), NULL);

	/* called every interval * 2 (4 minutes) */
	if (timer_cnt % 2 == 0)
	{
		/* disconnect all users which do not have at least one heartbeat count
		 * and then reset all heartbeat values */
		ft_netorg_foreach (NODE_USER, NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(drop_heartbeat), NULL);
	}

	if (need_parents ())
	{
		/* ask for more nodelist information just in case */
		ft_netorg_foreach (NODE_USER, NODE_CONNECTED, 10,
		                   FT_NETORG_FOREACH(get_nodes), NULL);

		/* attempt to become a child of FT_MAX_PARENTS currently non-parent
		 * search nodes */
		ft_netorg_foreach (NODE_SEARCH, NODE_CONNECTED, FT_MAX_PARENTS,
		                   FT_NETORG_FOREACH(new_parents), NULL);

		/* establish some new search node connections */
		n = ft_netorg_foreach (NODE_SEARCH, NODE_DISCONNECTED, FT_MAX_PARENTS,
		                       FT_NETORG_FOREACH(make_conn), NULL);

		/* we're getting desperate for parents, start making connections
		 * to user nodes */
		if (n < FT_MAX_PARENTS)
		{
			ft_netorg_foreach (NODE_USER, NODE_DISCONNECTED, 25,
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

	TRACE_FUNC();

	/* not enough weight left, but we cant really abort the loop so we'll
	 * just stop counting iter */
	if (*weight == 0)
		return FALSE;

	/* this node is free, and therefore we don't want it ... low quality
	 * foreign crap! :) */
	if ((cost = get_cost (node, time (NULL))) <= 0)
		return FALSE;

	TRACE (("%s: costs %i", ft_node_fmt (node), cost));

	/* only subtract weight if the connection was actually made */
	if (ft_session_connect (node) < 0)
		return FALSE;

	*weight -= CLAMP(cost,0,*weight);

	return TRUE;
}

int ft_conn_initial ()
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
	n = ft_netorg_foreach (NODE_USER, NODE_DISCONNECTED, 0,
	                       FT_NETORG_FOREACH(start_connection), &weight);

	TRACE (("began %i connections (remaining weight: %i)", n, weight));
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

int ft_conn_auth (FTNode *node, int outgoing)
{
	TRACE_FUNC();
	if (!check_local_allow (node))
		return FALSE;

	TRACE((""));

	/* we won't authorize a duplicate connection */
	if (node->state != NODE_DISCONNECTED)
		return FALSE;

	TRACE((""));
	/* avoid making outgoing connections to users that we believe are
	 * outdated */
	if (outgoing && node->version && node->version < FT_VERSION_LOCAL) {
		TRACE(("%s: %x",ft_node_fmt(node),node->version));
		return FALSE;
	}

	TRACE (("%s(%i): authorized connection", ft_node_fmt (node), outgoing));
	return TRUE;
}
