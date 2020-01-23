/*
 * search.c
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
#include "search.h"
#include "download.h"
#include "daemon.h"

/*****************************************************************************/

/*
 * used to buffer 20 search results as to avoid massive CPU usage and
 * flickering when flooded with results
 */
#define MAX_RESULTS 20
#define MAX_FLUSH   2000

static GList *srch_buffer     = NULL;
static int    search_flush_id = -1;

/*****************************************************************************/

/*
 * locate a position in the tree and insert the given search result
 * ... group under file hash
 *
 * TODO - this is _slow_
 */
static int search_insert_node (Search *srch, GtkWidget *tree)
{
	SharedFile *share;
	char *add_id[3], *user = NULL;
	GtkCTreeNode *new_node, *insert_node, *search_node;

	share = SHARE (srch);

	if (!share->filename || !share->user)
	{
		OBJ_FREE (srch);
		return FALSE;
	}

	/* this is a temp node location to store under the search query */
	search_node = obj_get_data (OBJ_DATA (srch), "node");

	/* if insert_node is NULL, insert at the root level */
	insert_node = find_share_node (share->hash, NULL, share->filesize,
	                               tree, search_node);

	/* check to make sure this is not a duplicate from the same user */
	gtk_ctree_node_get_text (GTK_CTREE (tree), insert_node, 1, &user);

	if ((user && !strcmp (share->user, user)) ||
	    find_share_node (share->hash, NULL, share->filesize,
	                     tree, insert_node) != insert_node)
	{
		OBJ_FREE (srch);
		return FALSE;
	}

	add_id[0] = share->filename;
	add_id[1] = share->user;
	add_id[2] = format_size_disp (0, share->filesize);

	new_node = gtk_ctree_insert_node (GTK_CTREE (tree), insert_node, NULL,
	                                  add_id, 0, NULL, NULL, NULL, NULL, 0, 0);

	obj_copy_data (OBJ_DATA (srch), "ft_app", OBJ_DATA (srch));
	obj_set_data  (OBJ_DATA (srch), "tree",   tree);
	obj_set_data  (OBJ_DATA (srch), "node",   new_node); /* overwrite it */

	SET_NODE_DATA (tree, new_node, srch);

	free (add_id[2]);

	return TRUE;
}

/*****************************************************************************/

static int search_flush_results (FTApp *ft)
{
	/* flush */
	gtk_clist_freeze (GTK_CLIST (ft->srch_list));
	g_list_foreach (srch_buffer, (GFunc) search_insert_node, ft->srch_list);
	gtk_clist_thaw (GTK_CLIST (ft->srch_list));

	/* reset */
	g_list_free (srch_buffer);
	srch_buffer = NULL;

	return TRUE;
}

/* callback from the daemon parsing code */
static int search_response (char *head, int keys, GData *dataset,
                            DaemonEvent *event)
{
	Search       *query;
	Search       *srch;
	FTApp        *ft_app;
	GtkWidget    *tree;
	GtkCTreeNode *node;

	if (!event || !event->obj)
		return FALSE;

	query     = event->obj;
	query->id = event->id;

	tree = obj_get_data (OBJ_DATA (query), "tree");
	node = obj_get_data (OBJ_DATA (query), "node");

	if (!tree || !node)
		return FALSE;

	/* end-of-event */
	if (!head || !dataset)
	{
		int   length;
		char *results;
		char *search_query;

		search_query = obj_get_data (OBJ_DATA (query), "query");

		printf ("end of search '%s'\n", search_query);

		length  = ctree_node_child_length (tree, node);
		results = g_strdup_printf ("%s (%i result%s)", search_query, length,
		                           (length == 1) ? "" : "s");

		gtk_ctree_node_set_text (GTK_CTREE (tree), node, 0, results);

		free (results);

		/* sort the results for them */
		gtk_ctree_sort_recursive (GTK_CTREE (tree), node);

		return FALSE;
	}

	/* id registration */
	if (!strcmp (head, "event"))
		return TRUE;

	/* deal w/ this result? */
	ft_app = obj_get_data (OBJ_DATA (query), "ft_app");
	assert (ft_app);

	/* allocate data for this entry */
	srch = OBJ_NEW (Search);

	obj_set_data  (OBJ_DATA (srch), "ft_app", ft_app);
	obj_copy_data (OBJ_DATA (srch), "tree",   OBJ_DATA (query));
	obj_copy_data (OBJ_DATA (srch), "node",   OBJ_DATA (query));

	share_fill_data (SHARE (srch), dataset);

	/* insert this search into the buffer */
	srch_buffer = g_list_prepend (srch_buffer, srch);

	/* buffer is full, flush it... */
	if (g_list_length (srch_buffer) >= MAX_RESULTS)
	{
		search_flush_results (ft_app);

		/* cap at 500 results - TODO - configurable ... */
		if (g_list_length (GTK_CLIST (ft_app->srch_list)->row_list) >= 500)
			return FALSE;
	}

	/* returning FALSE means we don't want anymore data... */
	return TRUE;
}

