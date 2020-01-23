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
 * $Id: tree.c,v 1.26 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tree.h"

static void tree_node_touch(tree_node *);

static int compare_sortkey(const tree_node * a, const tree_node * b)
{
	g_assert(a->sortkey);
	g_assert(b->sortkey);

	if (a == b || a->sortkey == b->sortkey)
		return 0;

	/* every sortkey is guaranteed to be unique, so we don't limit this */
	return memcmp(a->sortkey, b->sortkey, 999);
}

static void add_entries(list * flat, tree_node * node)
{
	/* recursive helper function */
	int i;

	if (!node->expanded)
		return;
	for (i = 0; i < node->children.num; i++) {
		tree_node *nod = list_index(&node->children, i);

		list_append(flat, nod);
		add_entries(flat, nod);
	}
}

static void topology_change(tree_node * t)
{
	for (; t && t->expanded; t = t->parent)
		t->dirty = 1;
}

void tree_flatten(tree_ptr ptr)
{
	int i, offset;
	void *selected;
	list *flat = tree_flat(ptr);
	tree_node *t = ptr;

	selected = flat->num == 0 || flat->sel == -1 ? NULL : list_selected(flat);
	offset = flat->sel - flat->start;

	list_remove_all(flat);
	add_entries(flat, t);

	t->dirty = 0;

	if (!selected)
		return;
	/* find the selected. */
	for (i = 0; i < flat->num; i++) {
		if (list_index(flat, i) == selected) {
			flat->sel = i;
			flat->start = i - offset;
			return;
		}
	}
	/* the selected item was lost. */
}

void tree_initialize(node_ptr t)
{
	list_initialize(tree_flat(t));
	list_initialize(tree_children(t));
	list_sort(tree_children(t), (CmpFunc) compare_sortkey);
	((tree_node *) t)->expanded = 1;
}

void tree_destroy_children(node_ptr t)
{
	int i;
	list *l = tree_children(t);

	for (i = 0; i < l->num; i++) {
		tree_node *node = list_index(l, i);

		if (node && node->klass->destroy)
			node->klass->destroy(node);
	}
	list_remove_all(l);
}

void tree_destroy_all(tree_ptr t)
{
	list_remove_all(tree_flat(t));
	tree_destroy_children(t);
}

void tree_node_touch(tree_node * node)
{
	if (node->pretty) {
		g_free(node->pretty);
		node->pretty = NULL;
	}

	tree_touch_all(node);
}

void tree_touch_all(tree_ptr t)
{
	list *l = tree_children(t);
	int i;

	g_assert(l);

	for (i = 0; i < l->num; i++) {
		tree_node *node = list_index(l, i);

		if (node)
			tree_node_touch(node);
	}
}

void tree_insort(node_ptr parent, leaf_ptr item)
{
	tree_node *t = parent;
	tree_node *node = item;

	/* Check that the node is not in a tree already */
	g_assert(node->pretty == NULL);

	node->parent = t;
	if (t->children.order == (CmpFunc) compare_sortkey) {
		const char *sort_order = t->klass->get_sort_order(t);

		g_assert(sort_order);
		g_assert(node->sortkey == NULL);
		make_sortkey(&node->sortkey, node, sort_order, node->klass->getattr);
	}
	if (t->children.order)
		list_insort(&t->children, node);
	else
		list_append(&t->children, node);

	topology_change(t);
}

int tree_hold(leaf_ptr item)
{
	int i;
	tree_node *node = item;
	tree_node *parent = node->parent;

	g_assert(parent);

#ifndef NDEBUG
	g_assert(parent->owner == NULL);
	parent->owner = item;
#endif

	i = list_find(&parent->children, item);

	g_assert(i >= 0);

	return i;
}

void tree_release(leaf_ptr item, int index)
{
	int changed = 1;
	tree_node *node = item;
	tree_node *parent = node->parent;

#ifndef NDEBUG
	g_assert(index >= 0);
	g_assert(parent->children.entries[index] == node);
	g_assert(parent->owner == node);
#endif

	if (parent->children.order == (CmpFunc) compare_sortkey)
		changed =
			make_sortkey(&node->sortkey, node, parent->klass->get_sort_order(parent),
						 node->klass->getattr);
	if (changed)
		changed = list_resort(&parent->children, index);

	if (changed)
		topology_change(parent);

	tree_node_touch(node);

#ifndef NDEBUG
	parent->owner = NULL;
#endif
}

void tree_resort(node_ptr item)
{
	int i;
	tree_node *t = item;
	const char *sortstring = t->klass->get_sort_order(t);

	/* recalculate the sort string for all items */
	for (i = 0; i < t->children.num; i++) {
		tree_node *node = list_index(&t->children, i);

		g_free(node->sortkey);
		node->sortkey = NULL;
		make_sortkey(&node->sortkey, node, sortstring, node->klass->getattr);
	}

	list_sort(&t->children, (CmpFunc) compare_sortkey);
	topology_change(t);
}

void tree_select_top(tree_ptr t, int i)
{
	tree_select_item(t, list_index(tree_children(t), i));
}

void tree_select_item(tree_ptr t, leaf_ptr item)
{
	tree_flat(t)->sel = list_find(tree_flat(t), item);
}

void tree_filter(node_ptr t, gboolean(*keep) (leaf_ptr))
{
	int i;
	list *l = tree_children(t);

	for (i = l->num - 1; i >= 0; i--) {
		tree_node *item = list_index(l, i);

		if (!keep(item)) {
			if (item->klass->destroy)
				item->klass->destroy(item);
			list_remove_entry(l, i);
		}
	}
	topology_change(t);
}

void tree_node_destroy(leaf_ptr a)
{
	tree_node *node = a;

	g_free(node->pretty);
	g_free(node->sortkey);
	g_free(node);
}

void tree_unlink(leaf_ptr item)
{
	int i;
	tree_node *child = item;
	tree_node *parent = child->parent;

	g_assert(parent);
	i = list_find(&parent->children, child);
	g_assert(i >= 0);
	list_remove_entry(&parent->children, i);
	topology_change(parent);

	child->parent = NULL;
}
