/*
 * $Id: ft_stats.h,v 1.8 2003/06/24 19:57:20 jasta Exp $
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

#ifndef __FT_STATS_H
#define __FT_STATS_H

/*****************************************************************************/

/**
 * Simple interface structure to reduce calling arguments and generally
 * confuse you.  Yes, that's right...I'm talking to YOU.
 */
typedef struct
{
	unsigned int users;                /**< number of users used to collect
	                                    *   the described stats */
	unsigned int shares;               /**< number of files shared */
	double       size;                 /**< volume of shares, in gigabytes */
} ft_stats_t;

/*****************************************************************************/

void ft_stats_insert (in_addr_t depend, in_addr_t user, ft_stats_t *stats);
void ft_stats_remove (in_addr_t depend, in_addr_t user);
void ft_stats_remove_dep (in_addr_t depend);
BOOL ft_stats_collect (ft_stats_t *collect);

/*****************************************************************************/

int openft_stats (Protocol *p, unsigned long *users, unsigned long *files,
                  double *size, Dataset **extra);

/*****************************************************************************/

#endif /* __FT_STATS_H */
