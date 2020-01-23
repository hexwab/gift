/*
 * pref.c
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

/*****************************************************************************/

static GtkWidget *pref_dialog = NULL;
static GtkWidget *pref_pages  = NULL;
static GtkWidget *pref_pane   = NULL;

#define PREF_CLOSE 0x01
#define PREF_APPLY 0x02

/*****************************************************************************/

static void pref_hide ()
{
	gtk_widget_hide (pref_dialog);
}

static void pref_dialog_close ()
{
	gtk_widget_destroy (pref_dialog);
	pref_dialog = NULL;
}

/*****************************************************************************/

static void pref_select_page (char *page)
{
	if (!pref_dialog)
		return;
}

static void pref_button (GtkWidget *button, unsigned long pref_enum)
{
	if (pref_enum & PREF_APPLY)
	{
		printf ("applying...\n");
	}

	if (pref_enum & PREF_CLOSE)
	{
		printf ("closing...\n");
		pref_dialog_close ();
	}
}

/*****************************************************************************/

void pref_show (char *page)
{
	GtkWidget *button;
	GtkWidget *vbox;
	GtkWidget *hbox;

	printf ("showing pref page %s\n", page);

	if (pref_dialog)
	{
		gtk_widget_show (pref_dialog);
		pref_select_page (page);
		return;
	}

	pref_dialog = gtk_dialog_new ();
	gtk_widget_set_usize (pref_dialog, 550, 300);

	gtk_signal_connect (GTK_OBJECT (pref_dialog), "delete-event",
						GTK_SIGNAL_FUNC (pref_dialog_close), NULL);

/*****************************************************************************/

	vbox = GTK_DIALOG (pref_dialog)->vbox;
	gtk_box_set_spacing (GTK_BOX (vbox), 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	hbox = gtk_hbox_new (FALSE, 5);
	PACK (vbox, hbox, PACK_FILL | PACK_EXPAND);

/*****************************************************************************/

	{
		char *titles[] = { "Category" };
		pref_pages = gtk_ctree_new_with_titles (LIST_SIZE (titles), 0, titles);
	}

	gtk_widget_set_usize (pref_pages, 130, -1);

	PACK (hbox, pref_pages, PACK_NONE);

/*****************************************************************************/

	pref_pane = gtk_vbox_new (FALSE, 5);
	PACK (hbox, pref_pane, PACK_FILL | PACK_EXPAND);

/*****************************************************************************/

	vbox = GTK_DIALOG (pref_dialog)->action_area;

	hbox = gtk_hbox_new (FALSE, 5);
	PACK (vbox, hbox, PACK_FILL | PACK_EXPAND);

	button = gtk_button_new_with_label ("Cancel");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
						GTK_SIGNAL_FUNC (pref_button), (void *) PREF_CLOSE);
	gtk_widget_set_usize (button, 50, 26);
	PACK_END (hbox, button, PACK_NONE);

	button = gtk_button_new_with_label ("Apply");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
						GTK_SIGNAL_FUNC (pref_button), (void *) PREF_APPLY);
	gtk_widget_set_usize (button, 50, 26);
	PACK_END (hbox, button, PACK_NONE);

	button = gtk_button_new_with_label ("OK");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
						GTK_SIGNAL_FUNC (pref_button), (void *) (PREF_APPLY | PREF_CLOSE));
	gtk_widget_set_usize (button, 50, 26);
	PACK_END (hbox, button, PACK_NONE);

	gtk_widget_show_all (pref_dialog);
}
