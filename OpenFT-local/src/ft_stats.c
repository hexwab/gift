/*
 * $Id: ft_stats.c,v 1.17 2004/02/03 01:18:44 jasta Exp $
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

#include "ft_stats.h"                  /* not proto/ft_stats.h */

/*****************************************************************************/

/*
 * The structure of this dataset is as follows (in perl):
 *
 * my $stats =
 * {
 *     parent_host => { child_host => { shares => 500,
 *                                      size   => 100 },
 *                      next_child => { ... } },
 *     next_parent => { ... }
 * };
 *
 * Note that multiple children can exist here under different parent
 * management.  Currently the OpenFT network frowns upon this, but it is not
 * technically illegal to do.  If the behaviour becomes popular after
 * release, users will be punished.
 *
 * Also note that this code is used only by INDEX nodes currently, who are
 * the only nodes that hold large stats databases for querying.
 */
static Dataset *stats_db = NULL;          /* by_parent */

/*****************************************************************************/

void ft_stats_insert (in_addr_t depend, in_addr_t user, ft_stats_t *stats)
{
	Dataset *by_user;

	/* dont abuse the shitty interface :) */
	assert (stats->users == 1);

	if (!(by_user = dataset_lookup (stats_db, &depend, sizeof (depend))))
	{
		if (!(by_user = dataset_new (DATASET_HASH)))
			return;

		dataset_insert (&stats_db, &depend, sizeof (depend), by_user, 0);
	}

	/* copy the contents of the stats structure into the dataset as the
	 * pointer we were given is local to ft_stats_digest_add */
	dataset_insert (&by_user, &user, sizeof (user), stats, sizeof (ft_stats_t));
}

void ft_stats_remove (in_addr_t depend, in_addr_t user)
{
	Dataset *by_user;

	if (!(by_user = dataset_lookup (stats_db, &depend, sizeof (depend))))
		return;

	dataset_remove (by_user, &user, sizeof (user));
}

void ft_stats_remove_dep (in_addr_t depend)
{
	DatasetNode *node;

	if (!(node = dataset_lookup_node (stats_db, &depend, sizeof (depend))))
		return;

	/* destroy the dataset held within */
	dataset_clear (node->value->data);

	/* now remove the node that references the destroyed dataset */
	dataset_remove_node (stats_db, node);
}

static void stats_collect_by_user (ds_data_t *key, ds_data_t *value,
                                   ft_stats_t *collect)
{
	ft_stats_t *stats = value->data;

	collect->users  += 1;
	collect->shares += stats->shares;
	collect->size   += stats->size;
}

static void stats_collect (ds_data_t *key, ds_data_t *value,
                           ft_stats_t *collect)
{
	Dataset *by_user = value->data;

	dataset_foreach (by_user, DS_FOREACH(stats_collect_by_user), collect);
}

BOOL ft_stats_collect (ft_stats_t *collect)
{
	if (!collect)
		return FALSE;

	collect->users  = 0;
	collect->shares = 0;
	collect->size   = 0.0;

	dataset_foreach (stats_db, DS_FOREACH(stats_collect), collect);

	return TRUE;
}

/*****************************************************************************/

static BOOL gather_stats (FTNode *node, ft_stats_t *stats)
{
	ft_stats_t *sess_stats;

	if (!node || !node->session)
		return FALSE;

	sess_stats = &node->session->stats;

	if (sess_stats->users == 0)
		return FALSE;

	stats->users  += sess_stats->users;
	stats->shares += sess_stats->shares;
	stats->size   += sess_stats->size;

	return TRUE;
}

int openft_stats (Protocol *p,
                  unsigned long *users, unsigned long *files,
                  double *size, Dataset **extra)
{
	ft_stats_t stats;
	int        n = 0;
	int        conns;

	memset (&stats, 0, sizeof (stats));

#if 0
	if (openft->klass & FT_NODE_INDEX)
		n += (int)(gather_stats (FT_SELF, &stats));
#endif

	/* gather stats from all users that can store the stats structure */
	n += ft_netorg_foreach (FT_NODE_SEARCH | FT_NODE_INDEX, FT_NODE_CONNECTED, 0,
	                        FT_NETORG_FOREACH(gather_stats), &stats);

	/* establish the total number of connections for the return value */
	conns = ft_netorg_length (FT_NODE_USER, FT_NODE_CONNECTED);

	/* adjust the stats for an average of all connected users (yup, this is
	 * crap alright...if you'd like to improve this, be my guest) */
	if (n > 1)
	{
		stats.users  /= n;
		stats.shares /= n;
		stats.size   /= (float)n;
	}

	/* if nobody is around to provide stats, lets try to help the user out
	 * by displaying the total number of connections as users */
	if (stats.users == 0)
		stats.users = conns;

	/* pass back to giFT */
	*users = stats.users;
	*files = stats.shares;
	*size  = stats.size;

	return conns;
}
