/*
 * fe_pref.c
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

#include "conf.h"

/*****************************************************************************/

extern Config    *gift_fe_conf;
static GtkWidget *pref_dialog = NULL;
static GtkWidget *pref_pages  = NULL;
static GtkWidget *pref_pane   = NULL;

#define PREF_CLOSE 0x01
#define PREF_APPLY 0x02

/*****************************************************************************/

static void apply_general ();
static void create_general ();

/*****************************************************************************/

struct option_entry
{
	char *kind;  /* this should be an typedef enumerator */
	char *descr;
	char *name;
	GtkWidget *widget;
};

struct page_entry
{
	char *name;
	void (*f_create) ();
	void (*f_apply) ();
}
pages[] =
{
	{"general", create_general, apply_general },
	{ NULL,     NULL,           NULL }
};

static struct option_entry options_general[] =
{
	{ "text", "Max Search Results",
		"general/max_search_results=1000", NULL },
	{ "bool", "Debug Output/Options",           "general/debug=0",    NULL },
	{ "bool", "Auto Remove finished Downloads", "general/autormdl=0", NULL },
	{ "bool", "Auto Remove finished Uploads",   "general/autormul=0", NULL },
	{ NULL,   NULL,                             NULL,                 NULL }
};

/*****************************************************************************/

#if 0
/* not used anywhere */
static void pref_hide ()
{
	gtk_widget_hide (pref_dialog);
}
#endif

static void apply_general ()
{
	struct option_entry *counter;

	for (counter = options_general; counter->kind; counter++)
	{
		if (!strcmp(counter->kind, "text"))
		{
			config_set_str (gift_fe_conf, counter->name,
			                gtk_entry_get_text (GTK_ENTRY (counter->widget)));
		}
		else if (!strcmp(counter->kind, "bool"))
		{
			config_set_int (gift_fe_conf, counter->name,
			                gtk_toggle_button_get_active (
								GTK_TOGGLE_BUTTON (counter->widget)));
		}
	}
}

static void create_general ()
{
	GtkWidget *rows;
	GtkWidget *cols;
	GtkWidget *value;
	GtkWidget *description;
	GtkWidget *vbox;
	struct option_entry *counter;

	vbox = pref_pane;
	rows = gtk_vbox_new (TRUE, 5);

	for (counter = options_general; counter->kind; counter++)
	{
		cols  = gtk_hbox_new (FALSE, 5);

		if (!strcmp (counter->kind, "text"))
		{
			char *state;

			description = gtk_label_new (counter->descr);

			value = gtk_entry_new ();
			state = config_get_str (gift_fe_conf, counter->name);

			gtk_entry_set_max_length (GTK_ENTRY (value), 5);
			gtk_entry_set_text (GTK_ENTRY (value), state);

			PACK (cols, description, PACK_EXPAND);
			PACK (cols, value, PACK_EXPAND);
		}
		else if (!strcmp (counter->kind, "bool"))
		{
			int state;

			value = gtk_check_button_new_with_label (counter->descr);
			state = config_get_int (gift_fe_conf, counter->name);

			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (value), state);
			PACK (cols, value, PACK_EXPAND | PACK_FILL);
		}

		counter->widget = value;
		PACK (rows, cols, PACK_FILL | PACK_EXPAND);
	}

	PACK (vbox, rows, PACK_FILL | PACK_EXPAND);
	gtk_widget_show_all (vbox);
}

static void pref_dialog_close ()
{
	gtk_widget_destroy (pref_dialog);
	pref_dialog = NULL;
}

/*****************************************************************************/

static void pref_select_page (char *page)
{
	struct page_entry *walker;
	GtkWidget *temp;

	if (!pref_dialog)
		return;

	/* removes current page, assumes pref_pane has only one child */
	temp = gtk_container_children (GTK_CONTAINER (pref_pane))->data;
	gtk_container_remove (GTK_CONTAINER (pref_pane), temp);

	for (walker = pages; walker->name; walker++)
	{
		if (!strcmp (page, walker->name))
			walker->f_create();
	}
}

static void pref_select_cb (GtkCTree *ctree, GList *node, gint column,
                            gpointer user)
{
	pref_select_page (pages[column].name);
}

static void pref_button (GtkWidget *button, unsigned long pref_enum)
{
	struct page_entry *walker;

	if (pref_enum & PREF_APPLY)
	{
		gift_fe_debug ("applying...\n");
		for(walker = pages; walker->name; walker++)
			walker->f_apply();
	}

	if (pref_enum & PREF_CLOSE)
	{
		gift_fe_debug ("closing...\n");
		pref_dialog_close ();
	}
}

/*****************************************************************************/

void pref_show (char *page)
{
	GtkWidget *button;
	GtkWidget *vbox;
	GtkWidget *hbox;

	if (!page)
		page = "general";

	gift_fe_debug ("showing pref page %s\n", page);

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
		struct page_entry *walker;

		pref_pages = gtk_ctree_new_with_titles (LIST_SIZE (titles), 0, titles);
		for (walker = pages; walker->name; walker++)
		{
			gtk_ctree_insert_node (GTK_CTREE (pref_pages),
			                       NULL, NULL, &walker->name , 0, NULL,
			                       NULL, NULL, NULL,  0, 0);
		}
		gtk_signal_connect (GTK_OBJECT (pref_pages), "tree-select-row",
		                    pref_select_cb, NULL);
	}

	gtk_widget_set_usize (pref_pages, 130, -1);

	PACK (hbox, pref_pages, PACK_NONE);

/*****************************************************************************/

	pref_pane = gtk_vbox_new (FALSE, 5);
	PACK (hbox, pref_pane, PACK_FILL | PACK_EXPAND);

	/* add default page "general" to the initial pref pane */
	create_general ();

/*****************************************************************************/

	vbox = GTK_DIALOG (pref_dialog)->action_area;

	hbox = gtk_hbox_new (FALSE, 5);
	PACK (vbox, hbox, PACK_FILL | PACK_EXPAND);

	button = gtk_button_new_with_label ("Cancel");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
	                    GTK_SIGNAL_FUNC (pref_button), (void *) PREF_CLOSE);
	gtk_widget_set_usize (button, 60, 26);
	PACK_END (hbox, button, PACK_NONE);

	button = gtk_button_new_with_label ("Apply");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
	                    GTK_SIGNAL_FUNC (pref_button), (void *) PREF_APPLY);
	gtk_widget_set_usize (button, 50, 26);
	PACK_END (hbox, button, PACK_NONE);

	button = gtk_button_new_with_label ("OK");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
	                    GTK_SIGNAL_FUNC (pref_button), (void *) (PREF_APPLY |
	                    PREF_CLOSE));
	gtk_widget_set_usize (button, 50, 26);
	PACK_END (hbox, button, PACK_NONE);

	gtk_widget_show_all (pref_dialog);
}
