/*
 * fe_ui_utils.h
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

#ifndef __FE_UI_UTILS_H
#define __FE_UI_UTILS_H

#include "fe_transfer.h"

/*****************************************************************************/

int gift_message_box		(char *title, char *message);
int ctree_node_child_length	(GtkWidget *tree, GtkCTreeNode *node);
void with_ctree_selection	(GtkCTree *ctree, void function(Transfer *));

/*****************************************************************************/

#endif /* __FE_UI_UTILS_H */
