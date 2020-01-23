/*
 * $Id: gt_stats.h,v 1.3 2003/07/21 16:48:07 hipnod Exp $
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

#ifndef __GT_STATS_H__
#define __GT_STATS_H__

void gt_stats_accumulate (in_addr_t ipv4, in_port_t port,
                          in_addr_t src_ip, uint32_t files,
                          uint32_t size_kb);

int gnutella_stats (Protocol *p, unsigned long *users, unsigned long *files,
                    double *size, Dataset **extra);

#endif /* __GT_STATS_H__ */
