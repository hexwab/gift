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
 * $Id: ui.h,v 1.34 2003/05/14 09:09:36 chnix Exp $
 */
#ifndef _UI_H
#define _UI_H

#include "list.h"
#include "screen.h"

G_INLINE_FUNC G_GNUC_CONST int PRESSED(int key)
{
	return key == KEY_ENTER || key == ' ';
}

typedef struct {
	void (*draw) (void);
	int (*key_handler) (int key);
} ui_methods;

extern const ui_methods *active_screen;
extern gboolean show_buttonbar;

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