/*****************************************************************************/

int search_execute (FTApp *ft, char *search, char *media)
{
	GtkCTreeNode *node;
	Search *query;
	char *add_id[3];

	if (!search || !(*search)) /* I'm sorry but 5 million results is too */
		return FALSE;          /* damn many :)                           */

	/*
	 * Create a search query object so that it can reference it's current
	 * node in the tree for lookup with the right click menu
	 */

	query = OBJ_NEW (Search);

	/* create a parent to hold all searches from the incoming fd */
	add_id[0] = search;
	add_id[1] = add_id[2] = NULL;
	node = gtk_ctree_insert_node (GTK_CTREE (ft->srch_list), NULL, NULL,
								  add_id, 0, NULL, NULL, NULL, NULL, 0, 1);

	/* ... */
	obj_set_data (OBJ_DATA (query), "ft_app", ft);
	obj_set_data (OBJ_DATA (query), "tree", ft->srch_list);
	obj_set_data (OBJ_DATA (query), "node", node);
	obj_set_data (OBJ_DATA (query), "query", STRDUP (search));
	obj_set_data (OBJ_DATA (query), "media", STRDUP (media));

	SET_NODE_DATA (ft->srch_list, node, query);

	/*
	 * Ok, now actually request the search from the daemon
	 */

	if (search_flush_id < 0)
	{
		/* every 4 seconds flush the search regardless of buffer status */
		search_flush_id = gtk_timeout_add (MAX_FLUSH,
										   (GtkFunction) search_flush_results,
										   ft);
	}

	if (!STRCMP (media, "Everything"))
		media = "";

	if (!daemon_request ((ParsePacketFunc) search_response, query, NULL,
	                     "<search query=\"%s\" realm=\"%s\"/>\n",
	                     search, media))
	{
		fprintf (stderr, "daemon not running -- you shall burn in hell!\n");
		return FALSE;
	}

	fprintf (stderr, "executing search '%s'\n", search);

	return TRUE;
}


/*****************************************************************************/

/* locate the "parent" hash if there are multiple unique hashes grouped as
 * one */
static GtkCTreeNode *search_find_unique (Search *srch)
{
	GtkCTree *tree;
	Search *parent_data;
	GtkCTreeNode *parent, *node;

	if (!srch)
		return NULL;

	tree = obj_get_data (OBJ_DATA (srch), "tree");
	node = obj_get_data (OBJ_DATA (srch), "node");

	parent = GTK_CTREE_ROW (node)->parent;

	if (!parent) /* uhh */
		return node;

	parent_data = gtk_ctree_node_get_row_data (tree, parent);

	/* too high... */
	if (!SHARE (parent_data)->filename)
		return node;

	return node;
}

/*****************************************************************************/

static void search_remove_query (GtkWidget *mitem, Search *srch)
{
	GtkWidget *tree;
	Search *ssrch;
	GList *sel, *rem = NULL;

	assert (srch);

	daemon_event_remove (srch->id);

	/* don't really remove srch, remove all of the selections */
	tree = obj_get_data (OBJ_DATA (srch), "tree");

	for (sel = GTK_CLIST (tree)->selection; sel; sel = sel->next)
	{
		ssrch = gtk_ctree_node_get_row_data (GTK_CTREE (tree),
											 sel->data);

		if (!ssrch)
		{
			printf ("uhhh sel->data = %p\n", sel->data);
			continue;
		}

		if (SHARE (ssrch)->filename) /* don't process regular searches */
			continue;

		/* TODO - notify the daemon?  or something? */

		/* flush the buffered results... yeah yeah I know some of them could
		 * still be valid but WHO ASKED YOU!?!?!  asshole. */
		if (srch_buffer)
		{
			g_list_free (srch_buffer);
			srch_buffer = NULL;
		}

		/* buffer the nodes to remove (can't remove them in this loop, as it
		 * would alter 'sel' */
		rem = g_list_prepend (rem, sel->data);
	}

	for (sel = rem; sel; sel = sel->next)
		gtk_ctree_remove_node (GTK_CTREE (tree), sel->data);

	g_list_free (rem);
}

