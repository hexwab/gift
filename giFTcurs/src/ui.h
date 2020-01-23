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
 * $Id: ui.h,v 1.41 2003/06/27 11:20:14 weinholt Exp $
 */
#ifndef _UI_H
#define _UI_H

#include "list.h"
#include "screen.h"

#define PRESSED(key) ((key) == KEY_ENTER || (key) == ' ')

/* The functions that all screens must implement */
typedef struct {
	void (*draw) (void);		/* redraw whole screen */
	void (*hide) (void);		/* can be NULL */
	void (*update) (int entity);	/* called via ui_update if in focus */
	int (*key_handler) (int key);	/* returns TRUE if the key was catched */
	void (*destroy) (void);
	const char *name;			/* untranslated name */
} ui_methods;

extern gboolean show_buttonbar;
extern int active_screen;		/* only for debugging! */

void ui_init(void);
void ui_destroy(void);

void ui_hilight_button(int button);

int register_screen(const ui_methods * methods);

#ifdef MOUSE
void mouse_handler(MEVENT event);
#endif
void ui_handler(int key);

int ui_list_handler(list * foo, int key, int page_len);
int ui_input_handler(GString * foo, int *pos, int key);

void ui_draw(void);

#endif
