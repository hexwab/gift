/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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
	unsigned int size;                /* GB */
} FSTStats;

/*****************************************************************************/

// giFT callback to retrieve stats
int gift_cb_stats (Protocol *p, unsigned long *users, unsigned long *files, double *size, Dataset **extra);

/*****************************************************************************/

// alloc and init stats
FSTStats *fst_stats_create ();

// free stats
void fst_stats_free (FSTStats *stats);

// set stats
void fst_stats_set (FSTStats *stats, unsigned int users, unsigned int files, unsigned int size);

// get stats
void fst_stats_get (FSTStats *stats, unsigned int *users, unsigned int *files, unsigned int *size);

/*****************************************************************************/

#endif /* __FST_STATS_H */
