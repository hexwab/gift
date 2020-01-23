/*
 * download.c
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
#include "download.h"
#include "daemon.h"

/* download stats interval (in seconds) */
#define STATS_INTERVAL 1

/*****************************************************************************/

Transfer *download_insert (GtkWidget *dl_list, GtkCTreeNode *node_hint,
                           SharedFile *shr)
{
	Transfer     *transfer;
	char         *add_id[3];
	GtkCTreeNode *new_node;
	GtkCTreeNode *insert_node;

	/* setup the transfer object ... */
	transfer = OBJ_NEW (Transfer);

	share_dup (SHARE (transfer), shr);
	shr = SHARE (transfer);

	/* insert into the tree with the appropriate outsourced organization */
	insert_node = (node_hint ? node_hint :
				   find_share_node (shr->hash, NULL, 0, dl_list, NULL));

	add_id[0] = shr->filename;
	add_id[1] = shr->user;
	add_id[2] = (insert_node) ? "" : "Processing...";

	new_node = gtk_ctree_insert_node (GTK_CTREE (dl_list), insert_node, NULL,
									  add_id, 0, NULL, NULL, NULL, NULL, 0, 0);

	obj_set_data (OBJ_DATA (transfer), "ft_app", NULL);
	obj_set_data (OBJ_DATA (transfer), "tree", dl_list);
	obj_set_data (OBJ_DATA (transfer), "node", new_node);

	SET_NODE_DATA (dl_list, new_node, transfer);

	return transfer;
}

/*****************************************************************************/

static void write_source (FEConnection *c, Transfer *obj)
{
	char *request;

	request = malloc (strlen (SHARE (obj)->location) + 256);
	sprintf (request,
	         "<transfer id=\"%lu\" user=\"%s\" hash=\"%s\" addsource=\"%s\"/>\n",
	         obj->id, SHARE (obj)->user, SHARE (obj)->hash,
	         SHARE (obj)->location);

	printf ("write_source: %s\n", request);
	net_send (c->fd, request);
	free (request);
}

