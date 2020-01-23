/*
 * fe_stats.c
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

#include "fe_daemon.h"
#include "fe_stats.h"

/* 2 Pixmaps, one for each protocol atm */
#include "local.xpm"
#include "network.xpm"
static GtkWidget *network = NULL;
static GtkWidget *local = NULL;

/*****************************************************************************/

static int stats_response (char *head, int keys, GData *dataset,
						   DaemonEvent *event)
{
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	GtkWidget *clist;
	int row;
	char *text[6];

	clist = event->data;

	if (!head || !dataset)
	{
		gtk_clist_thaw (GTK_CLIST (clist));
		return FALSE;
	}

	if (!strcmp (head, "event"))
	{
		gtk_clist_freeze (GTK_CLIST (clist));
		gtk_clist_clear (GTK_CLIST (clist));

		return TRUE;
	}

	text[0] = g_datalist_get_data (&dataset, "protocol");
	text[1] = g_datalist_get_data (&dataset, "status");
	text[2] = g_datalist_get_data (&dataset, "users");
	text[3] = g_datalist_get_data (&dataset, "files");
	text[4] = format_size_disp ('G', ATOI (g_datalist_get_data (&dataset, "size")));
	text[5] = NULL;
	row = gtk_clist_append (GTK_CLIST (clist), text);
	if (text[0] && !strcmp(text[0], "local"))
		gtk_pixmap_get (GTK_PIXMAP (local), &pixmap, &bitmap);
	else
		gtk_pixmap_get (GTK_PIXMAP (network), &pixmap, &bitmap);
	gtk_clist_set_pixtext(GTK_CLIST (clist), row, 0, text[0], 5, pixmap, bitmap);
	g_free (text[4]);
	return TRUE;
}

int stats_populate (GtkWidget *clist)
{
	if (!local)
	{
		GdkPixmap *pixmap;
		GdkBitmap *bitmap;
		GdkColormap *colormap;

		/* create pixmaps */
		colormap = gtk_widget_get_colormap (GTK_WIDGET (clist));
		pixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL, colormap,
                                                      &bitmap, NULL, local_xpm);
		local = gtk_pixmap_new(pixmap, bitmap);
		pixmap = gdk_pixmap_colormap_create_from_xpm_d(NULL, colormap,
                                                      &bitmap, NULL, network_xpm);
		network = gtk_pixmap_new(pixmap, bitmap);
	}
	return daemon_request ((ParsePacketFunc) stats_response, NULL, clist,
	                      "<stats/>\n");
}

int stats_timer_populate (FTApp *ftapp)
{
	stats_populate (ftapp->stats_list);
	return TRUE;
}
