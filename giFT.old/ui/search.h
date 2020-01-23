/*
 * search.h
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

#ifndef __SEARCH_H
#define __SEARCH_H

/**************************************************************************/

#include "share.h"
#include "obj.h"

/**************************************************************************/

/*
 * search result (event-based object)
 */
typedef struct {
	unsigned long id;

	ObjectData obj_data;

	SharedFile shr;    /* shared file information */
} Search;

#define Search_Type 0

/**************************************************************************/

int search_execute (FTApp *ft, char *search, char *media);

void srch_list_sort_column (GtkWidget *w, int column, void *sort_col);

int menu_popup_search (GtkWidget *srch_list, Search *srch, guint button,
					   guint32 at);

void search_free (Search *obj);

/**************************************************************************/

#endif /* __SEARCH_H */
