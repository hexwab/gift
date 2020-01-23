/*
 * ft_stats.h
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

#ifndef __FT_STATS_H
#define __FT_STATS_H

/*****************************************************************************/

typedef struct
{
	Dataset      *parents;             /* parent search nodes that submitted
	                                    * this digest...when parents == NULL,
	                                    * the digest will be destroyed and
	                                    * removed.  backasswards ref
	                                    * counting. */
	unsigned long users;               /* this value depends on context of the
	                                    * structure */
	unsigned long shares;
	double        size;                /* GB */
} FTStats;

/*****************************************************************************/

void ft_stats_add    (in_addr_t parent, in_addr_t user,
                      unsigned long shares, unsigned long size);
void ft_stats_remove (in_addr_t parent, in_addr_t user);
void ft_stats_get    (unsigned long *users, unsigned long *shares,
                      double *size);

int openft_stats (Protocol *p, unsigned long *users, unsigned long *files,
                  double *size, Dataset **extra);

/*****************************************************************************/

#endif /* __FT_STATS_H */
