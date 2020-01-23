/*
 * fe_upload.c
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
#include "fe_ui_utils.h"
#include "fe_menu.h"
#include "fe_share.h"
#include "fe_upload.h"

#include "conf.h"
extern Config *gift_fe_conf;

/*****************************************************************************/

Transfer *upload_insert (GtkWidget *ul_list, GData *ft_data)
{
	Transfer     *transfer;
	char         *add_id[3];
	GtkCTreeNode *new_node;
	GtkCTreeNode *insert_node;
	GdkPixmap *nodepixmap;
	GdkBitmap *nodebitmap;

	transfer = OBJ_NEW (Transfer);

 	transfer->transfer_rate = NULL;

	share_fill_data (SHARE (transfer), ft_data);
	transfer->total = SHARE (transfer)->filesize;

	/* locate placement in the upload ctree */
	insert_node = NULL;
	/* find_share_node (SHARE (transfer)->hash, NULL, ul_list, NULL); */

	add_id[0] = SHARE (transfer)->filename;
	add_id[1] = SHARE (transfer)->user;
	add_id[2] = "";

	obj_set_data (OBJ_DATA (transfer), "tree", ul_list);

	transfer_get_pixmap (transfer, &nodepixmap, &nodebitmap);
	new_node = gtk_ctree_insert_node (GTK_CTREE (ul_list), insert_node, NULL,
	                                  add_id, 5, nodepixmap, nodebitmap,
	                                  nodepixmap, nodebitmap, 0, 0);

	obj_set_data (OBJ_DATA (transfer), "ft_app", NULL);
	obj_set_data (OBJ_DATA (transfer), "node", new_node);

	SET_NODE_DATA (ul_list, new_node, transfer);

	return transfer;
}

/*****************************************************************************/

int upload_response (char *head, int keys, GData *dataset, DaemonEvent *event)
{
	Transfer      *obj;
	GtkCTree      *tree;
	GtkCTreeNode  *node;
	char          *cell_text;
	unsigned long  transmit;

	if (!event || !event->obj)
		return FALSE;

	obj = event->obj;

	tree = GTK_CTREE      (obj_get_data (OBJ_DATA (obj), "tree"));
	node = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (obj), "node"));

	if (!tree || !node)
		return FALSE;

	if (!head || !dataset)
	{
		/* Event is beiing removed clean UI up */
		if (config_get_int (gift_fe_conf, "general/autormul=0") ||
		                    obj->active==TRANSFER_CANCELLED)
		{
			gtk_ctree_remove_node (tree, node);
		}
		else
			gtk_ctree_node_set_text (tree, node, 2, "Finished");
		obj->active = TRANSFER_FINISHED;
		return FALSE;
	}

	/* ignore anything else */
	if (!g_datalist_get_data (&dataset, "transmit"))
		return TRUE;

	transmit = ATOI (g_datalist_get_data (&dataset, "transmit"));

	if (!obj->transmit)
		obj->transmit = transmit;

	obj->transfer_rate =
		g_list_append (obj->transfer_rate,
					   (void *) (transmit - obj->transmit));
	if (g_list_length (obj->transfer_rate) > TR_SAMPLES)
		obj->transfer_rate =
			g_list_remove (obj->transfer_rate,
						   g_list_first (obj->transfer_rate)->data);

	cell_text =
	    calculate_transfer_info (obj->transmit, obj->total,
								 transfer_get_rate (obj));

	obj->transmit = transmit;
	gtk_ctree_node_set_text (tree, node, 2, cell_text);
	g_free (cell_text);

	return TRUE;
}

/*****************************************************************************/

static void menu_cancel_upload_it (Transfer *item)
{
	if (item->active != TRANSFER_FINISHED)
	{
		item->active = TRANSFER_CANCELLED;
		daemon_send ("<transfer id=\"%lu\" action=\"cancel\"/>\n", item->id);
		/* the daemon emits a <transfer id=XX/> and the underlying code
		 * in daemon_handle_packet should take care of this:
		 * daemon_event_remove (item->id, "upload"); */
	}
	else
	{
		GtkCTree *tree;
		GtkCTreeNode *node;

		tree = GTK_CTREE      (obj_get_data (OBJ_DATA (item), "tree"));
		node = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (item), "node"));
		gtk_ctree_remove_node (tree, node);
	}
}

static void menu_cancel_upload (GtkWidget *menu, Transfer *transfer)
{
	GtkCTree     *ctree;
	ctree = GTK_CTREE (obj_get_data (OBJ_DATA (transfer), "tree"));
	with_ctree_selection (ctree, menu_cancel_upload_it);
}

/*****************************************************************************/

static void menu_clear_upload_it (Transfer *item)
{
	if (item->active != TRANSFER_FINISHED)
	{
		gift_fe_debug ("*** transfer of '%s' is not finished!\n",
		               SHARE (item)->filename);
	}
	else
		menu_cancel_upload_it (item);
}

static void menu_clear_upload (GtkWidget *menu, Transfer *transfer)
{
	GtkCTree *ctree;

	ctree = GTK_CTREE (obj_get_data (OBJ_DATA (transfer), "tree"));
	with_ctree_selection (ctree, menu_clear_upload_it);
}

/*****************************************************************************/

int menu_popup_upload (GtkWidget *ul_list, Transfer *upload, guint button,
					   guint32 at)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new ();

	item = menu_append (menu, "More files from this user...");

	menu_append_sep (menu);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
						GTK_SIGNAL_FUNC (menu_search_user), upload);

	/* you big bastard, thats not nice */
	item = menu_append (menu, "Cancel Upload");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
						GTK_SIGNAL_FUNC (menu_cancel_upload), upload);

	item = menu_append (menu, "Clear if Finished");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
	                    GTK_SIGNAL_FUNC (menu_clear_upload), upload);	

	gtk_widget_show_all (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, at);

	return TRUE;
}

/*****************************************************************************/


void menu_search_user (GtkWidget *menu, Transfer *transfer) {
	GtkCTree     *ctree;
	GtkCTreeNode *node;
	FEConnection *c;
	char         *user;

	trace ();

	assert (transfer);

	gift_fe_debug ("transfer->id = %lu\n", transfer->id);
	c     = daemon_interface ();
	ctree = GTK_CTREE      (obj_get_data (OBJ_DATA (transfer), "tree"));
	node  = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (transfer), "node"));
	user  = SHARE (transfer)->user;

	assert (user);
	search_execute(c->data, user, "", "user");
}
