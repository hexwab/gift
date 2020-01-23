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
 * $Id: gift.h,v 1.80 2003/09/08 17:47:23 saturn Exp $
 */
#ifndef _GIFT_H
#define _GIFT_H

#include "protocol.h"

typedef guint16 gift_id;
typedef int (*EventCallback) (void *param, void *udata);

int gift_emit(const char *command, void *param);

/* this needs to be called upon startup/exit */
void gift_init(void);
void gift_cleanup(void);

int gift_write(ntree **packet);

/* Starts a query with the next available id. Registers the callback to
 * listen on responses with that id. Returns the new id. */
int gift_write_register(ntree **packet, const char *command, EventCallback cb, void *udata);

void gift_register(const char *command, EventCallback, void *udata);
void gift_register_id(gift_id id, EventCallback, void *udata);

void gift_unregister(const char *command, EventCallback, void *udata);
void gift_unregister_id(gift_id id);

/* This replaces the handler for 'id' with one that simply unregisters it
   when an interface_isempty() thing comes along. */
void gift_eat_id(gift_id id);

/* Calls the ui update callback if the screen is focused.
 * The high bits can be used as flags which is passed along to the
 * update method of the screen
 * XXX This don't belong here */
void ui_update(int screen);

/* An update should not be done more often than 3 times/second even if
 * ui_update_delayed is called more often. */
void ui_update_delayed(int screen);

enum { UPDATE_ATTENTION = 1 << 31 };	/* flag to highlight the button */

#endif
