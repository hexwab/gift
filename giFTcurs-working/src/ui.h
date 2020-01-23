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
 * $Id: ui.h,v 1.30 2002/09/28 11:41:27 weinholt Exp $
 */
#ifndef _UI_H
#define _UI_H

#include "list.h"
#include "screen.h"

#define PRESSED(key)		(key == KEY_ENTER || key == ' ')

typedef struct {
	void (*draw) (void);
	int (*key_handler) (int key);
} ui_methods;

extern ui_methods *active_screen;
extern int show_buttonbar;

void ui_init(void);
void ui_destroy(void);

void ui_show_messages(void);

#ifdef MOUSE
void mouse_handler(MEVENT event);
#endif
void ui_handler(int key);

int ui_list_handler(list * foo, int key, int page_len);
int ui_input_handler(char *foo, int *pos, int key, int max_len);

void ui_draw(void);

#endif
