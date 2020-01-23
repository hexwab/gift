/*
 * $Id: ft_conn.c,v 1.55 2004/05/05 04:42:42 jasta Exp $
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

#include "ft_search_db.h"              /* automatic node promotion */

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */

/* TODO: move the RLIMIT_OFILE mess to autoconf */
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>             /* getrlimit/setrlimit */
# ifndef RLIMIT_OFILE
#  ifdef RLIMIT_NOFILE
#	define RLIMIT_OFILE RLIMIT_NOFILE
#  else /* !RLIMIT_NOFILE */
#	define RLIMIT_OFILE 7
#  endif /* RLIMIT_NOFILE */
# endif /* RLIMIT_OFILE */
#endif /* HAVE_SYS_RESOURCE_H */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */

/*****************************************************************************/

/*
 * Number of times ft_conn_maintain has been called in our lifetime.
 * Primarily used for heartbeat checks, but may be useful for some purpose
 * later.
 */
static unsigned int timer_cnt = 0;

/*
 * Store the maximum number of connections to allow in ft_conn_auth.
 */
static int max_active = -1;

/**
  * The number of 2 minute blocks we can go without a parent before we
  * begin automatic search node promotion tests.
  */
#define IMPOVERISH_MIN (4)

/**
  * Ditto, but for seeing if our services as a search node are no longer
  * required.
  */
#define SURPLUS_MIN (6)

static unsigned int poverty_count = 0;

static unsigned int surplus_count = 0;

/*****************************************************************************/

static BOOL drop_notalive (FTNode *node, void *udata)
{
	BOOL ret = TRUE;

	if (!node->session->keep)
	{
		ft_node_err (node, FT_ERROR_IDLE, "Dummy remote peer");
		ft_session_stop (FT_CONN(node));
	}
	else
	{
		FTSession *s;

		/* shorthand */
		s = node->session;
		assert (s != NULL);

		/*
		 * This user has sufficiently responded (or they were never given the
		 * chance), reset the value for the next interval update.  We will
		 * reset the heartbeat to 1 indicating that they have been given the
		 * chance to reply, and if it does not increment by the next call,
		 * they will be considered to be dead.
		 */
		if (s->heartbeat == 0 || s->heartbeat > 1)
		{
			/*
			 * We are going to set keep back to FALSE so that the next pass
			 * of keep_alive will have to evaluate whether or not this node
			 * is particularly useful to us.  At some point it might be
			 * better to simply improve the purpose system so that we do not
			 * have to continually poll the usefulness of our connections.
			 */
			s->heartbeat = 1;
			s->keep = FALSE;

			/* the node "failed" to be removed, so dont count it
			 * in the ft_netorg_foreach parent call */
			ret = FALSE;
		}
		else
		{
			ft_node_err (node, FT_ERROR_UNKNOWN, "heartbeat timeout");
			ft_session_stop (FT_CONN(node));
		}
	}

	return ret;
}

/*****************************************************************************/

static int set_keep (FTNode *node, void *udata)
{
	node->session->keep = TRUE;
	return TRUE;
}

static int send_heartbeat (FTNode *node, Dataset *sent)
{
	in_addr_t host = node->ninfo.host;

	/* already sent a heartbeat here */
	if (dataset_lookup (sent, &host, sizeof (host)))
		return FALSE;

	ft_packet_sendva (FT_CONN(node), FT_PING_REQUEST, 0, NULL);
	set_keep (node, NULL);

	dataset_insert (&sent, &host, sizeof (host), "in_addr_t", 0);

	return TRUE;
}

/*****************************************************************************/

BOOL ft_conn_need_parents (void)
{
	int n;

	n = ft_netorg_length (FT_NODE_PARENT, FT_NODE_CONNECTED);

	return BOOL_EXPR (n < FT_CFG_SEARCH_PARENTS);
}

