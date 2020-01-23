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
 * $Id: ui_draw.h,v 1.44 2003/11/16 16:15:37 weinholt Exp $
 */
#ifndef _UI_DRAW_H
#define _UI_DRAW_H

#include "ui_mouse.h"
#include "list.h"

void show_status(const char *status);
void clrscr(void);
void clear_area(int x, int y, int w, int h);
void draw_list(int x, int y, int w, int h, int draw_sel, list *snafu);

/* offset is describing how far in the struct the charpointer is */
/* mkpretty is a function that generates the string if it is NULL */
void draw_list_pretty(int x, int y, int w, int h, int draw_sel, list *snafu);
void draw_box(int x, int y, int w, int h, const char *title, int attr);
void draw_button(int x, int y, const char *str, int attr
#ifdef MOUSE
				 , MFunc callback, void *data
#endif
	);

#ifndef MOUSE
# define draw_button(x,y,str,attr,cb,data) draw_button(x,y,str,attr)
#endif
void draw_string(int x, int y, int w, const char *header);
void draw_fmt_str(int x, int y, int w, int selected, const char *str);

void draw_printfmt(int w, const char *fmt, ...);

#endif
