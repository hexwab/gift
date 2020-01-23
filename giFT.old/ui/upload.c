/*
 * upload.c
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

#include "menu.h"
#include "share.h"
#include "upload.h"

/*****************************************************************************/

Transfer *upload_insert (GtkWidget *ul_list, GData *ft_data)
{
	Transfer     *transfer;
	char         *add_id[3];
	GtkCTreeNode *new_node;
	GtkCTreeNode *insert_node;

	transfer = OBJ_NEW (Transfer);

	share_fill_data (SHARE (transfer), ft_data);
	transfer->total = SHARE (transfer)->filesize;

	/* locate placement in the upload ctree */
	insert_node = NULL;
	/* find_share_node (SHARE (transfer)->hash, NULL, ul_list, NULL); */

	add_id[0] = SHARE (transfer)->filename;
	add_id[1] = SHARE (transfer)->user;
	add_id[2] = "";

	new_node = gtk_ctree_insert_node (GTK_CTREE (ul_list), insert_node, NULL,
									  add_id, 0, NULL, NULL, NULL, NULL, 0, 0);

	obj_set_data (OBJ_DATA (transfer), "ft_app", NULL);
	obj_set_data (OBJ_DATA (transfer), "tree", ul_list);
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
		gtk_ctree_remove_node (tree, node);
		return FALSE;
	}

	/* ignore anything else */
	if (!g_datalist_get_data (&dataset, "transmit"))
		return TRUE;

	transmit = ATOI (g_datalist_get_data (&dataset, "transmit"));

	cell_text =
		calculate_transfer_info (obj->transmit, obj->total,
		                         transmit - obj->transmit);

	obj->transmit = transmit;

	gtk_ctree_node_set_text (tree, node, 2, cell_text);
	free (cell_text);

	return TRUE;
}

/*****************************************************************************/

static void menu_cancel_upload (GtkWidget *menu, Transfer *transfer)
{
	GtkCTree     *ctree;
	GtkCTreeNode *node;
	FEConnection *c;
	char         *request;

	trace ();

	assert (transfer);

	printf ("transfer->id = %lu\n", transfer->id);

	ctree = GTK_CTREE      (obj_get_data (OBJ_DATA (transfer), "tree"));
	node  = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (transfer), "node"));

	c = daemon_interface ();

	request = g_strdup_printf ("<transfer id=\"%lu\" action=\"cancel\"/>\n",
	                           transfer->id);
	net_send (c->fd, request);
	free (request);

	daemon_event_remove (transfer->id);
	gtk_ctree_remove_node (ctree, node);
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

	/* you big bastard, thats not nice */
	item = menu_append (menu, "Cancel Upload");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
						GTK_SIGNAL_FUNC (menu_cancel_upload), upload);

	gtk_widget_show_all (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, at);

	return TRUE;
}

/*****************************************************************************/