static void add_sources (FEConnection *c, Transfer *obj,
                         GtkCTree *tree, GtkCTreeNode *node)
{
	trace ();

	write_source (c, obj);

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

	tree     = GTK_CTREE      (obj_get_data (OBJ_DATA (obj), "tree"));
	tree_wid = (GtkWidget *)  (obj_get_data (OBJ_DATA (obj), "tree"));
	node     = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (obj), "node"));

	if (!tree || !tree_wid)
		return FALSE;

	if (!head || !dataset)
	{
		printf ("*** transfer finished ... %lu / %lu\n",
				obj->transmit, obj->total);

		gtk_ctree_node_set_text (tree, node, 2, "Finished");

		obj->finished = TRUE;

		return FALSE;
	}

	addsource = g_datalist_get_data (&dataset, "addsource");

	/* if this was an implicit download request, we are going to need to add
	 * sources on the fly */
	if (!node)
	{
		unsigned long total = SHARE (obj)->filesize;

		share_fill_data (SHARE (obj), dataset);

		SHARE (obj)->location = STRDUP (addsource);
		SHARE (obj)->filename = format_href_disp (SHARE (obj)->location);

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
		shr.location = STRDUP (addsource);
		shr.filename = format_href_disp (shr.location);

		download_insert (tree_wid, node, &shr);

		share_free (&shr);

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

/* process the inactive nodes ... */
static void download_start_node (GtkCTree *ctree, GtkCTreeNode *node,
								 void *data)
{
	Transfer     *transfer;

	transfer = gtk_ctree_node_get_row_data (GTK_CTREE (ctree), node);

	assert (transfer);

	if (transfer->active)
	{
		fprintf (stderr, "transfer is active (%i), TODO -- add_source\n",
				 transfer->finished);
		return;
	}

	/* we now have an active transfer, let's fuck with it */
	transfer->active = TRANSFER_ACTIVE;
	transfer->total  = SHARE (transfer)->filesize;

	printf ("activating search for %u %s\n\t%s\n\n",
			SHARE (transfer)->filesize, SHARE (transfer)->filename,
			SHARE (transfer)->location);

#if 0
	locations    = g_list_prepend (locations, SHARE (transfer)->location);
	request_len += strlen (SHARE (transfer)->location) + 256;

	if (GTK_CTREE_ROW (node)->children)
	{
		Transfer *ctransfer;
		GtkCTreeNode *cnode;

		cnode = GTK_CTREE_ROW (node)->children;

		for (; cnode; cnode = GTK_CTREE_ROW (cnode)->sibling)
		{
			ctransfer = gtk_ctree_node_get_row_data (GTK_CTREE (ctree), cnode);
			if (!ctransfer)
				return;

			printf ("\t%s\n", SHARE (ctransfer)->location);
			locations    = g_list_prepend (locations, SHARE (ctransfer)->location);
			request_len += strlen (SHARE (transfer)->location) + 256;
		}
	}
#endif

	daemon_request ((ParsePacketFunc) download_response, transfer, NULL,
	                "<transfer action=download save=\"%s\" size=\"%u\" hash=\"%s\"/>\n",
	                SHARE (transfer)->filename, SHARE (transfer)->filesize,
	                SHARE (transfer)->hash);

#if 0
	c = daemon_request ((ParsePacketFunc) download_response, transfer,
	                    "<transfer method=download action=start size=\"%u\" name=\"%s\"/>\n"
	                    "<transfer method=download action=addsource location=\"%s\"/>\n",
	                    SHARE (transfer)->filesize, SHARE (transfer)->filename,
	                    SHARE (transfer)->location);

	printf ("}\n");

	printf ("c = %p\n", c);
	obj_set_data (OBJ_DATA (transfer), "FEConnection", c);


	g_list_free (locations);
#endif
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
void download_stop (Transfer *transfer, int cancel)
{
	GtkCTree     *ctree;
	GtkCTreeNode *node;

	trace ();

	assert (transfer);

	printf ("transfer->id = %lu, %i, %i\n", transfer->id,
	        cancel, transfer->active);

	/* already paused */
	if (!cancel && transfer->active == TRANSFER_PAUSED)
		return;

	ctree = GTK_CTREE      (obj_get_data (OBJ_DATA (transfer), "tree"));
	node  = GTK_CTREE_NODE (obj_get_data (OBJ_DATA (transfer), "node"));

	if (!transfer->finished)
	{
		FEConnection *c;
		char *request;

		c = daemon_interface ();

		request = g_strdup_printf ("<transfer id=\"%lu\" action=\"%s\"/>\n",
								   transfer->id, (cancel ? "cancel" : "pause"));
		net_send (c->fd, request);
		free (request);
	}

	if (cancel)
	{
		daemon_event_remove (transfer->id);
		gtk_ctree_remove_node (ctree, node);
	}
	else
	{
		gtk_ctree_node_set_text (ctree, node, 2, "Paused");
		transfer->active = TRANSFER_PAUSED;
	}
}

/*****************************************************************************/

static void menu_pause_download (GtkWidget *menu, Transfer *transfer)
{
	if (transfer->active == TRANSFER_PAUSED)
	{
		printf ("resume not implemented\n");
		return;
	}

	download_stop (transfer, FALSE);
}

static void menu_cancel_download (GtkWidget *menu, Transfer *transfer)
{
	download_stop (transfer, TRUE);
}

static void menu_clear_download (GtkWidget *menu, Transfer *transfer)
{
	if (!transfer->finished)
	{
		printf ("*** transfer isnt finished!\n");
		return;
	}

	download_stop (transfer, TRUE);
}

int menu_popup_download (GtkWidget *dl_list, Transfer *download, guint button,
						 guint32 at)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new ();

#if 0
	item = menu_append (menu,
	                    (download->active == TRANSFER_PAUSED) ?
	                    "Resume Download" : "Pause Download");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
	                    GTK_SIGNAL_FUNC (menu_pause_download), download);
#endif

	item = menu_append (menu, "Cancel Download");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
	                    GTK_SIGNAL_FUNC (menu_cancel_download), download);

	menu_append_sep (menu);

	item = menu_append (menu, "Clear Finished");
	gtk_signal_connect (GTK_OBJECT (item), "activate",
	                    GTK_SIGNAL_FUNC (menu_clear_download), download);

	gtk_widget_show_all (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, at);

	return TRUE;
}
