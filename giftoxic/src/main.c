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
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <config.h>

#include "gtkcellrendererprogress.h"
#include "common.h"
#include "main.h"
#include "utils.h"
#include "parse.h"
#include "stats.h"
#include "io.h"
#include "transfer.h"
#include "event_ids.h"
#include "search.h"
#include "search_tab.h"

Options		    *options;
GtkWidget		*toggle_button_sharing;
GtkWidget		*toggle_button_autoclean[2];
gint			running_searches;
gboolean        gift_started = FALSE;

void set_autoclean_status(AutoClean autoclean)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_autoclean[0]), autoclean & AUTO_CLEAN_CANCELLED);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_autoclean[1]), autoclean & AUTO_CLEAN_COMPLETED);
}

void set_sharing_status(gboolean sharing)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_sharing), sharing);
}


static GtkWidget *create_transfer_tab()
{
    GtkWidget		    *transfer_tab;
    GtkWidget		    *frame;
    GtkWidget		    *scrolled;
    GtkWidget		    *tree;
	GtkWidget			*label;
    GtkCellRenderer	    *renderer;
    GtkTreeViewColumn	*column;
    GtkTreeSelection	*select;
    GtkTreeModel        *tree_model;
    gchar				*frame_captions[2] = { N_("Downloads") ,  N_("Uploads")};
    gchar				*column_header[TN_COLS] = {N_("Filename"), N_("User"), N_("Status"), N_("Progress"), N_("Size"), N_("Speed"), N_("ETA"), "transmitted", "hash", "id", "size", "tracked size", "tracked time", "status", "updated", "user unique"};
    gint				i;
    gint				j;
	gchar				buf[100];

    transfer_tab = gtk_vpaned_new(); 
    
    for (i = 0; i < 2; i++) {
		frame = gtk_frame_new(NULL);
		gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
		gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

		sprintf(buf,"<span weight=\"bold\">%s</span>", _(frame_captions[i]));
		label = gtk_label_new(buf);
		gtk_frame_set_label_widget(GTK_FRAME(frame), label);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	
		scrolled = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(scrolled), 10);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
		stores[i] = gtk_tree_store_new(TN_COLS,
				G_TYPE_STRING, /* filename */
				G_TYPE_STRING, /* user, only alias */
				G_TYPE_STRING, /* status */
				G_TYPE_FLOAT, /* progress */
				G_TYPE_STRING, /* filesize resp. human readable */
				G_TYPE_STRING, /* current speed */
				G_TYPE_STRING, /* time remaining */
				G_TYPE_ULONG, /* bytes transmitted */
				G_TYPE_STRING, /* hash */
				G_TYPE_INT, /* transfer id */
				G_TYPE_ULONG, /* filesize resp. chunksize */
				G_TYPE_ULONG, /* tracked size */
				G_TYPE_ULONG, /* tracked time */
				G_TYPE_INT, /* TransferStatus */
				G_TYPE_INT, /* throughput updated or not */
				G_TYPE_STRING /* unique username@ip */
				);
		
		tree_model = GTK_TREE_MODEL(stores[i]);
		tree = gtk_tree_view_new_with_model(tree_model);

		/* create columns */
		for (j = 0; j < TN_COLS; j++) {
			if (j == TPROGRESS) {
				renderer = gtk_cell_renderer_progress_new();
				column = gtk_tree_view_column_new_with_attributes(_(column_header[j]), renderer, "value", j, NULL);
		
				g_object_set(G_OBJECT(renderer), "low-value", 0.0, NULL);
				g_object_set(G_OBJECT(renderer), "high-value", 100.0, NULL);
				g_object_set(G_OBJECT(renderer), "low-color", "white", NULL);
				g_object_set(G_OBJECT(renderer), "med-color", "lightblue", NULL);
				g_object_set(G_OBJECT(renderer), "high-color", "white", NULL);
				g_object_set(G_OBJECT(renderer), "text-color", "black", NULL);
			} else {
				renderer = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(_(column_header[j]), renderer, "text", j, NULL);
			}

			if (j == TTRANS || j == THASH || j == TSTATUS_NUM || j == TID || j == TSIZE || j == TUPDATED 
				 || j == TTRACKED_SIZE || j == TTRACKED_TIME || j > TETA) 
				gtk_tree_view_column_set_visible(column, FALSE);
			
			gtk_tree_view_column_set_sort_column_id(column, j);
			gtk_tree_view_column_set_resizable(column, TRUE);
		
			if (j == TNAME) {
				gtk_tree_view_column_set_fixed_width(column, COL_NAME_WIDTH);
				gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
			}
		
			if (j==TSIZEHUMAN){
				gtk_tree_view_column_set_sort_column_id(column,TSIZE);
			}
			gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
		}

		select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
        gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	
		g_signal_connect(G_OBJECT(tree), "button_press_event", G_CALLBACK(show_context_menu), (gpointer) i);
   
		gtk_container_add(GTK_CONTAINER(scrolled), tree);
		gtk_container_add(GTK_CONTAINER(frame), scrolled);

		if (i) 
			gtk_paned_pack2(GTK_PANED(transfer_tab), frame, TRUE, FALSE);
		else
			gtk_paned_pack1(GTK_PANED(transfer_tab), frame, TRUE, FALSE);
	}

    return transfer_tab;
}


