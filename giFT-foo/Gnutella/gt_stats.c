/*
 * $Id: gt_stats.c,v 1.7 2003/06/01 09:34:48 hipnod Exp $
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
#include "gt_netorg.h"

struct gt_stats
{
	double        size_kb;
	unsigned long files;
	size_t        users;
};

static TCPC *count_stats (TCPC *c, GtNode *node, struct gt_stats *st)
{
	st->size_kb += node->size_kb;
	st->files   += node->files;

	if (st->size_kb > 0 || st->files > 0)
		st->users++;

	return NULL;
}

int gnutella_stats (Protocol *p, unsigned long *users, unsigned long *files,
                    double *size, Dataset **extra)
{
	struct gt_stats stats;

	memset (&stats, 0, sizeof (struct gt_stats));

#if 0
	/* count the total files and their sizes among connected nodes */
	gt_conn_foreach ((ConnForeachFunc) count_stats, &stats, GT_NODE_NONE,
	                 GT_NODE_CONNECTED, 0);
#else
	/* hmm, try this */
	gt_conn_foreach ((ConnForeachFunc) count_stats, &stats, GT_NODE_NONE,
	                 -1, 0);
#endif

#if 0
	/*
	 * Make users be the number of search connections for now.
	 */
	*users = gt_conn_length (GT_NODE_ULTRA, GT_NODE_CONNECTED);
#else
	*users = stats.users;
#endif

	*files = stats.files;
	*size  = stats.size_kb / 1024 / 1024;

	return TRUE;
}
