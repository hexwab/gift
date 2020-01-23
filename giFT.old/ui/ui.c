/*
 * ui.c
 *
 * GTK is crappy.  But not your usual crap.  It's used because it looks pretty
 * on the outside, but is really shallow and bitchy on the inside.  Yeah, I
 * just wish those sorts of toolkits would let me use them when I was in
 * high school.
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

#include "ui.h"
#include "menu.h"

#include "search.h"
#include "download.h"
#include "upload.h"
#include "stats.h"

/*****************************************************************************/

static void ui_search_clicked (GtkWidget *button, FTApp *ft)
{
	char *query = gtk_entry_get_text (GTK_ENTRY (ft->query));
	char *media_filter = gtk_object_get_data (GTK_OBJECT (ft->media_filter),
											  "filter");

	search_execute (ft, query, media_filter);

	gtk_entry_set_text (GTK_ENTRY (ft->query), "");
}

/**************************************************************************/

static void media_filter_select (GtkWidget *widget, char *data)
{
	GtkWidget *o_menu;
	char *cleanup;

	o_menu = gtk_object_get_data (GTK_OBJECT (widget), "option_menu");

	if ((cleanup = gtk_object_get_data (GTK_OBJECT (o_menu), "filter")))
	{
		/* do nothing if we are currently selecting that entry ... */
		if (!strcmp (cleanup, data))
			return;

		free (cleanup);
	}

	gtk_object_set_data (GTK_OBJECT (o_menu), "filter", strdup (data));
}

static GtkWidget *construct_media_filter ()
{
	char **ptr, *items[] = { SUPPORTED_FORMATS };
	GtkWidget *o_menu, *sub_menu;

	o_menu = gtk_option_menu_new ();

	sub_menu = gtk_menu_new ();

	/* create all the entries within the media filter list */
	for (ptr = items; *ptr; ptr++)
	{
		GtkWidget *mitem;

		mitem = gtk_menu_item_new_with_label (*ptr);
		gtk_menu_append (GTK_MENU (sub_menu), mitem);

		/* this is a lame hack to reference the option menu */
		gtk_object_set_data (GTK_OBJECT (mitem), "option_menu", o_menu);

		gtk_signal_connect (GTK_OBJECT (mitem), "activate",
							GTK_SIGNAL_FUNC (media_filter_select),
							STRDUP (*ptr)); /* ... this is never freed */
		gtk_widget_show (mitem);

		/* activate the first entry ... */
		if (ptr == items)
			gtk_menu_item_activate (GTK_MENU_ITEM (mitem));
	}

	/* actually add it */
	gtk_option_menu_set_menu (GTK_OPTION_MENU (o_menu), sub_menu);

	return o_menu;
}
/**************************************************************************/