BOOL ft_conn_need_peers (void)
{
	int n;

	/* the new design says that peers are required only by the search class */
	if (!(openft->ninfo.klass & FT_NODE_SEARCH))
		return FALSE;

	n = ft_netorg_length (FT_NODE_SEARCH, FT_NODE_CONNECTED);

	if (n < FT_CFG_SEARCH_MINPEERS)
		return TRUE;

	return FALSE;
}

BOOL ft_conn_need_index (void)
{
	int n;

	/* only search nodes need to actually maintain index node connections
	 * anymore as they will cache the responses for their children */
	if (!(openft->ninfo.klass & FT_NODE_SEARCH))
		return FALSE;

	/* why would an index node need another one? */
	if ((openft->ninfo.klass & FT_NODE_INDEX))
		return FALSE;

	n = ft_netorg_length (FT_NODE_INDEX, FT_NODE_CONNECTED);

	return BOOL_EXPR (n < 1);
}

int ft_conn_children_left (void)
{
	int n;
	int max;

	if (!(openft->ninfo.klass & FT_NODE_SEARCH))
		return 0;

	n = ft_netorg_length (FT_NODE_CHILD, FT_NODE_CONNECTED);
	max = FT_CFG_MAX_CHILDREN;

	if (n >= max)
		return 0;

	return (max - n);
}

/*****************************************************************************/

static BOOL gather_stats (FTNode *node, void *udata)
{
	ft_packet_sendva (FT_CONN(node), FT_STATS_REQUEST, 0, NULL);
	return TRUE;
}

static BOOL get_nodes (FTNode *node, void *udata)
{
#if 0
  //	ft_packet_sendva (FT_CONN(node), FT_NODELIST_REQUEST, 0, NULL);
	ft_packet_sendva (FT_CONN(node), FT_NODELIST_REQUEST, 0, "hh", FT_NODE_SEARCH, 500);
#else
	FTPacket *pkt;

	if (!(pkt = ft_packet_new (FT_NODELIST_REQUEST, 0)))
		return FALSE;

	ft_packet_put_uint32 (pkt, 0, TRUE);
	ft_packet_put_uint32 (pkt, 0, TRUE);

	if (ft_packet_send (FT_CONN(node), pkt) < 0)
		return FALSE;

#endif
	return TRUE;
}

/*****************************************************************************/

static BOOL new_parents (FTNode *node, void *udata)
{
	if (node->ninfo.klass & FT_NODE_PARENT ||
	    node->ninfo.klass & FT_NODE_PARENT_FULL)
		return FALSE;

	ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);

	return TRUE;
}

static BOOL make_conn_purpose (FTNode *node, ft_purpose_t goal)
{
	int cret;

	/*
	 * HACK: We know that this function will only be used when establishing
	 * new connections as a result of lacking parents, peers, and/or index
	 * nodes after initial connect, so we can apply some connection
	 * throttling to ease overall network load.
	 */
	if (!node->ninfo.indirect &&
	    (node->last_session + (6 * EMINUTES)) >= time (NULL))
	{
#if 0
		FT->DBGFN (FT, "refusing connection to %s: previous "
		               "session too recent", ft_node_fmt (node));
#endif
		return FALSE;
	}

	cret = ft_session_connect (node, goal);
	return BOOL_EXPR (cret >= 0);
}

static BOOL make_conn (FTNode *node, void *udata)
{
	return make_conn_purpose (node, FT_PURPOSE_UNDEFINED);
}

static BOOL make_conn_for_new_parents (FTNode *node, void *udata)
{
	if (node->ninfo.klass & FT_NODE_PARENT_FULL)
		return FALSE;

	return make_conn_purpose (node, FT_PURPOSE_PARENT_TRY);
}

static BOOL make_conn_get_nodes (FTNode *node, void *udata)
{
	return make_conn_purpose (node, FT_PURPOSE_GET_NODES);
}

/*****************************************************************************/

