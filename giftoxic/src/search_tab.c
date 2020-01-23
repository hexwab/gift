/* giFToxic, a GTK2 based GUI for giFT
 * Copyright (C) 2002, 2003 giFToxic team (see AUTHORS)
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
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include "config.h"
#include "common.h"
#include "search.h"
#include "transfer.h"

SearchMode search_mode = SEARCH_MODE_ALL;
GtkWidget  *stop_button = NULL;

gboolean option_menu_changed(GtkWidget *menu, gpointer data)
{
	gint *value = data;
    
	*value = gtk_option_menu_get_history(GTK_OPTION_MENU(menu));
	
	/* this is because of the menu separator: */
	if (*value > 5)
		(*value)--;

    return FALSE;
}

void gui_enable_stop(gboolean enabled)
{
	gtk_widget_set_sensitive(GTK_WIDGET(stop_button), enabled);
}

gboolean gui_start_search(GtkWidget *button, gpointer entry)
{
	gui_enable_stop(TRUE);
	start_search(entry, search_mode);

	return FALSE;
}

gboolean gui_stop_search(GtkWidget *button, gpointer entry)
{
	stop_search();
	gui_enable_stop(FALSE);

	return FALSE;
}

gboolean search_entry_key_press(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_Return)
		gui_start_search(NULL, entry);

    return FALSE;
}

GtkWidget *create_search_tab()
{
    GtkWidget		    *search_tab;
    GtkWidget		    *results_frame;
    GtkWidget		    *hbox;
    GtkWidget		    *label;
    GtkWidget		    *entry;
    GtkWidget		    *button;
    GtkWidget		    *scrolled;
    GtkWidget		    *tree;
    GtkWidget		    *option_menu;
    GtkWidget		    *menu;
    GtkWidget		    *menu_item;
	GtkWidget			*frame;
    GtkCellRenderer	    *renderer;
    GtkTreeModel	    *tree_model;
    GtkTreeViewColumn	*column;
    GtkTreeSelection	*select;
    gchar				*column_header[SN_COLS] = {"root", N_("Sources"), N_("Filename"), N_("Size"), N_("User"), N_("Avail."), "user unique", "size", "hash", "url", "mime", "bitrate"};
    gchar				*menu_entries[] = {N_("All Files"), N_("Audio"), N_("Video"), N_("Images"), N_("Documents"), N_("Software"), N_("User"), N_("Checksum")};
    gint				i;
	gchar				buf[100];
    
    search_tab = gtk_vbox_new(FALSE, 5);

	/* "Query" label */
	frame = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	
	snprintf(buf, sizeof(buf), "<span weight=\"bold\">%s</span>", _("Query")); 
	label = gtk_label_new(buf);
	gtk_frame_set_label_widget(GTK_FRAME(frame), label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    
	/* entry for search string */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
  
    entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 256);
	
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(entry), "key-press-event", G_CALLBACK(search_entry_key_press), NULL);
    
	/* option menus for search mode */
	option_menu = gtk_option_menu_new();
    menu = gtk_menu_new();
    
	for (i = 0; i < 6; i++)
		if (menu_entries[i]) {
			menu_item = gtk_menu_item_new_with_label(_(menu_entries[i]));
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		}
	
	menu_item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	
	for (i = 6; i < 8; i++)
		if (menu_entries[i]) {
			menu_item = gtk_menu_item_new_with_label(_(menu_entries[i]));
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
		}
    
	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), 0);

	g_signal_connect(G_OBJECT(option_menu), "changed", G_CALLBACK(option_menu_changed), &search_mode);
	gtk_box_pack_start(GTK_BOX(hbox), option_menu, FALSE, FALSE, 5);

	/* search button */
    button = gtk_button_new_from_stock(GTK_STOCK_FIND);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(gui_start_search), (gpointer) entry);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);

    stop_button = gtk_button_new_from_stock(GTK_STOCK_STOP);
	gtk_widget_set_sensitive(GTK_WIDGET(stop_button), FALSE);
    g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(gui_stop_search), (gpointer) entry);
    gtk_box_pack_start(GTK_BOX(hbox), stop_button, FALSE, FALSE, 5);
	
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_box_pack_start(GTK_BOX(search_tab), frame, FALSE, FALSE, 5);
	
	
	/* search results frame */
    results_frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(results_frame), 5);

	gtk_frame_set_shadow_type(GTK_FRAME(results_frame), GTK_SHADOW_NONE);
	snprintf(buf, sizeof(buf), "<span weight=\"bold\">%s</span>", _("Search Results")); 
	label = gtk_label_new(buf);
	gtk_frame_set_label_widget(GTK_FRAME(results_frame), label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	
    scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled), 10);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    stores[STORE_SEARCH] = gtk_tree_store_new(SN_COLS,
			G_TYPE_BOOLEAN, /* top level item? */
			G_TYPE_INT, /* number of sources */
			G_TYPE_STRING, /* filename */
			G_TYPE_STRING, /* size, human readable */
			G_TYPE_STRING, /* user */
			G_TYPE_BOOLEAN, /* available? */
			G_TYPE_STRING, /* alias@ip */
			G_TYPE_ULONG, /* size */
			G_TYPE_STRING, /* hash */
			G_TYPE_STRING, /* url */
			G_TYPE_STRING, /* mime type */
			G_TYPE_STRING /* bitrate */
			);

    tree_model = GTK_TREE_MODEL(stores[STORE_SEARCH]);
    tree = gtk_tree_view_new_with_model(tree_model);

/* create columns */
    for (i = 0; i < SN_COLS; i++) {
		if (i == SROOT || i == SAVAIL) {
			renderer = gtk_cell_renderer_toggle_new();
			column = gtk_tree_view_column_new_with_attributes(_(column_header[i]), renderer, "active", i, NULL);
		} else {
			renderer = gtk_cell_renderer_text_new();
			column = gtk_tree_view_column_new_with_attributes(_(column_header[i]), renderer, "text", i, NULL);
		}

		if (i == SROOT || i > SAVAIL)
			gtk_tree_view_column_set_visible(column, FALSE);

		if (i == SSOURCES)
			gtk_tree_view_column_add_attribute(column, renderer, "visible", SROOT);

		if (i == SNAME) {
			gtk_tree_view_column_set_fixed_width(column, COL_NAME_WIDTH);
			gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
		}
		
		gtk_tree_view_column_set_sort_column_id(column, i);
		gtk_tree_view_column_set_resizable(column, TRUE);
		if (i == SSIZEHUMAN) {
			gtk_tree_view_column_set_sort_column_id(column,SSIZE);
		}
		gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }
    
    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(tree), "button_press_event", G_CALLBACK(show_context_menu), (gpointer) -1);

    gtk_container_add(GTK_CONTAINER(scrolled), tree);
    gtk_container_add(GTK_CONTAINER(results_frame), scrolled);
    
    gtk_box_pack_start(GTK_BOX(search_tab), results_frame, TRUE, TRUE, 5);

    return search_tab;
}


