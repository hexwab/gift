/*
 * fe_utils.h
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

#ifndef __FE_UTILS_H
#define __FE_UTILS_H

#include "fe_obj.h"

/**************************************************************************/

#define LIST_SIZE(list) (sizeof (list) / sizeof (list[0]))

/* data helpers */
#define SET_ROW_DATA(clist,row,data) gtk_clist_set_row_data_full \
									 (GTK_CLIST (clist), row, data, \
									 (GtkDestroyNotify) obj_destroy_notify)
#define SET_NODE_DATA(ctree,node,data) gtk_ctree_node_set_row_data_full \
									   (GTK_CTREE (ctree), node, data, \
									   (GtkDestroyNotify) obj_destroy_notify)

#define SET_PARENT(widget,ftapp) gtk_object_set_data (GTK_OBJECT (widget), \
													 "ft_app", ftapp)

/* get text at sort column of the given GtkCListRow ... */
#define ROW_GET_TEXT(clist,row) GTK_CELL_TEXT(((GtkCListRow *)row)->cell[ \
								GTK_CLIST(clist)->sort_column])->text

/**************************************************************************/

char *calculate_transfer_info	(size_t transmit, size_t total, size_t diff);

char *format_size_disp			(char start_unit, size_t size);
char *format_user_disp			(char *user);
char *format_href_disp			(char *href);
int   oct_value_from_hex		(char hex_char);

GtkCTreeNode *find_share_node	(char *hash, char *user, unsigned long size,
							     GtkWidget *tree, GtkCTreeNode *node);

int   children_length		    (GtkCTreeNode *parent);

GtkWidget *ft_scrolled_window	(GtkWidget *widget, GtkPolicyType hpolicy,
								 GtkPolicyType vpolicy);

void   generic_column_sort      (GtkWidget *w, int column, char *sort_info);

/**************************************************************************/

#endif /* __FE_UTILS_H */