static int keep_alive (void)
{
	Dataset *sent;
	int      n = 0;                    /* total nodes pinged */
	int      p;                        /* number of parents pinged */
	int      children;
	int      parents;
	int      peers;

	/* avoid sending dupe heartbeats */
	if (!(sent = dataset_new (DATASET_HASH)))
		return 0;

	/* shorthand... */
	children = FT_CFG_MAX_CHILDREN;
	parents  = FT_CFG_SEARCH_PARENTS;
	peers    = FT_CFG_SEARCH_MAXPEERS;

	/* set the keep flag but dont send a heartbeat (this user will be
	 * pinging us, no need for a bidirectional keep-alive) */
	ft_netorg_foreach (FT_NODE_CHILD, FT_NODE_CONNECTED, children,
	                   FT_NETORG_FOREACH(set_keep), NULL);

	if (openft->ninfo.klass & FT_NODE_SEARCH)
	{
		/* maintain a connection to a small group of index nodes to mix the
		 * stats around a little... */
		n += ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_CONNECTED, 4,
		                        FT_NETORG_FOREACH(send_heartbeat), sent);
	}

	/* keep alive all parent connections (up to FT_SEARCH_PARENTS, to
	 * eliminate any possible race conditions or bugs that exist elsewhere
	 * in the code) */
	p = ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, parents,
	                       FT_NETORG_FOREACH(send_heartbeat), sent);
	n += p;

	if (p < peers && openft->ninfo.klass & FT_NODE_SEARCH)
	{
		int iter = peers - p;

		/* keep alive all peer connections used to satisfy our childrens
		 * searches (as well as our own) */
		n += ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, iter,
		                        FT_NETORG_FOREACH(send_heartbeat), sent);
	}

	FT->DBGFN (FT, "kept %i connections alive", n);

	dataset_clear (sent);

	return n;
}

/*****************************************************************************/

static BOOL check_promote (void)
{
	float diceroll;
	int ret;
	
	assert (!(openft->ninfo.klass & FT_NODE_SEARCH));
	if (!(openft->klass_max & FT_NODE_SEARCH))
		return FALSE;

	diceroll = ((float)rand() / (RAND_MAX + 1.0));
	
	if (diceroll > FT_CFG_PROMOTE_CHANCE)
		return FALSE;

	/* if you're hap^H^H^Hfirewalled and you know it... */
	if (openft->ninfo.port_openft == 0)
		return FALSE;

	/* insert other	tests here */

	FT->warn (FT, "Automatically promoting to FT_NODE_SEARCH status");

	if ((ret = ft_search_db_init (FT_CFG_SEARCH_ENV_PATH, FT_CFG_SEARCH_ENV_CACHE)))
		openft->ninfo.klass |= FT_NODE_SEARCH;
	else
		FT->DBGFN (FT, "search db startup failed");
	
	surplus_count = 0;

	return ret;
}

static BOOL drop_after_demotion (FTNode *node, void *udata)
{
	if (!node->ninfo.klass & FT_NODE_PARENT)
		ft_session_stop (FT_CONN(node));
	return TRUE;
}

static BOOL drop_children (FTNode *node, void *udata)
{
	ft_node_remove_class (node, FT_NODE_CHILD);
	return TRUE;
}

