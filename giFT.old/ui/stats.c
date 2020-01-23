/*
 * stats.c
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

#include "daemon.h"
#include "stats.h"

/*****************************************************************************/

static int stats_response (char *head, int keys, GData *dataset,
						   DaemonEvent *event)
{
	GtkWidget *clist;
	char *text[6];

	clist = event->data;

	if (!head || !dataset)
	{
		gtk_clist_thaw (GTK_CLIST (clist));
		return FALSE;
	}

	if (!strcmp (head, "event"))
		return TRUE;

	text[0] = g_datalist_get_data (&dataset, "protocol");

	if (!strcmp (text[0], "0.0.0.0"))
		text[0] = "local";

	text[1] = g_datalist_get_data (&dataset, "status");
	text[2] = g_datalist_get_data (&dataset, "users");
	text[3] = g_datalist_get_data (&dataset, "files");
	text[4] = format_size_disp ('G', ATOI (g_datalist_get_data (&dataset, "size")));
	text[5] = NULL;

	gtk_clist_append (GTK_CLIST (clist), text);

	return TRUE;
}

int stats_populate (GtkWidget *clist)
{
	gtk_clist_freeze (GTK_CLIST (clist));
	gtk_clist_clear (GTK_CLIST (clist));

	if (!(daemon_request ((ParsePacketFunc) stats_response, NULL, clist,
	                      "<stats/>\n")))
		return FALSE;

	return TRUE;
}

int stats_timer_populate (FTApp *ftapp)
{
	stats_populate (ftapp->stats_list);

	return TRUE;
}
