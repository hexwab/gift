/*
 * fe_download.c
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

#include "fe_menu.h"
#include "fe_share.h"
#include "fe_download.h"
#include "fe_daemon.h"
#include "fe_search.h"
#include "fe_ui_utils.h"

#include "conf.h"
extern Config *gift_fe_conf;

/*****************************************************************************/

/* download stats interval (in seconds) */
#define STATS_INTERVAL 1

/*****************************************************************************/

Transfer *download_insert (GtkWidget *dl_list, GtkCTreeNode *node_hint,
                           SharedFile *shr)
{
	Transfer     *transfer;
	Transfer     *parent;
	char         *add_id[3];
	GtkCTreeNode *new_node;
	GtkCTreeNode *insert_node;
	GdkPixmap *nodepixmap;
	GdkBitmap *nodebitmap;

	/* insert into the tree with the appropriate outsourced organization */
	insert_node = (node_hint ? node_hint :
	               find_share_node (shr->hash, NULL, 0, dl_list, NULL));

	if (insert_node != NULL)
	{
		Transfer *dup_check;
		GtkCTreeNode *cnode;

		if (shr->user == NULL)
			return NULL;

		for (cnode = GTK_CTREE_ROW (insert_node)->children; cnode;
			 cnode = GTK_CTREE_ROW (cnode)->sibling)
		{
			dup_check = gtk_ctree_node_get_row_data (GTK_CTREE (dl_list), cnode);
			assert (dup_check);
			if (!STRCMP (SHARE (dup_check)->location, shr->location))
				return NULL;
		}
	}

	/* setup the transfer object ... */
	transfer = OBJ_NEW (Transfer);

 	transfer->transfer_rate = NULL;

	share_dup (SHARE (transfer), shr);
	shr = SHARE (transfer);

	/* a addsource packet without id */
	if (!transfer->id)
	{
		parent = gtk_ctree_node_get_row_data (GTK_CTREE(dl_list), insert_node);

		/* if a parent could be found get id from him */
		if (parent)
			transfer->id = parent->id;
	}

	add_id[0] = shr->filename;
	add_id[1] = shr->user ? shr->user : "";
	add_id[2] = (insert_node) ? "" : "Processing...";

	obj_set_data (OBJ_DATA (transfer), "tree", dl_list);

	transfer_get_pixmap (transfer, &nodepixmap, &nodebitmap);
	new_node = gtk_ctree_insert_node (GTK_CTREE (dl_list), insert_node, NULL,
                                     add_id, 5, nodepixmap, nodebitmap,
                                     nodepixmap, nodebitmap, 0, 0);

	obj_set_data (OBJ_DATA (transfer), "ft_app", NULL);
	obj_set_data (OBJ_DATA (transfer), "node", new_node);

	SET_NODE_DATA (dl_list, new_node, transfer);

	return transfer;
}

/*****************************************************************************/

static void write_source (FEConnection *c, Transfer *obj)
{
	gift_fe_debug ("write_source for id=%d, source=%s, user%s\n", obj->id,
	               SHARE (obj)->location, SHARE (obj)->user);
	daemon_send ("<transfer id=\"%lu\" user=\"%s\""
	             " addsource=\"%s\"/>\n",
	             obj->id, SHARE (obj)->user, SHARE (obj)->location);
}

static void add_sources (FEConnection *c, Transfer *obj,
						 GtkCTree *tree, GtkCTreeNode *node)
{
	trace ();

	/* the root node doesnt contain a source any more */
	/* write_source (c, obj); */

	if (GTK_CTREE_ROW (node)->children)
	{
		GtkCTreeNode *cnode;

		cnode = GTK_CTREE_ROW (node)->children;

		/* loop all siblings and add them as a source */
		for (; cnode; cnode = GTK_CTREE_ROW (cnode)->sibling)
		{
			Transfer *child_obj;

			if (!(child_obj = gtk_ctree_node_get_row_data (tree, cnode)))
				continue;

			/* set object id for this child */
			child_obj->id = obj->id;
			write_source (c, child_obj);
		}
	}
}

