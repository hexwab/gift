/*
 * fe_transfer.c
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

#include "fe_share.h"
#include "fe_connect.h"
#include "fe_transfer.h"

unsigned long transfer_get_rate (Transfer *tr)
{
	unsigned long rate = 0;
	GList *walker = tr->transfer_rate;

	if (g_list_length (tr->transfer_rate) == 0)
		return 0;
	while (walker)
	{
		rate += (int) walker->data;
		walker = walker->next;
	}
	return rate / g_list_length (tr->transfer_rate);
}

/*****************************************************************************/
/* Gets pixmap according to mimetype of a transfer,
 * gets the colormap from the tree object of the transfer
 */

int transfer_get_pixmap (Transfer* transfer, GdkPixmap **pixmap,
						 GdkBitmap **bitmap)
{
	GdkColormap *colormap;
	GtkCTree *tree;

	tree = obj_get_data (OBJ_DATA (transfer), "tree");
	colormap = gtk_widget_get_colormap (GTK_WIDGET (tree));
	return share_get_pixmap (&(transfer->shr), colormap, pixmap, bitmap);
}

/*****************************************************************************/

void fe_transfer_free (Transfer *obj)
{
	fe_share_free (SHARE (obj));
	g_list_free (obj->transfer_rate);
	free (obj);
}
