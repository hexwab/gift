/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 G�ran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian H�ggstr�m <chm@c00.info>
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
 * $Id: ui_main.h,v 1.46 2003/05/14 09:09:36 chnix Exp $
 */
#ifndef _UI_MAIN
#define _UI_MAIN

#include "ui.h"
#include "search.h"

extern const ui_methods main_screen_methods;

void main_screen_init(void);
void main_screen_destroy(void);
void start_browse(const char *user, const char *node);

void initialize_hit(hit *);

#endif
