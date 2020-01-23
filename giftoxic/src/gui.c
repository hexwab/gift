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
#include "config.h"
#include "common.h"
#include "transfer.h"
#include "search.h"

GtkTreeStore	*stores[STORE_NUM]; /* download, upload and search treestore */

static void add_transfer_menu_entries(GtkWidget **menu, Moo *data, TransferStatus status)
{
    GtkWidget	    *menu_item;

	menu_item = gtk_menu_item_new_with_label(_("Remove item"));
	gtk_menu_shell_append(GTK_MENU_SHELL(*menu), menu_item);
		
	if (status == STATUS_COMPLETE || status == STATUS_CANCELLED)
		g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(transfer_remove_with_path), data);
	else
		gtk_widget_set_sensitive(menu_item, FALSE);

	menu_item = gtk_menu_item_new_with_label(_("Clear all"));
	gtk_menu_shell_append(GTK_MENU_SHELL(*menu), menu_item);

#if (GTK_MINOR_VERSION < 2)
	gtk_widget_set_sensitive(menu_item, FALSE);
#else
	g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(transfer_remove_all), data);
#endif

    return; 
}

void add_generic_menu_entries(GtkWidget **menu, Moo *data)
{
    GtkWidget	    *menu_item;

    menu_item = gtk_menu_item_new_with_label(_("Browse user's files"));
    gtk_menu_shell_append(GTK_MENU_SHELL(*menu), menu_item);
    g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(browse_users_files), data);
    
    return;
}

GtkWidget *create_search_menu(GtkTreePath *path)
{
    GtkWidget		*menu;
    GtkWidget		*menu_item;

    menu = gtk_menu_new();
    
    menu_item = gtk_menu_item_new_with_label(_("Download"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(file_download), path);

    return menu;
}


GtkWidget *create_upload_menu(Moo *data, GtkTreePath *path, TransferStatus status)
{
    GtkWidget		*menu;
    GtkWidget		*menu_item;

    menu = gtk_menu_new();
    
    menu_item = gtk_menu_item_new_with_label(_("Cancel"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    if (status == STATUS_COMPLETE || status == STATUS_CANCELLED)
		gtk_widget_set_sensitive(menu_item, FALSE);
    else
		g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(transfer_cancel), data);
    
    return menu;
}

GtkWidget *create_download_menu(Moo *data, GtkTreePath *path, TransferStatus status)
{
    GtkWidget		*menu;
    GtkWidget		*menu_item_sources = NULL;
    GtkWidget		*menu_item_pause;
    GtkWidget		*menu_item_resume;
    GtkWidget		*menu_item_cancel;

    /* create the menu */
    menu = gtk_menu_new();

    menu_item_sources = gtk_menu_item_new_with_label(_("Get more sources"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_sources);

    menu_item_pause = gtk_menu_item_new_with_label(_("Pause"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_pause);
		    
    menu_item_resume = gtk_menu_item_new_with_label(_("Resume"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_resume);
		    
    menu_item_cancel = gtk_menu_item_new_with_label(_("Cancel"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_cancel);
		   
    gtk_widget_set_sensitive(menu_item_pause, FALSE);
    gtk_widget_set_sensitive(menu_item_resume, FALSE);
    gtk_widget_set_sensitive(menu_item_cancel, FALSE);
    gtk_widget_set_sensitive(menu_item_sources, FALSE);
    
    if (status == STATUS_PAUSED) {
		gtk_widget_set_sensitive(menu_item_resume, TRUE);
		gtk_widget_set_sensitive(menu_item_cancel, TRUE);

		g_signal_connect(G_OBJECT(menu_item_resume), "activate", G_CALLBACK(transfer_resume), data);
		g_signal_connect(G_OBJECT(menu_item_cancel), "activate", G_CALLBACK(transfer_cancel), data);
    } else if (status == STATUS_ACTIVE) {
		gtk_widget_set_sensitive(menu_item_sources, TRUE);
		gtk_widget_set_sensitive(menu_item_pause, TRUE);
		gtk_widget_set_sensitive(menu_item_cancel, TRUE);

		g_signal_connect(G_OBJECT(menu_item_sources), "activate", G_CALLBACK(transfer_get_sources), data);
		g_signal_connect(G_OBJECT(menu_item_pause), "activate", G_CALLBACK(transfer_pause), data);
		g_signal_connect(G_OBJECT(menu_item_cancel), "activate", G_CALLBACK(transfer_cancel), data);
    }

    return menu;
}


GtkWidget *create_context_menu(GtkTreeView *treeview, GtkTreePath *path, TreeStore store)
{
	TransferStatus		status;
	Moo					*data;
	GtkWidget			*menu;
    GtkTreeModel	    *tree_model;
    GtkTreeIter		    iter;
    GtkTreeIter		    parent;
    gint				id;
	
	data = g_new0(Moo, 1);
			
	/* let's check from which GtkTreeStore this menu was opened */
	if (store == -1) {
		menu = create_search_menu(path);
		data->path = path;
	} else {
		tree_model = GTK_TREE_MODEL(stores[store]);
		gtk_tree_model_get_iter(tree_model, &iter, path);
	
		/* always use the parent if available, as the chunks and the transfer itself share the same ID */
		if (gtk_tree_model_iter_parent(tree_model, &parent, &iter))
			iter = parent;

		gtk_tree_model_get(tree_model, &iter, TSTATUS_NUM, &status, TID, &id, -1);

		data->store = store;
		data->path = path;
		data->id = id;

		if (store == STORE_DOWNLOAD) {
			menu = create_download_menu(data, path, status);
			add_transfer_menu_entries(&menu, data, status);
		} else if (store == STORE_UPLOAD) {
			menu = create_upload_menu(data, path, status);
			add_transfer_menu_entries(&menu, data, status);
		}
	}

	add_generic_menu_entries(&menu, data);

	return menu;
}


gboolean show_context_menu(GtkTreeView *treeview, GdkEventButton *event, gint store)
{
    GtkWidget		    *menu;
    GtkTreePath		    *path;
    GdkRectangle	    rectangle;
    
    if (event->button == 3) {
        if (gtk_tree_view_get_path_at_pos(treeview, event->x, event->y, &path, NULL, NULL, NULL)) {
			menu = create_context_menu(treeview, path, store);
			
			rectangle.x = event->x_root;
			rectangle.y = event->y_root;
		
			gtk_widget_show_all(GTK_WIDGET(menu));
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, &rectangle, event->button, event->time);
		}
    }

    return FALSE;
}

