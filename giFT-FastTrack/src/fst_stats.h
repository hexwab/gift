/*
 * $Id: fst_stats.h,v 1.5 2004/07/08 17:58:44 mkern Exp $
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

#ifndef __FST_STATS_H
#define __FST_STATS_H

/*****************************************************************************/

typedef struct
{
	unsigned int users;               
	unsigned int files;
	unsigned int size;      /* GB */

	unsigned int sessions;	/* number of currently established sessions */
} FSTStats;

/*****************************************************************************/

// giFT callback to retrieve stats
int fst_giftcb_stats (Protocol *p, unsigned long *users, unsigned long *files,
					  double *size, Dataset **extra);

/*****************************************************************************/

// alloc and init stats
FSTStats *fst_stats_create ();

// free stats
void fst_stats_free (FSTStats *stats);

// set stats
void fst_stats_set (FSTStats *stats, unsigned int users, unsigned int files,
					unsigned int size);

// get stats
void fst_stats_get (FSTStats *stats, unsigned int *users, unsigned int *files,
					unsigned int *size);

/*****************************************************************************/

#endif /* __FST_STATS_H */