static void autoclean_button_toggled(GtkToggleButton *button, gpointer num)
{
	if (gtk_toggle_button_get_active(button))
		options->autoclean |= GPOINTER_TO_INT(num) + 1;
	else
		options->autoclean &= 14 - GPOINTER_TO_INT(num);
			
	return;
}

static void sharing_button_toggled(GtkToggleButton *button, gpointer data)
{
	gchar		*actions[2] = {"hide", "show"};
	
	options->sharing = gtk_toggle_button_get_active(button);

	gift_send(2, "SHARE", NULL, "action", actions[options->sharing]);
	
	return;
}

static void sync_shares(GtkButton *button, gpointer data)
{
	gift_send(2, "SHARE", NULL, "action", "sync");
}

static GtkWidget *create_preferences_tab()
{
	GtkWidget			*pref_tab;
	GtkWidget			*frame;
	GtkWidget			*table;
	GtkWidget			*label;
	GtkWidget			*button;
	GtkWidget			*vbox;
	GtkWidget			*hbox;
	gchar				*captions[4] = {N_("Automatically remove"), " ", N_("Cancelled transfers"), N_("Completed transfers")};
	gint				i;
	gchar				buf[100];

	pref_tab = gtk_vbox_new(FALSE, 5);

	/* autoclean frame */
	frame = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(pref_tab), frame, FALSE, FALSE, 5);

	sprintf(buf, "<span weight=\"bold\">%s</span>", _("Autoclean options"));
	label = gtk_label_new(buf);
	gtk_frame_set_label_widget(GTK_FRAME(frame), label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	
	table = gtk_table_new(2, 2, TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 5);
	
	for (i = 0; i < 2; i++) {
		label = gtk_label_new(_(captions[i]));
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, i, i + 1);

		toggle_button_autoclean[i] = gtk_check_button_new_with_label(_(captions[i + 2]));
		g_signal_connect(G_OBJECT(toggle_button_autoclean[i]), "toggled", G_CALLBACK(autoclean_button_toggled), GINT_TO_POINTER(i));
		gtk_table_attach_defaults(GTK_TABLE(table), toggle_button_autoclean[i], 1, 2, i, i + 1);
	}
	
	/* shares frame */
	frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(pref_tab), frame, FALSE, FALSE, 5);
	
	sprintf(buf, "<span weight=\"bold\">%s</span>", _("Shares"));
	label = gtk_label_new(buf);
	gtk_frame_set_label_widget(GTK_FRAME(frame), label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
 	
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	
	toggle_button_sharing = gtk_check_button_new_with_label(_("Enable sharing"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_sharing), FALSE);
	g_signal_connect(G_OBJECT(toggle_button_sharing), "toggled", G_CALLBACK(sharing_button_toggled), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), toggle_button_sharing, FALSE, FALSE, 5);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	
	button = gtk_button_new_with_label(_("Synchronize shares"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(sync_shares), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	return pref_tab;
}

static void clean_up()
{
	stop_search();
	
	if (gift_started)
		gift_send(1, "QUIT", NULL);
	else
		gift_send(1, "DETACH", NULL);

	if (options) {
		save_settings(options);
		g_free(options->target_host);
		g_free(options);
	}
}

static gboolean delete(GtkWidget *widget, GtkWidget *event, gpointer data)
{
	clean_up();
    gtk_main_quit();

    return FALSE;
}

static GtkWidget *create_img_label(gchar *stock_item, gchar *label)
{
	GtkWidget	*img_label;
	GtkWidget	*image;

	img_label = gtk_hbox_new(FALSE, 5);
	
    image = gtk_image_new_from_stock(stock_item, GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(img_label), image, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(img_label), gtk_label_new(label), FALSE, FALSE, 0);
    gtk_widget_show_all(GTK_WIDGET(img_label));

	return img_label;
}

static void create_gui()
{
	GtkWidget    *window;
    GtkWidget    *vbox;
    GtkWidget    *notebook;
	GdkPixbuf    *ico;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 480);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete), NULL);
    
	ico = gdk_pixbuf_new_from_file(PIXMAPDIR "/giFToxic-48.png", NULL);
	gtk_window_set_icon(GTK_WINDOW(window), ico);
	
    vbox = gtk_vbox_new(FALSE, 0);
    
	/* create notebook */
    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_container_set_border_width (GTK_CONTAINER (notebook), 10);

	/* create home tab */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_home_tab(), NULL);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook),
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0),
			create_img_label(GTK_STOCK_HOME, _("Home")));

	/* create search tab */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_search_tab(), NULL);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook),
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1),
			create_img_label(GTK_STOCK_FIND, _("Search")));

	/* create transfer tab */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_transfer_tab(), NULL);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook),
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 2),
			create_img_label(GTK_STOCK_REFRESH, _("Transfers")));

	/* create preferences tab */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), create_preferences_tab(), NULL);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook),
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 3),
			create_img_label(GTK_STOCK_PREFERENCES, _("Preferences")));

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	/* create statusbars */
	gtk_box_pack_start(GTK_BOX(vbox), create_statusbar(), FALSE, FALSE, 0);
 
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(GTK_WIDGET(window));

	return;
}