static BOOL check_demote (void)
{
	int n, min;
	float chance, diceroll;

	assert (openft->ninfo.klass & FT_NODE_SEARCH);

	/* Don't demote unless we automatically promoted earlier */
	if (openft->klass_min & FT_NODE_SEARCH)
		return FALSE;

	n = ft_netorg_length (FT_NODE_CHILD, FT_NODE_CONNECTED);
	min = FT_CFG_MIN_CHILDREN;

	if (n >= min)
	{
		surplus_count = 0;
		return FALSE;
	}

	/* wait a while for child acquisition */
	if (++surplus_count < SURPLUS_MIN)
		return FALSE;

	/* 
	 * This needs to be random so that newly-orphaned children
	 * have time to locate a new parent before said parent demotes
	 * itself too.
	 *
	 * This should become less critical as network size increases.
	 */
	chance = FT_CFG_DEMOTE_CHANCE;
	chance *= ((float)min / (n + 1)); /* few children == more chance to demote */

	/*
	 * we haven't had any incoming connections yet; we might be
         * firewalled but unaware of the fact...
	 */
	if (openft->ninfo.indirect)
		chance *= 5; 

	diceroll = ((float)rand() / (RAND_MAX + 1.0));

	if (diceroll > chance)
		return FALSE;
	
	/* OK, do it */
	FT->DBGFN (FT, "demoting: firewalled=%d, children=%d, min=%d, surplus_count=%d",
		   openft->ninfo.indirect, n, min, surplus_count);

	openft->ninfo.klass &= ~FT_NODE_SEARCH;
	ft_search_db_destroy ();
	
	/*
	 * Drop all non-PARENT connections first.  FIXME: this might
	 * not be necessary, and is yet another place where network
	 * topology is hardcoded, but I'm trying it anyway in an
	 * effort to remove demoted nodes as quickly as possible.
	 */
	ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 0,
			   FT_NETORG_FOREACH(drop_after_demotion), NULL);

	/*
	 * Remove CHILD status from any remaining connections.  Note
	 * that this does *not* tell the remote node that we've done
	 * so, thus causing problems if our PARENT is also our
	 * CHILD...
	 */
	ft_netorg_foreach (FT_NODE_CHILD, FT_NODE_CONNECTED, 0,
			       FT_NETORG_FOREACH(drop_children), NULL);
	
	return TRUE;
}

static int acquire_new_stuff (void)
{
	BOOL need_parents;
	BOOL need_peers;
	BOOL need_index;
	int  n = 0;

	/* determine what connectivity we currently have and whether or not
	 * we need to actively pursue new connections */
	need_parents = ft_conn_need_parents();
	need_peers   = ft_conn_need_peers();
	need_index   = ft_conn_need_index();

	/* 
	 * 4 possibilities here:
	 * search orphan action
	 * no     no     stable, do nothing
	 * no     yes    try autopromotion
	 * yes    no     check if we're no longer needed
	 * yes    yes    wait for a parent (don't demote without one)
	 *
	 * In particular, this last case means that the minimum child
	 * count is inapplicable on tiny networks with only one search
	 * node.  
	 */

	if (openft->ninfo.klass & FT_NODE_SEARCH)
	{
		poverty_count = 0;

		if (!need_parents)
			check_demote ();
	}
	else 
	{
		if (!need_parents)
			poverty_count = 0;
		else
			if (++poverty_count >= IMPOVERISH_MIN)
				check_promote ();
	}

	/* nothing is needed, get out of here */
	if (!need_parents && !need_peers && !need_index)
		return 0;

	if (need_parents)
		FT->DBGFN (FT, "seeking more parents...");

	if (need_peers)
		FT->DBGFN (FT, "seeking more peers...");

	if (need_index)
		FT->DBGFN (FT, "seeking more index nodes...");

	/* ask for more nodelist information just in case */
	ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 10,
	                   FT_NETORG_FOREACH(get_nodes), NULL);

	if (need_parents)
	{
		/* attempt to become a child of FT_CFG_SEARCH_PARENTS currently
		 * non-parent search nodes */
		ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED,
		                   FT_CFG_SEARCH_PARENTS,
		                   FT_NETORG_FOREACH(new_parents), NULL);
	}

	if (need_parents || need_peers)
	{
		/* establish some new search node connections */
		n += ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_DISCONNECTED, 10,
		                        FT_NETORG_FOREACH(make_conn_for_new_parents),
		                        NULL);

		/* we're getting desperate for parents, start making connections
		 * to user nodes */
		if (n < 3)
		{
			ft_netorg_foreach (FT_NODE_USER, FT_NODE_DISCONNECTED, 15,
			                   FT_NETORG_FOREACH(make_conn_get_nodes), NULL);
		}
	}

	if (need_index)
	{
		/* make a new connection to a few index nodes, we should really use
		 * purpose for this! */
		ft_netorg_foreach (FT_NODE_INDEX, FT_NODE_DISCONNECTED, 3,
		                   FT_NETORG_FOREACH(make_conn), NULL);
	}

	/* HACK: we don't really track the number of new connections acquired,
	 * but we are supposed to */
	return n + 1;
}

