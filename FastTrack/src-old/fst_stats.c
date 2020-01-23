/*
 * $Id: fst_stats.c,v 1.3 2003/06/26 18:34:37 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_stats.h"

/*****************************************************************************/

// giFT callback to retrieve stats
int gift_cb_stats (Protocol *p, unsigned long *users, unsigned long *files, double *size, Dataset **extra)
{
	*users = FST_PLUGIN->stats->users;
	*files = FST_PLUGIN->stats->files;
	*size  = FST_PLUGIN->stats->size;

	return 1; // number of connections
}

/*****************************************************************************/

// alloc and init stats
FSTStats *fst_stats_create ()
{
	FSTStats *stats = malloc (sizeof(FSTStats));

	stats->users = 0;
	stats->files = 0;
	stats->size = 0;

	return stats;
}

// free stats
void fst_stats_free (FSTStats *stats)
{
	free (stats);
}

// set stats
void fst_stats_set (FSTStats *stats, unsigned int users, unsigned int files, unsigned int size)
{
	stats->users = users;
	stats->files = files;
	stats->size = size;
}

// get stats
void fst_stats_get (FSTStats *stats, unsigned int *users, unsigned int *files, unsigned int *size)
{
	*users = stats->users;
	*files = stats->files;
	*size  = stats->size;
}

/*****************************************************************************/
