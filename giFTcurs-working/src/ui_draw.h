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
 * $Id: ui_draw.h,v 1.30 2002/06/27 23:37:00 weinholt Exp $
 */
#ifndef _UI_DRAW_H
#define _UI_DRAW_H

#include "ui_mouse.h"
#include "list.h"

#define TOUCH(t) do{if((t)->pretty) {free((t)->pretty);(t)->pretty=NULL; }} while (0)

void show_status(char *status);
void clrscr(void);
void clear_area(int x, int y, int w, int h);
void draw_list(int x, int y, int w, int h, int draw_sel, list * snafu);

/* offset is describing how far in the struct the charpointer is */
/* mkpretty is a function that generates the string if it is NULL */
void draw_list_pretty(int x, int y, int w, int h, int draw_sel, list * snafu);
void draw_box(int x, int y, int w, int h, char *title, int attr);
int draw_input(int x, int y, int w, char *header, char *value, int attr);
void draw_button(int x, int y, char *str, int attr
#ifdef MOUSE
				 , MFunc callback, void *data
#endif
	);

#ifndef MOUSE
# define draw_button(x,y,str,attr,cb,data) draw_button(x,y,str,attr)
#endif
void draw_header(int x, int y, char *header);
void draw_string(int x, int y, int w, char *header);
void draw_fmt_str(int x, int y, int w, int selected, char *str);

int draw_printf(int w, char *fmt, ...);
int draw_printfmt(int w, char *fmt, ...);

#endif
