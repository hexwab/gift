/*
 * fe_upload.h
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

#ifndef __FE_UPLOAD_H
#define __FE_UPLOAD_H

/**************************************************************************/

#include "fe_transfer.h"
#include "fe_search.h"

/**************************************************************************/

Transfer *upload_insert     (GtkWidget *ul_list, GData *ft_data);
int       upload_response   (char *head, int keys, GData *dataset,
                             DaemonEvent *event);
int       menu_popup_upload (GtkWidget *ul_list, Transfer *upload,
                             guint button, guint32 at);
void	  menu_search_user  (GtkWidget *menu, Transfer *transfer);

/**************************************************************************/

#endif /* __FE_UPLOAD_H */