/*****************************************************************************/

/*
 * Main connection maintenace timer.  Eventually this function should take
 * advantage of the node weighting/distribution code that ft_conn_initial
 * uses, but for the time being it is just using crappy hardcoded logic to
 * determine how many nodes are ideal to connect to.
 */
int ft_conn_maintain (void *udata)
{
	int n;

	/* make sure the nodes cache is read in and written out properly */
	if (timer_cnt & 1)
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
	 * connections with (mostly defined by class, but can also use the new
	 * purpose settings) */
	keep_alive ();

	/* called every interval * 2 (4 minutes) */
	if (timer_cnt & 1)
	{
		/*
		 * Drop all connections which have been flagged as idle (most peers
		 * should be using the purpose data to determine when is a more
		 * appropriate time to disconnect from us, but we need this just
		 * in case there are broken peers out there that don't even know
		 * to drop their connections.
		 *
		 * This loop will also tackle nodes which have not been properly
		 * sending their heartbeat messages to us.
		 */
		ft_netorg_foreach (FT_NODE_USER, FT_NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(drop_notalive), NULL);
	}

	/* gather new connections as they are required by our current
	 * connectivity state */
	acquire_new_stuff();

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
	                0 * EDAYS,  2 * EDAYS,   4 * EDAYS,  8 * EDAYS, 16 * EDAYS,
	               32 * EDAYS, 64 * EDAYS, 128 * EDAYS);

	x = math_dist ((long)(current - node->last_session), 7,
	               0 * EHOURS, 1 * EHOURS, 2 * EHOURS, 3 * EHOURS,
	               4 * EHOURS, 5 * EHOURS, 6 * EHOURS);

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
	if (ft_session_connect (node, FT_PURPOSE_UNDEFINED) < 0)
		return FALSE;

	FT->DBGFN (FT, "%s: costs %i", ft_node_fmt (node), cost);
	*weight -= CLAMP(cost,0,*weight);

	return TRUE;
}

static int get_fd_limit (void)
{
	int limit = -1;

#if defined(_MSC_VER)
	/* this cannot fail apparently */
	limit = _getmaxstdio();

	/*
	 * With msvcrt the default is 512 fds, max is 2048. This does not
	 * include sockets.
	 *
	 * NOTE: According to MSDN setting the limit is no guarantee that we
	 * can actually use that many fds: "This upper limit may be beyond
	 * what is supported by a particular Win32 platform and configuration".
	 */
	if (limit < 2048)
	{
		if ((limit = _setmaxstdio (2048)) == -1)
		{
			FT->err (FT, "_setmaxstdio(2048): %s", GIFT_STRERROR());
			return limit;
		}
	}
#elif defined(HAVE_SYS_RESOURCE_H)
	struct rlimit rlim;
	int ret;

	if ((ret = getrlimit (RLIMIT_OFILE, &rlim)) != 0)
	{
		FT->err (FT, "getrlimit: %s", GIFT_STRERROR());
		return limit;
	}

	/* make sure we return a meaningful value even if setrlimit fails */
	limit = (int)rlim.rlim_cur;

	/* try to set the new rlimit as high as we are allowed, within reason of
	 * course */
	if (limit < 4096)
	{
		rlim.rlim_cur = MAX (rlim.rlim_max, 4096);

		if ((ret = setrlimit (RLIMIT_OFILE, &rlim)) != 0)
		{
			FT->err (FT, "setrlimit(%d): %s",
			         (int)(rlim.rlim_cur), GIFT_STRERROR());

			return limit;
		}

		limit = (int)rlim.rlim_cur;
	}
#endif /* HAVE_SYS_RESOURCE_H */

	return limit;
}

