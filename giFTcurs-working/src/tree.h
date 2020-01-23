/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: tree.h,v 1.10 2002/11/23 16:23:56 chnix Exp $
 */
#ifndef _TREE_H
#define _TREE_H

#include "list.h"

/* The virtual functions all tree items must implement. The first thing
 * in such a struct should be a pointer its virtual functions */

typedef struct {
	char *(*pretty_line) (void *);	/* return the line to be drawn */
	list *(*expansion) (void *);	/* return subtree or NULL */
	void (*destroy) (void *);	/* free all data used by this */
	void (*touch) (void *);		/* mark this as changed */
} rendering;

/* A tree is made up of a list of structs implementing the above
 * 'rendering' methods. the 'expansion' method returns the subtree
 * of that item. For displaying purposes, an 'flat' list is also
 * carried around. It can be updated to reflect the tree with the
 * function 'tree_flatten'
 */
typedef struct {
	list top;
	list flat;
	void (*update_ui) (void);
} tree;

#define tree_isempty(t) (!(t)->top.num)
#define TREE_INITIALIZER { LIST_INITIALIZER, LIST_INITIALIZER, NULL }

/* Call this whenever tree topology has changed. */
void tree_flatten(tree * t);

void *tree_find_parent(tree * t, void *item);

void tree_initialize(tree * t);
void tree_destroy_all(tree * t);
void list_destroy_all(list * t);
void tree_touch_all(tree * t);
void list_touch_all(list * t);
void tree_sort(tree * t, CmpFunc cmp);
void tree_select_top(tree * t, int i);
void tree_select_item(tree * t, void *item);
void tree_destroy_item(tree * t, void *item);
void tree_append(tree * t, void *item);
void tree_prepend(tree * t, void *item);
void tree_filter(tree * t, int (*keep) (void *));

#endif
