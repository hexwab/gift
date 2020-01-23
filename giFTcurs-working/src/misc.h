/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: misc.h,v 1.7 2002/09/29 16:36:27 weinholt Exp $
 */
#ifndef _MISC_H
#define _MISC_H

#include "list.h"

typedef struct {
	/* Aggregate statistics from all networks */
	unsigned int users, megs, files;
	/* Our own statistics */
	unsigned int own_files, own_megs;
} gift_stat;

extern gift_stat stats;
extern list messages;
extern int sharing;

/* begin fetch of fresh stats from giFT, and register a few more handlers. */
void misc_init(void (*stats_callback) (void));

void sharing_toggle(void);
void sharing_sync(void);

/* What text should be printed on the toggle shares button */
char *sharing_button_action(void);

#endif