int download_response (char *head, int keys, GData *dataset, DaemonEvent *event)
{
	Transfer      *obj;
	GtkWidget     *tree_wid;
	GtkCTree      *tree;
	GtkCTreeNode  *node;
	char          *cell_text;
	char          *addsource;
	unsigned long  transmit;

	if (!event || !event->obj)
		return FALSE;

	obj     = event->obj;
	obj->id = event->id;

	tree_wid = (GtkWidget *) (obj_get_data (OBJ_DATA (obj), "tree"));
	tree     = GTK_CTREE (tree_wid);
	node     = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (obj), "node"));

	if (!tree_wid || !tree)
		return FALSE;

	/* download finished or canceled by UI will match this */
	if (!head || !dataset)
	{
		/* is a cancel request from USER or autoremove, remove node */
		if (obj->active == TRANSFER_CANCELLED ||
			config_get_int (gift_fe_conf, "general/autormdl=0"))
		{
			obj->active = TRANSFER_FINISHED;
 			gtk_ctree_remove_node (tree, node);
		}
		else
		{
			/* is a finished event from daemon, don't remove node */
			gift_fe_debug ("*** transfer finished ... %lu / %lu\n",
			               obj->id, obj->transmit, obj->total);
 			gtk_ctree_node_set_text (tree, node, 2, "Finished");
			obj->active = TRANSFER_FINISHED;
		}
		return FALSE;
	}

	addsource = g_datalist_get_data (&dataset, "addsource");

	/* if this was an implicit download request, we are going to need to add
	 * sources on the fly */
	if (!node)
	{
		unsigned long total = SHARE (obj)->filesize;
		char *hash;

		hash = SHARE (obj)->hash;
		share_fill_data (SHARE (obj), dataset);

		SHARE (obj)->filename = format_href_disp (SHARE (obj)->location);
		/* since we don't know the hash from the 2nd Package we get it
		 * from the 1st Package's transfer object and duplicate it */
		SHARE (obj)->hash     = STRDUP (hash);

		event->obj = download_insert (tree_wid, node, SHARE (obj));

		/* download_insert created a new (more complete) object reference
		 * for this source than we had previously...let's play a little
		 * trickery for the root node */
		obj_free (obj);

		obj     = event->obj;
		obj->id = event->id;

		obj->total = total;

		return TRUE;
	}
	else if (addsource)
	{
		SharedFile shr;

		share_fill_data (&shr, dataset);
		/* this is the 2nd-nth  "2nd Package" we get we also dont know
		 * the hash here so duplicate it from the 1st Package */
		shr.hash = STRDUP (SHARE (obj)->hash);
		download_insert (tree_wid, node, &shr);

		fe_share_free (&shr);

		return TRUE;
	}

	/*
	 * updating transfer statistics now
	 */

	/* we have not added any sources, do that now */
	if (!strcmp (head, "event"))
	{
		add_sources (daemon_interface (), obj, tree, node);
		return TRUE;
	}

	/* ignore anything without transmit attribute */
	if (g_datalist_get_data (&dataset, "transmit"))
	{
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
			calculate_transfer_info (obj->transmit,
									 obj->total,
									 transfer_get_rate (obj));

		obj->transmit = transmit;
		gtk_ctree_node_set_text (tree, node, 2, cell_text);
		g_free (cell_text);
		if (obj->active == TRANSFER_WAITING)
			obj->active = TRANSFER_ACTIVE;
	}

	return TRUE;
}

/* process the inactive nodes ... */
static void download_start_node (GtkCTree *ctree, GtkCTreeNode *node,
                                 void *data)
{
	Transfer     *transfer;

	transfer = gtk_ctree_node_get_row_data (GTK_CTREE (ctree), node);

	assert (transfer);

	if (transfer->active)
	{
		GtkCTreeNode *cnode;
		Transfer     *sibling_tr;

		for (cnode = GTK_CTREE_ROW (node)->children; cnode;
		     cnode = GTK_CTREE_ROW (cnode)->sibling)
		{
			sibling_tr =
				gtk_ctree_node_get_row_data (GTK_CTREE (ctree), cnode);

			assert (sibling_tr);

			if (!sibling_tr->active)
			{
				/* This is an added but not yet activated transfer */
				sibling_tr->id = transfer->id;
				write_source (daemon_interface (), sibling_tr);
				sibling_tr->active = TRANSFER_ACTIVE;
			}
		}
	}
	else
	{
		/* we now have an active transfer, let's fuck with it */
		transfer->active = TRANSFER_ACTIVE;
		transfer->total  = SHARE (transfer)->filesize;

		daemon_request ((ParsePacketFunc) download_response, transfer, NULL,
						"<transfer action=download save=\"%s\" size=\"%u\" "
						"hash=\"%s\"/>\n",
						SHARE (transfer)->filename, SHARE (transfer)->filesize,
						SHARE (transfer)->hash);
	}
}

/* actually start downloading the new files ... */
void download_start (GtkWidget *dl_list)
{
	trace ();

	/* search the root nodes only ... */
	gtk_ctree_post_recursive_to_depth (GTK_CTREE (dl_list), NULL, 1,
	                                   (GtkCTreeFunc) download_start_node,
	                                   NULL);
}

/* if cancel is true, the temporary file will be removed and will be removed
 * from the clist.  otherwise, the download will merely be suspended
 *
 * NOTE - this function may operate on a finished download to simply remove
 * it from the ctree */
