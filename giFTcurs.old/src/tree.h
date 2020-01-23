/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: tree.h,v 1.16 2003/05/14 09:09:35 chnix Exp $
 */
#ifndef _TREE_H
#define _TREE_H

#include "list.h"
#include "format.h"

/* The virtual functions all tree nodes must have. */
typedef struct {
	char *(*pretty_line) (void *);	/* return the line to be drawn */
	void (*destroy) (void *);	/* free all data used by this */
	getattrF getattr;
} rendering;

/* A tree is made up of a list of tree_node structs implementing the above
 * of that item. For displaying purposes, an 'flat' list is also
 * carried around. It can be updated to reflect the tree with the
 * function 'tree_flatten'
 */
typedef struct {
	const rendering *methods;	/* the methods, see above */
	char *pretty;				/* formatted line to be displayed */
	char *sortkey;				/* key to sort after */
	void *parent;				/* the tree_node containing this item */

	/* FIXME: these items should be omitted for leaves */
	unsigned expanded:1;		/* display children? */
	unsigned dirty:1;			/* have topology or sorting changed? */
	list children;				/* list of tree_node children */
#ifndef NDEBUG
	void *owner;				/* detects unmatched tree_hold/release */
#endif
} tree_node;

/* A tree is made up of a tree_node plus some extra stuff:
 * - A additional 'flat' list, that is displayed on screen.
 *   Should be updated with 'tree_flatten' if the tree is dirty
 * - update_ui is only used in ui_transfer.c
 * - sort_order is the format string that rule the sortkey generation
 */
typedef struct {
	tree_node tnode;
	list flat;
	void (*update_ui) (void);
	const char *sort_order;
} tree;

/* Call this whenever tree topology has changed. */
void tree_flatten(tree * t);

void tree_initialize(tree * t);
void tree_destroy_all(tree * t);
void list_destroy_all(list * t);
void tree_node_touch(void *item);
void tree_touch_all(tree * t);
void list_touch_all(list * t);
void tree_sort(tree * t, const char *sortstring);
void tree_select_top(tree * t, int i);
void tree_select_item(tree * t, void *item);
void tree_destroy_item(tree * t, void *item);
void tree_filter(tree * t, int (*keep) (void *));

void tree_touch_item(tree * t, void *item, int index);
void tree_node_destroy(void *);

/* Important. When members a tree_node that affect sorting is changed,
 * that part of code must be surrounded by these guards. Example:
 *     int pos = tree_hold(current_transfer);
 *     ... code that modifies current_transfer->filesize ...
 *     tree_release(current_transfer, pos);
 * the item is resorted after the call to tree_release */
int tree_hold(void *);
void tree_release(void *, int key);

void tree_remove(void *item);
void tree_insort(void *parent, void *child);

/* virtual methods helper macros */
#define IS_A(expr, type) (((tree_node*)(expr))->methods == &type##_methods)
#define NEW_NODE(type) __extension__(({ type *t = calloc(1, sizeof(type)); ((tree_node*)t)->methods = &type##_methods; t; }))

/* useful defines */
#define tree_children(t) (&((tree_node*)t)->children)
#define tree_parent(t) (((tree_node*)t)->parent)
#define tree_isempty(t) (!tree_children(t)->num)

#endif
