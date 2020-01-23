/*
 * ui_utils.c
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

#include "ui_utils.h"

/*****************************************************************************/

static void dialog_close (GtkWidget *button, GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

/* TODO - modal */
int gift_message_box (char *title, char *message)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *ok_button;
	char      *message0;
	char      *message_dup;
	size_t     message_len;
	size_t     divide_len;

	dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (dialog), title);

	vbox = GTK_DIALOG (dialog)->vbox;

	gtk_box_set_spacing (GTK_BOX (vbox), 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	/* multi-line message */
	message_len = strlen (message);
	message0 = message_dup = STRDUP (message);

	/* calculate the character length of each line evenly */
	divide_len = message_len / (int) (((float)message_len / 100.0)+0.99999);
	printf ("%i\n", divide_len);

	/* actually divide the string */
	while ((message_dup - message0) < message_len)
	{
		size_t write_len = message_len - (message_dup - message0);

		if (write_len > divide_len)
			write_len = divide_len;

		while (message_dup[write_len] && !isspace (message_dup[write_len]))
			write_len++;

		message_dup[write_len] = '\n';

		message_dup += write_len + 1;
	}

	label = gtk_label_new (message0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	free (message0);

	ok_button = gtk_button_new_with_label ("OK");
	gtk_widget_set_usize (ok_button, 50, 26);
	gtk_signal_connect (GTK_OBJECT (ok_button), "clicked",
						GTK_SIGNAL_FUNC (dialog_close), dialog);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
						ok_button, FALSE, FALSE, 0);

	gtk_widget_show_all (dialog);

	return TRUE;
}

/*****************************************************************************/

static void calc_node_len (GtkCTree *ctree, GtkCTreeNode *node, int *len)
{
	(*len)++;
}

int ctree_node_child_length (GtkWidget *ctree, GtkCTreeNode *node)
{
	int len = 0;

	gtk_ctree_post_recursive (GTK_CTREE (ctree), node,
	                          (GtkCTreeFunc) calc_node_len, &len);

	/* post_recursive is broken, it counts the parent you supply, but only
	 * iterates the children */
	if (len)
		len--;

	return len;
}
