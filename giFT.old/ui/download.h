/*
 * download.h
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

#ifndef __DOWNLOAD_H
#define __DOWNLOAD_H

/**************************************************************************/

#include "transfer.h"

/**************************************************************************/

Transfer *download_insert  (GtkWidget *dl_list, GtkCTreeNode *node_hint,
                            SharedFile *shr);
void download_start        (GtkWidget *dl_list);

int  download_response     (char *head, int keys, GData *dataset,
                            DaemonEvent *event);

int  menu_popup_download   (GtkWidget *dl_list, Transfer *download,
                            guint button, guint32 at);

/**************************************************************************/

#endif /* __DOWNLOAD_H */
