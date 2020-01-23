/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
 * $Id: misc.h,v 1.13 2003/05/07 20:01:43 chnix Exp $
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
void misc_init(void (*stats_callback) (void));
void misc_destroy(void);

/* Choose another network for stats */
void stats_cycle(void);

void sharing_toggle(void);
void sharing_sync(void);

/* What text should be printed on the toggle shares button */
char *sharing_button_action(void);

#endif
