/*
 * menu.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __FE_MENU_H
#define __FE_MENU_H

/**************************************************************************/

/* generic popup creation system */
typedef int (*MenuCreateFunc) (GtkWidget *w, void *data, guint button,
							   guint32 at);

/**************************************************************************/

GtkWidget *menu_create_main		(GtkWidget *window);

GtkWidget *menu_submenu_append	(GtkWidget *menu_bar, char *menu_text);
GtkWidget *menu_append_sep		(GtkWidget *menu);
GtkWidget *menu_append			(GtkWidget *menu, char *menu_text);

int ft_handle_popup				(GtkWidget *widget, GdkEventButton *event,
								 MenuCreateFunc callback);

void menu_about_show			(GtkWidget *widget, char *page);

/**************************************************************************/

#endif /* __FE_MENU_H */