static int get_max_active (void)
{
	int nconns;
	int fdlim;

	/* the default (-1) asks us to make an educated guess */
	if ((nconns = FT_CFG_MAX_ACTIVE) == -1)
	{
		if (openft->klass_max & FT_NODE_SEARCH)
		{
			int nchildren = FT_CFG_MAX_CHILDREN;

			/* 2n * (n / 3) */
			nconns = (7 * nchildren) / 3;
		}
		if (nconns < 400)
			nconns = 400;

		FT->warn (FT, "guessing max_active=%d", nconns);
	}

	/* apply pragmatic clamps to the max_active setting */
	if ((fdlim = get_fd_limit ()) != -1)
	{
		if (nconns > fdlim)
		{
			FT->warn (FT, "clamping max_active to %d!", fdlim);
			nconns = fdlim;
		}
	}

	return nconns;
}

BOOL ft_conn_initial (void)
{
	int n;
	int weight = FT_CFG_INITIAL_WEIGHT;

	/* determine the maximum number of connections to allow */
	max_active = get_max_active ();

	/* we need some nodes to work with! */
	ft_node_cache_update ();

	/*
	 * We are utilizing a node-weighting system to determine how many nodes
	 * to connect to.  In general, nodes with very high costs are going to be
	 * higher quality, and we therefore want to make fewer connections.  In
	 * the long run, this is done to minimize the need to create TCP
	 * connections that will not be desired.
	 */
	n = ft_netorg_random (FT_NODE_USER, FT_NODE_DISCONNECTED, 0,
	                       FT_NETORG_FOREACH(start_connection), &weight);

	FT->DBGFN (FT, "began %i connections (remaining weight: %i)", n, weight);
	return TRUE;
}

/*****************************************************************************/

static int check_local_allow (FTNode *node)
{
    /* local hosts_allow may need to be evaluated to keep outside sources
	 * away */
    if (FT_CFG_LOCAL_MODE)
    {
		if (!net_match_host (node->ninfo.host, FT_CFG_LOCAL_ALLOW))
			return FALSE;
	}

	return TRUE;
}

BOOL ft_conn_auth (FTNode *node, int outgoing)
{
	int n;

	if (!check_local_allow (node))
	{
		FT->DBGFN (FT, "%s: not local", ft_node_fmt (node));
		return FALSE;
	}

	/* we won't authorize a duplicate connection */
	if (node->state != FT_NODE_DISCONNECTED)
	{
		FT->DBGFN (FT, "%s: dup", ft_node_fmt (node));
		return FALSE;
	}

	if (outgoing)
	{
		/* refuse outbound connections when we suspect the version may be
		 * outdated...let them come to us to prove otherwise */
		if (node->version && FT_VERSION_LT(node->version, FT_VERSION_LOCAL))
		{
			FT->DBGFN (FT, "%s: outdated outbound", ft_node_fmt (node));
			return FALSE;
		}
	}

	/* make sure we honor FT_CFG_MAX_ACTIVE, when set */
	if (max_active > 0)
	{
		n = ft_netorg_length (FT_NODE_CLASSANY, FT_NODE_CONNECTED);

		if (n >= max_active)
		{
			FT->DBGFN (FT, "%s: max_active hit", ft_node_fmt (node));
			return FALSE;
		}

		/* when we only have 100 connections left, start refusing the broken
		 * 0.2.0.0 nodes (and 0.2.0.1 out of CVS) */
		if ((node->version > 0 && node->version <= OPENFT_0_2_0_1) &&
		    (n + 100 >= max_active))
		{
			FT->DBGFN (FT, "%s: semi-outdated", ft_node_fmt (node));
			return FALSE;
		}
	}

	return TRUE;
}