void io_handler_init()
{
	io_watch_create();
	
    gift_send(3, "ATTACH", NULL, "client", "giFToxic", "version", VERSION);
	gift_send(2, "SHARE", NULL, "action", NULL);
	
	return;
}

gint main(gint argc, gchar *argv[])
{
    gtk_init(&argc, &argv);
	
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	
	create_gui();

    options = read_settings();
    parse_parameters(argc, argv, &options);

	if (!options->target_host || !options->target_port)
		show_dialog(_("You need to specify a host and port to connect to!"), GTK_MESSAGE_ERROR);
	
	apply_settings(options);
	
	running_searches = 0;

    if (!daemon_connection_init(options->target_host, options->target_port)) {
		if (is_local_host(options->target_host)) {
			/* giFT cannot be connected to, so we are going to start giFT and try
		     * to connect again. In this case, the giFT daemon will be shut down
		     * at exit.
		     */
		
			if (!fork()) {
				execl("/bin/sh", "/bin/sh", "-c", "giFT", "giFT", "-d", NULL);
				exit(0);
			} else {
				sleep(1);

				if (!daemon_connection_init("localhost", options->target_port)) {
					show_dialog(_("The giFT deamon could not be started!"), GTK_MESSAGE_ERROR);
					return TRUE;
				} else
					gift_started = TRUE;
			}
		} else
			show_dialog(_("Couldn't connect to giFT daemon on remote system!"), GTK_MESSAGE_ERROR);
	}

	io_handler_init();
	g_timeout_add(3000, (GSourceFunc) reset_speed_cols, NULL);

    gtk_main();
 
    return FALSE;
}

