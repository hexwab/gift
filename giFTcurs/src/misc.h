/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: misc.h,v 1.16 2003/06/27 11:20:14 weinholt Exp $
 */
#ifndef _MISC_H
#define _MISC_H

#include "list.h"

typedef struct {
	/* Gather statistics from this network, or all if set to NULL */
	char *network;
	guint64 bytes, own_bytes;
	unsigned int users, files;
	/* Our own statistics */
	unsigned int own_files;
} gift_stat;

extern gift_stat stats;
extern list messages;
extern int sharing;

/* begin fetch of fresh stats from giFT, and register a few more handlers. */
void misc_init(void);
void misc_destroy(void);

/* The given callback wants to be notified when the network status changes */
void request_stats(int callback);

/* The given callback wants to be notified of sharing status changes */
void share_register(int callback);

/* Choose another network for stats */
void stats_cycle(int callback);

void sharing_toggle(void);
void sharing_sync(void);

/* What text should be printed on the toggle shares button */
char *sharing_button_action(void);

#endif