int construct_main (FTApp *ft, char *title, char *cls)
{
	GtkWidget *vbox_mbar, *vbox, *hbox, *sw, *label, *mbar;

	/**********************************************************************/

	ft->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	/* set window properties */
	gtk_widget_set_usize (GTK_WIDGET (ft->window),
						  DEF_WIDTH, DEF_HEIGHT);
	gtk_window_set_wmclass (GTK_WINDOW (ft->window), cls, title);

	/* handle delete event */
	gtk_signal_connect (GTK_OBJECT (ft->window), "delete-event",
						GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	/**********************************************************************/

	vbox_mbar = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (ft->window), vbox_mbar);

	mbar = menu_create_main (ft->window);
	PACK (vbox_mbar, mbar, PACK_NONE);

	/**********************************************************************/

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	PACK (vbox_mbar, vbox, PACK_FILL | PACK_EXPAND);

	ft->notebook = gtk_notebook_new ();
	PACK (vbox, ft->notebook, PACK_FILL | PACK_EXPAND);

	/**********************************************************************
	 * SEARCH TAB
	 **********************************************************************/

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_notebook_append_page (GTK_NOTEBOOK (ft->notebook), vbox,
							  gtk_label_new ("Search"));

	hbox = gtk_hbox_new (FALSE, 5);
	PACK (vbox, hbox, PACK_NONE);

	label = gtk_label_new ("Query: ");
	PACK (hbox, label, PACK_NONE);

	/* search query string */
	ft->query = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (ft->query), "activate",
						GTK_SIGNAL_FUNC (ui_search_clicked), ft);
	PACK (hbox, ft->query, PACK_FILL | PACK_EXPAND);

	/* everything / audio / video / ... */
	ft->media_filter = construct_media_filter ();
	PACK (hbox, ft->media_filter, PACK_NONE);

	/* search button */
	ft->search = gtk_button_new_with_label (" Search ");
	gtk_signal_connect (GTK_OBJECT (ft->search), "clicked",
						GTK_SIGNAL_FUNC (ui_search_clicked), ft);
	PACK (hbox, ft->search, PACK_NONE);

	/**********************************************************************/

	hbox = gtk_hbox_new (FALSE, 5);
	PACK (vbox, hbox, PACK_FILL | PACK_EXPAND);

	/*
	 * Main search clist
	 */
	{
		char *titles[] = { "Query Result", "User", "Filesize" };
		ft->srch_list = gtk_ctree_new_with_titles (LIST_SIZE (titles), 0,
												   titles);
	}

	SET_PARENT (ft->srch_list, ft);

	gtk_ctree_set_line_style (GTK_CTREE (ft->srch_list),
	                          GTK_CTREE_LINES_DOTTED);

	/* allow multiple selections ... */
	gtk_clist_set_selection_mode (GTK_CLIST (ft->srch_list),
	                              GTK_SELECTION_EXTENDED);

	gtk_signal_connect (GTK_OBJECT (ft->srch_list), "button_press_event",
	                    GTK_SIGNAL_FUNC (ft_handle_popup),
	                    (MenuCreateFunc) menu_popup_search);
	gtk_signal_connect (GTK_OBJECT (ft->srch_list), "click-column",
	                    GTK_SIGNAL_FUNC (srch_list_sort_column),
	                    GINT_TO_POINTER (2));
	gtk_signal_connect (GTK_OBJECT (ft->srch_list), "click-column",
	                    GTK_SIGNAL_FUNC (generic_column_sort), "ss ");

	/* TODO - fix GTK's stupid clist width settings so that the last
	 * column doesnt gain max %age width, the 0th should. */
	gtk_clist_set_column_width (GTK_CLIST (ft->srch_list), 0,
	                            DEF_WIDTH - 270);
	gtk_clist_set_column_width (GTK_CLIST (ft->srch_list), 1, 130);

	/* create the scrolled win */
	sw = ft_scrolled_window (ft->srch_list,
							 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	PACK (hbox, sw, PACK_FILL | PACK_EXPAND);

	/**********************************************************************
	 * TRANSFER TAB
	 **********************************************************************/

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_notebook_append_page (GTK_NOTEBOOK (ft->notebook), vbox,
							  gtk_label_new ("Transfer"));

	/*
	 * Downloads frame
	 */
	ft->dl_frame = gtk_frame_new ("Downloads");
	PACK (vbox, ft->dl_frame, PACK_FILL | PACK_EXPAND);
	{
		GtkWidget *frame_vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (frame_vbox), 5);
		gtk_container_add (GTK_CONTAINER (ft->dl_frame), frame_vbox);

		/*
		 * Download clist
		 */
		{
			char *titles[] = { "Filename", "User", "Progress" };
			ft->dl_list = gtk_ctree_new_with_titles (LIST_SIZE (titles), 0,
													 titles);
		}

		SET_PARENT (ft->dl_list, ft);

		gtk_ctree_set_line_style (GTK_CTREE (ft->srch_list),
								  GTK_CTREE_LINES_DOTTED);

		gtk_clist_set_selection_mode (GTK_CLIST (ft->dl_list),
									  GTK_SELECTION_EXTENDED);

		gtk_signal_connect (GTK_OBJECT (ft->dl_list), "button_press_event",
							GTK_SIGNAL_FUNC (ft_handle_popup),
							(MenuCreateFunc) menu_popup_download);
		gtk_signal_connect (GTK_OBJECT (ft->dl_list), "click-column",
							GTK_SIGNAL_FUNC (generic_column_sort), "ss");

		gtk_clist_set_column_width (GTK_CLIST (ft->dl_list), 0,
									DEF_WIDTH - 460);
		gtk_clist_set_column_width (GTK_CLIST (ft->dl_list), 1, 130);

		sw = ft_scrolled_window (ft->dl_list,
								 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

		PACK (frame_vbox, sw, PACK_FILL | PACK_EXPAND);
	}

	/**********************************************************************/

	/*
	 * Uploads frame
	 */
	ft->ul_frame = gtk_frame_new ("Uploads");
	PACK (vbox, ft->ul_frame, PACK_FILL | PACK_EXPAND);
	{
		GtkWidget *frame_vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (frame_vbox), 5);
		gtk_container_add (GTK_CONTAINER (ft->ul_frame), frame_vbox);

		/*
		 * Upload clist
		 */
		{
			char *titles[] = { "Filename", "User", "Progress" };
			ft->ul_list = gtk_ctree_new_with_titles (LIST_SIZE (titles), 0,
													 titles);
		}

		SET_PARENT (ft->ul_list, ft);

		gtk_ctree_set_line_style (GTK_CTREE (ft->srch_list),
								  GTK_CTREE_LINES_DOTTED);

		gtk_clist_set_selection_mode (GTK_CLIST (ft->ul_list),
									  GTK_SELECTION_EXTENDED);

		gtk_signal_connect (GTK_OBJECT (ft->ul_list), "button_press_event",
							GTK_SIGNAL_FUNC (ft_handle_popup),
							(MenuCreateFunc) menu_popup_upload);
		gtk_signal_connect (GTK_OBJECT (ft->ul_list), "click-column",
							GTK_SIGNAL_FUNC (generic_column_sort), "ss");

		gtk_clist_set_column_width (GTK_CLIST (ft->ul_list), 0,
									DEF_WIDTH - 460);
		gtk_clist_set_column_width (GTK_CLIST (ft->ul_list), 1, 130);

		sw = ft_scrolled_window (ft->ul_list,
								 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

		PACK (frame_vbox, sw, PACK_FILL | PACK_EXPAND);
	}

	/*************************************************************************
	 * STATISTICS TAB
	 *************************************************************************/

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_notebook_append_page (GTK_NOTEBOOK (ft->notebook), vbox,
							  gtk_label_new ("Network"));

	/*
	 * Stats clist
	 */
	{
		char *titles[] = { "Protocol", "Status", "Users", "Shares", "Total" };
		ft->stats_list = gtk_ctree_new_with_titles (LIST_SIZE (titles), 0,
													titles);
	}

	SET_PARENT (ft->stats_list, ft);

	gtk_object_set_data (GTK_OBJECT (ft->stats_list), "refresh",
						 (void *) gtk_timeout_add (2000, (GtkFunction) stats_timer_populate, ft));
	stats_populate (ft->stats_list);

	gtk_ctree_set_line_style (GTK_CTREE (ft->stats_list),
	                          GTK_CTREE_LINES_DOTTED);

	gtk_clist_set_selection_mode (GTK_CLIST (ft->stats_list),
	                              GTK_SELECTION_EXTENDED);

#if 0
	gtk_signal_connect (GTK_OBJECT (ft->stats_list), "button_press_event",
						GTK_SIGNAL_FUNC (ft_handle_popup),
						(MenuCreateFunc) menu_popup_stats);
#endif
	gtk_signal_connect (GTK_OBJECT (ft->stats_list), "click-column",
	                    GTK_SIGNAL_FUNC (generic_column_sort), "ssiis");

	gtk_clist_set_column_width (GTK_CLIST (ft->stats_list), 0,
								DEF_WIDTH - 460);
	gtk_clist_set_column_width (GTK_CLIST (ft->stats_list), 1, 100);
	gtk_clist_set_column_width (GTK_CLIST (ft->stats_list), 2, 50);
	gtk_clist_set_column_width (GTK_CLIST (ft->stats_list), 3, 75);

	sw = ft_scrolled_window (ft->stats_list,
							 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

	PACK (vbox, sw, PACK_FILL | PACK_EXPAND);

	/*************************************************************************/

	/* end gtk crap.  phew. */

	gtk_widget_show_all (ft->window);

	return TRUE;
}
