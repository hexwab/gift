/*
 * menu.c
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

#include "gift-fe.h"

/* #define ITEMFACTORY_SUCKS */

/*****************************************************************************/

#include "menu.h"

#include "pref.h"

/*****************************************************************************/

/*
 * I hate GTK menus.
 */

static void menu_pref_show (GtkWidget *widget, char *page)
{
	pref_show (page);
}

/*****************************************************************************/

#include "menu-if.h"

/*****************************************************************************/

GtkWidget *menu_create_main (GtkWidget *window)
{
	GtkWidget *mbar;

#ifdef ITEMFACTORY_SUCKS
	GtkWidget *sub_menu, *mitem;

	mbar = gtk_menu_bar_new ();

	/*
	 * File submenu
	 */

	sub_menu = menu_submenu_append (mbar, "File");

	mitem = menu_append (sub_menu, "Quit");
	gtk_signal_connect (GTK_OBJECT (mitem), "activate",
						GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	/*
	 * Settings submenu
	 */

	sub_menu = menu_submenu_append (mbar, "Settings");

	mitem = menu_append (sub_menu, "General");
	gtk_signal_connect (GTK_OBJECT (mitem), "activate",
						GTK_SIGNAL_FUNC (menu_pref_show), "General");

#else /* sigh ... does too */
	GtkItemFactory *itemf;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();

	itemf = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
								  accel_group);
	gtk_item_factory_create_items (itemf, LIST_SIZE (menu_if),
								   menu_if, NULL);

	gtk_window_add_accel_group (GTK_WINDOW (window), itemf->accel_group);

	mbar = gtk_item_factory_get_widget (itemf, "<main>");
#endif /* ITEMFACTORY_SUCKS */

	return mbar;
}

/**************************************************************************/

GtkWidget *menu_submenu_append (GtkWidget *menu_bar, char *menu_text)
{
	GtkWidget *mitem, *menu;

	mitem = gtk_menu_item_new_with_label (menu_text);
	gtk_menu_bar_append (GTK_MENU_BAR (menu_bar), mitem);

	/* create the submenu */
	menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (mitem), menu);

	return menu;
}

GtkWidget *menu_append_sep (GtkWidget *menu)
{
	GtkWidget *item;

	item = gtk_menu_item_new ();
	gtk_menu_append (GTK_MENU (menu), item);

	return item;
}

GtkWidget *menu_append (GtkWidget *menu, char *menu_text)
{
	GtkWidget *mitem;

	mitem = gtk_menu_item_new_with_label (menu_text);
	gtk_menu_append (GTK_MENU (menu), mitem);

	return mitem;
}

/**************************************************************************/

int ft_handle_popup (GtkWidget *widget, GdkEventButton *event,
					 MenuCreateFunc callback)
{
	GtkCTreeNode *node;
	int row, col;
	void *data;

	gtk_clist_get_selection_info (GTK_CLIST (widget), event->x, event->y,
								  &row, &col);

	if (GTK_IS_CTREE (widget))
	{
		node = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (widget)->row_list, row));

		if (!node)
			return FALSE;

		data = gtk_ctree_node_get_row_data (GTK_CTREE (widget), node);
	}
	else if (GTK_IS_CLIST (widget))
	{
		data = gtk_clist_get_row_data (GTK_CLIST (widget), row);
	}
	else
		return FALSE;

	/* we now have data ... look for a right click */
	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
	{
		assert (data);

		/* gotcha!  create the menu ... */
		(*callback) (widget, data, event->button, event->time);

		/* ok, here is the logic behind selecting on right click...
		 *
		 * * unselect the old row in favor of this one if only one row
		 *   is selected
		 * * add the clicked row to the selection if there are multiple
		 *   selections already
		 */
		if (g_list_length (GTK_CLIST (widget)->selection) == 1)
			gtk_clist_unselect_all (GTK_CLIST (widget));

		gtk_clist_select_row (GTK_CLIST (widget), row, col);
	}

	return TRUE;
}