/*****************************************************************************/

static int search_sort_size (GtkCList *clist, const void *p1, const void *p2)
{
	Search *srch1, *srch2;
	SharedFile *s1, *s2;

	srch1 = ((GtkCListRow *)p1)->data;
	srch2 = ((GtkCListRow *)p2)->data;

	s1 = SHARE (srch1);
	s2 = SHARE (srch2);

	if (s1->filesize > s2->filesize)
		return -1;

	return (s1->filesize <= s2->filesize);
}

void srch_list_sort_column (GtkWidget *w, int column, void *sort_col)
{
	int scol;

	trace ();

	scol = GPOINTER_TO_INT (sort_col);

	if (column != scol)
		return;

	gtk_clist_set_compare_func (GTK_CLIST (w), search_sort_size);
}

/*****************************************************************************/

static void srch_list_download (GtkWidget *mitem, Search *srch_node)
{
	GList *sel;
	GtkWidget *srch_list;
	Search *srch;
	FTApp *ft;

	/* get the parent app ... */
	ft = obj_get_data (OBJ_DATA (srch_node), "ft_app");
	srch_list = obj_get_data (OBJ_DATA (srch_node), "tree");

	assert (ft);

	/* loop the selection ... add the selections into the download clist */
	for (sel = GTK_CLIST (srch_list)->selection; sel; sel = sel->next)
	{
		srch = gtk_ctree_node_get_row_data (GTK_CTREE (srch_list),
											sel->data);

		/* insert the nodes for organization by hash */
		download_insert (ft->dl_list, NULL, SHARE (srch));

		/* traverse children ... dont worry about duplicates here */
		if (GTK_CTREE_ROW (sel->data)->children)
		{
			Search *csrch;
			GtkCTreeNode *cnode;

			cnode = GTK_CTREE_ROW (sel->data)->children;

			for (; cnode; cnode = GTK_CTREE_ROW (cnode)->sibling)
			{
				csrch = gtk_ctree_node_get_row_data (GTK_CTREE (srch_list),
													 cnode);

				download_insert (ft->dl_list, NULL, SHARE (csrch));
			}
		}
	}

	/* begin downloading based on the determined organization */
	download_start (ft->dl_list);
}

/*****************************************************************************/

int menu_popup_search (GtkWidget *srch_list, Search *srch, guint button,
					   guint32 at)
{
	GtkWidget *menu, *submenu, *item;
	char *text;

	menu = gtk_menu_new ();

	/* filename == NULL if a search query node itself was clicked, not the
	 * search result for that query */
	if (!SHARE (srch)->filename)
	{
		trace ();

		item = menu_append (menu, "Refresh Search");

		/*
		 * Refresh As -> Everything
		 *               Audio
		 *               Video
		 *               ...
		 */

		item = menu_append (menu, "Refresh As...");

		submenu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

		{
			char **ptr, *titles[] = { SUPPORTED_FORMATS };

			for (ptr = titles; *ptr; ptr++)
				item = menu_append (submenu, *ptr);
		}

		menu_append_sep (menu);

		item = menu_append (menu, "Remove Results");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
							GTK_SIGNAL_FUNC (search_remove_query), srch);
	}
	else
	{
		/* the callbacks will simply gather the needed data from the srch_list
		 * itself... */
		item = menu_append (menu, "Download");
		gtk_signal_connect (GTK_OBJECT (item), "activate",
							GTK_SIGNAL_FUNC (srch_list_download), srch);

		menu_append_sep (menu);

		/* share/user information displayed as menu items... */

		gtk_widget_set_sensitive (menu_append (menu, SHARE (srch)->user), 0);

		/*	menu_append_sep (menu);*/

		text = g_strdup_printf ("Matched files: %i",
								children_length (search_find_unique (srch)));
		menu_append (menu, text);
		free (text);


		text = g_strdup_printf ("Network: %s", SHARE (srch)->network ?
								SHARE (srch)->network : "unknown");
		menu_append (menu, text);
		free (text);

		text = g_strdup_printf ("Filesize: %i", SHARE (srch)->filesize);
		menu_append (menu, text);
		free (text);
	}

	/* boo! */
	gtk_widget_show_all (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, at);

	return TRUE;
}

/*****************************************************************************/

void search_free (Search *obj)
{
	free (obj_get_data (OBJ_DATA (obj), "query"));
	free (obj_get_data (OBJ_DATA (obj), "media"));

	share_free (SHARE (obj));

	free (obj);
}
