/*
 * fe_search.h
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

#ifndef __FE_SEARCH_H
#define __FE_SEARCH_H

/**************************************************************************/

#include "fe_share.h"
#include "fe_obj.h"

/**************************************************************************/

/*
 * search result (event-based object)
 */
typedef struct
{
	unsigned long id;
	ObjectData obj_data;

	SharedFile shr;    /* shared file information */
	int complete;
	int availability;
	int results;
} Search;

#define Search_Type 0

/**************************************************************************/

int search_execute			(FTApp *ft, char *search, char *media, char *type);

void srch_list_sort_column	(GtkWidget *w, int column, void *sort_col);

int menu_popup_search		(GtkWidget *srch_list, Search *srch, guint button,
					   		 guint32 at);

void menu_search_hash		(GtkWidget *menu, Search *srch);

void search_free			(Search *obj);

/**************************************************************************/

#endif /* __FE_SEARCH_H */
