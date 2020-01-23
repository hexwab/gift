/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * $Id: tree.h,v 1.21 2003/07/22 23:58:37 weinholt Exp $
 */
#ifndef _TREE_H
#define _TREE_H

#include "list.h"
#include "format.h"

/* The virtual functions all tree nodes must have. */
typedef struct {
	/* return the line to be drawn */
	char *(*pretty_line) (void *);

	/* free all data used by this node */
	void (*destroy) (void *);

	/* How to get attributes from this type of data */
	getattrF getattr;

	/* should return a comma-separated list of attributes, which is
	 * passed to getattr() to create the sortkey.
	 * That sortkey is used when sorting the list.
	 */
	const char *(*get_sort_order) (void *);
} tree_class_t;

/* A tree node consists of a pointer to the parent and a list of
 * children. If you need more elements than in this struct,
 * make another struct with a 'tree_node' as the first element.
 */
typedef struct {
	const tree_class_t *klass;	/* the methods, see above */
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
 * - An additional 'flat' list, that is displayed on screen.
 *   Should be updated with 'tree_flatten' if the tree is dirty.
 */
typedef struct {
	tree_node tnode;
	list flat;
} tree;

/* Note. Most functions take a 'void *' as parameter, as they probably
 * are called with subclasses of tree or tree_node. These typedefs are 
 * just symbolic names that tells what value is expected. */
typedef void *tree_ptr, *node_ptr, *leaf_ptr;

/* Call this whenever tree topology has changed. */
void tree_flatten(tree_ptr);

void tree_initialize(node_ptr);
void tree_destroy_children(node_ptr);
void tree_destroy_all(tree_ptr);	/* also destroys 'flat' list */
void tree_touch_all(tree_ptr);
void tree_resort(node_ptr);
void tree_select_top(tree_ptr, int i);
void tree_select_item(tree_ptr, leaf_ptr item);
void tree_filter(node_ptr, gboolean (*keep) (leaf_ptr));

void tree_node_destroy(node_ptr);

/* Important. When members of a tree_node that affect sorting is changed,
 * that part of code must be surrounded by these guards. Example:
 *     int pos = tree_hold(current_transfer);
 *     ... code that modifies current_transfer->filesize ...
 *     tree_release(current_transfer, pos);
 * the item is resorted after the call to tree_release. */
int tree_hold(node_ptr);
void tree_release(node_ptr, int key);

void tree_unlink(leaf_ptr);
void tree_insort(node_ptr, leaf_ptr child);

/* virtual methods helper macros */
#define CLASSOF(expr) (((tree_node*)(expr))->klass)
#define INSTANCEOF(expr, type) (CLASSOF(expr) == &type##_class)
#define NEW_NODE(type) G_GNUC_EXTENSION(({ type *_t = g_new0(type, 1); ((tree_node*)_t)->klass = &type##_class; _t; }))

/* useful defines */
#define tree_children(t) (&((tree_node*)(t))->children)
#define tree_parent(t) (((tree_node*)(t))->parent)
#define tree_isempty(t) (!tree_children(t)->num)
#define tree_flat(t) (&((tree*)(t))->flat)

#endif
