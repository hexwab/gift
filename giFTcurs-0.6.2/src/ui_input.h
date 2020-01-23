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
 * $Id: ui_input.h,v 1.1 2003/11/16 16:15:37 weinholt Exp $
 */
#ifndef _UI_INPUT_H
#define _UI_INPUT_H

typedef struct {
	GString *str;
	int start, pos, vpos;
	unsigned use_utf8:1;
} ui_input;

void ui_input_init(ui_input *);
void ui_input_deinit(ui_input *);
int ui_input_handler(ui_input *, int key);
void ui_input_validate(ui_input *);
void ui_input_setvpos(ui_input *, gint vpos);
void ui_input_assign(ui_input *input, const gchar * str);
void ui_input_draw(int x, int y, int w, int attr, const char *header, ui_input *, gboolean active);

#endif