static void download_stop (Transfer *transfer, int cancel)
{
	GtkCTree     *ctree;
	GtkCTreeNode *node;

	trace ();

	assert (transfer);

	/* already paused */
	if (!cancel && transfer->active == TRANSFER_PAUSED)
		return;

	ctree = GTK_CTREE      (obj_get_data (OBJ_DATA (transfer), "tree"));
	node  = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (transfer), "node"));

	if (transfer->active != TRANSFER_FINISHED)
	{
		daemon_send ("<transfer id=\"%lu\" action=\"%s\"/>\n",
		             transfer->id, (cancel ? "cancel" : "pause"));
	}

	if (cancel)
	{
		/* this is a cludge: IF transfer finished and user called download_stop
		 * remobe the node, and thats it, the event is already removed, as the
		 * deamon wrote a event cancel message. ELSE the cancel request was written
		 * in the previous if statement. so just set transfer->active that the
		 * callback download_response knows that we also want to delete the node
		 */
		if (transfer->active == TRANSFER_FINISHED)
			gtk_ctree_remove_node (ctree, node);
		else
			transfer->active = TRANSFER_CANCELLED;
	}
	else
	{
		gtk_ctree_node_set_text (ctree, node, 2, "Paused");
		transfer->active = TRANSFER_PAUSED;
	}
}

/****************************************************************************/

#if 0

static void menu_pause_download (GtkWidget *menu, Transfer *transfer)
{
	if (transfer->active == TRANSFER_PAUSED) {
		/* What is neccessary to reactivate a download ?? */
		daemon_send ("<transfer id=\"%lu\" action=\"%s\"/>\n",
					 transfer->id, "resume");
		transfer->active = TRANSFER_ACTIVE;
	}
	else
		download_stop (transfer, FALSE);
}

#endif

static void menu_cancel_download_it (Transfer *item)
{
	download_stop (item, TRUE);
}

static void menu_cancel_download (GtkWidget *menu, Transfer *transfer)
{
	GtkCTree *ctree;

	ctree = GTK_CTREE (obj_get_data (OBJ_DATA (transfer), "tree"));
	with_ctree_selection (ctree, menu_cancel_download_it);
}

static void menu_clear_download_it (Transfer *item)
{
	if (item->active != TRANSFER_FINISHED)
	{
		gift_fe_debug ("*** transfer of '%s' is not finished!\n",
				SHARE (item)->filename);
	}
	else
		download_stop (item, TRUE);
}

static void menu_clear_download (GtkWidget *menu, Transfer *transfer)
{
	GtkCTree *ctree;

	ctree = GTK_CTREE (obj_get_data (OBJ_DATA (transfer), "tree"));
	with_ctree_selection (ctree, menu_clear_download_it);
}

static void menu_remove_source (GtkWidget *menu, Transfer *transfer)
{
	int id;
	char *location;
	char *hash;
	char *user;
	GtkCTreeNode *node;
	GtkCTree *tree;

	node = obj_get_data (OBJ_DATA (transfer), "node");
	tree = obj_get_data (OBJ_DATA (transfer), "tree");
	id = transfer->id;
	location = SHARE (transfer)->location;
	hash = SHARE (transfer)->hash;
	user = SHARE (transfer)->user;

	assert (hash);
	assert (user);
	assert (location);
	assert (node);
	assert (tree);

	daemon_send ("<transfer id=\"%lu\" hash=\"%s\" user=\"%s\" "
                     "delsource=\"%s\"/>\n", id, hash, user, location);

	gtk_ctree_remove_node (tree, node);
}

int menu_popup_download (GtkWidget *dl_list, Transfer *download, guint button,
						 guint32 at)
{
	GtkWidget *menu;
	GtkWidget *item;
	GtkCTreeNode *node;

	menu = gtk_menu_new ();
	node = obj_get_data (OBJ_DATA (download), "node");

#if 0
	item = menu_append (menu,
	                    (download->active == TRANSFER_PAUSED) ?
	                    "Resume Download" : "Pause Download");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
	                    GTK_SIGNAL_FUNC (menu_pause_download), download);
#endif

	if (SHARE (download)->user)
	{
		item = menu_append (menu, "More files from this user...");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
	                        GTK_SIGNAL_FUNC (menu_search_user), download);
	}
	if (SHARE (download)->hash)
	{
		item = menu_append (menu, "Find additional sources");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
                            GTK_SIGNAL_FUNC (menu_search_hash), download);
		menu_append_sep (menu);
	}

	switch (GTK_CTREE_ROW (node)->level)
	{
	 case 1: /* root node */
		item = menu_append (menu, "Cancel Download");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
	                        GTK_SIGNAL_FUNC (menu_cancel_download), download);

		item = menu_append (menu, "Clear if Finished");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
	                        GTK_SIGNAL_FUNC (menu_clear_download), download);
		break;
	 case 2: /* added source */
		item = menu_append (menu, "Remove Source");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
	                        GTK_SIGNAL_FUNC (menu_remove_source), download);
		break;
	 default:
		break;
	}

	gtk_widget_show_all (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, at);

	return TRUE;
}

/*****************************************************************************/
