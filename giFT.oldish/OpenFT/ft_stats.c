/*
 * ft_stats.c
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

#include "ft_stats.h"
#include "ft_netorg.h"

/*****************************************************************************/

static Dataset *ft_stats = NULL;

/*****************************************************************************/

static FTStats *stats_new ()
{
	FTStats *stats;

	stats = malloc (sizeof (FTStats));
	memset (stats, 0, sizeof (FTStats));

	return stats;
}

static void stats_free (FTStats *stats)
{
	if (!stats)
		return;

	dataset_clear (stats->parents);
	free (stats);
}

/*****************************************************************************/

void ft_stats_add (in_addr_t parent, in_addr_t user,
                   unsigned long shares, unsigned long size)
{
	FTStats *stats;

	if (!ft_stats)
		ft_stats = dataset_new (DATASET_HASH);

	if (!(stats = dataset_lookup (ft_stats, &user, sizeof (user))))
	{
		stats = stats_new ();
		dataset_insert (&ft_stats, &user, sizeof (user), stats, 0);
	}

	if (!dataset_lookup (stats->parents, &parent, sizeof (parent)))
		dataset_insert (&stats->parents, &parent, sizeof (parent), "parent", 0);

	stats->users  = 1;
	stats->shares = shares;
	stats->size   = (double)size / 1024.0; /* MB -> GB */
}

/*****************************************************************************/

static int stats_remove (FTStats *stats, in_addr_t *parent)
{
	/* remove this parent (this may just fail to locate it... either way) */
	dataset_remove (stats->parents, parent, sizeof (*parent));

	/* more parents are left, do not get rid of this share data */
	if (dataset_length (stats->parents))
		return FALSE;

	stats_free (stats);
	return TRUE;
}

static int stats_removeh (Dataset *d, DatasetNode *node,
                          in_addr_t *parent)
{
	return stats_remove (node->value, parent);
}

void ft_stats_remove (in_addr_t parent, in_addr_t user)
{
	FTStats *stats;

	/* remove by parent (search node disconnected) */
	if (!user)
	{
		dataset_foreach (ft_stats, DATASET_FOREACH (stats_removeh), &parent);
		return;
	}

	if (!(stats = dataset_lookup (ft_stats, &user, sizeof (user))))
		return;

	/* only remove it if all search nodes agree that it should be removed */
	if (stats_remove (stats, &parent))
		dataset_remove (ft_stats, &user, sizeof (user));
}

/*****************************************************************************/

static int stats_get (Dataset *d, DatasetNode *node, FTStats *ret)
{
	FTStats *stats = node->value;

	if (stats)
	{
		ret->users  += stats->users;
		ret->shares += stats->shares;
		ret->size   += stats->size;
	}

	return FALSE;
}

/* TODO - opt...this is written in such a way that ensures correctness, but
 * is much slower than a global counter of some kind */
void ft_stats_get (unsigned long *users, unsigned long *shares,
                   double *size)
{
	FTStats stats;

	memset (&stats, 0, sizeof (stats));

	dataset_foreach (ft_stats, DATASET_FOREACH (stats_get), &stats);

	if (users)
		*users = stats.users;
	if (shares)
		*shares = stats.shares;
	if (size)
		*size = stats.size;
}

/*****************************************************************************/

struct _stats
{
	unsigned long users;
	unsigned long files;
	double        size;                /* GB */
	int           connections;
};

static Connection *gather_stats (Connection *c, Node *node,
                                 struct _stats *stats)
{
	/* favor the most positive (longest lived stats) */
	if (node->stats.users > stats->users)
		stats->users = node->stats.users;

	if (node->stats.shares > stats->files)
		stats->files = node->stats.shares;

	if (node->stats.size > stats->size)
		stats->size = node->stats.size;

	stats->connections++;

	return NULL;
}

int openft_stats (Protocol *p, unsigned long *users, unsigned long *files,
                  double *size, Dataset **extra)
{
	struct _stats stats;

	memset (&stats, 0, sizeof (stats));

	if (FT_SELF->class & NODE_INDEX)
	{
		gather_stats (OPENFT->ft, FT_SELF, &stats);
		stats.connections--;
	}

	/* gather stats from all index nodes, use the longest living */
	conn_foreach ((ConnForeachFunc)gather_stats, &stats,
	              NODE_INDEX, NODE_CONNECTED, 0);

	/* just to keep Online/Offline happy */
	stats.connections +=
		conn_length (NODE_CHILD | NODE_SEARCH, NODE_CONNECTED);

	/* fallback when INDEX nodes aren't alive yet */
	if (stats.users == 0)
		stats.users = stats.connections;

	*users = stats.users;
	*files = stats.files;
	*size  = stats.size;

	return stats.connections;
}
