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
 * $Id: ui_mouse.h,v 1.19 2003/07/06 14:11:01 weinholt Exp $
 */
#ifndef _UI_MOUSE
#define _UI_MOUSE

#include "screen.h"

#ifdef MOUSE
typedef void (*MFunc) (int rx, int ry, void *data);

void mouse_init(void);
void mouse_deinit(void);

/* Clear the list of areas, from level and below */
void mouse_clear(int level);
void mouse_register(int x, int y, int w, int h, unsigned int bstate, MFunc callback, void *data,
					int level);
int mouse_check(MEVENT * m);
void handle_key_mouse(int key);
#else
#define mouse_init()
#define mouse_deinit()
#define mouse_register(x, y, w, h, bstate, callback, data, level)
#define mouse_clear(level)
#endif

/* Here we hope that KEY_MAX-1 is unused. */
#define KEY_MOUSE_DRAG (KEY_MAX-1)
#define BUTTON_UP BUTTON1_RESERVED_EVENT
#define BUTTON_DOWN BUTTON2_RESERVED_EVENT

void ui_mouse_simulate_keypress(int x, int y, gpointer data);

#endif
