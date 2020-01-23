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
 * $Id: tree.c,v 1.23 2003/05/10 19:47:43 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parse.h"
#include "tree.h"
#include "screen.h"
#include "format.h"

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

static void topology_change(void *parent)
{
	tree_node *t;

	for (t = parent; t && t->expanded; t = t->parent)
		t->dirty = 1;
}

void tree_flatten(tree * t)
{
	int i, offset;
	tree_node *selected;

	selected = t->flat.num == 0 || t->flat.sel == -1 ? NULL : list_selected(&t->flat);
	offset = t->flat.sel - t->flat.start;

	list_remove_all(&t->flat);
	add_entries(&t->flat, &t->tnode);

	t->tnode.dirty = 0;

	if (!selected)
		return;
	/* find the selected. */
	for (i = 0; i < t->flat.num; i++) {
		if (list_index(&t->flat, i) == selected) {
			t->flat.sel = i;
			t->flat.start = i - offset;
			return;
		}
	}
	/* the selected item was lost. */
}

void tree_initialize(tree * t)
{
	list_initialize(&t->flat);
	list_initialize(&t->tnode.children);
	list_sort(&t->tnode.children, (CmpFunc) compare_sortkey);
	t->tnode.expanded = 1;
}

void list_destroy_all(list * l)
{
	int i;

	for (i = 0; i < l->num; i++) {
		tree_node *node = list_index(l, i);

		if (node && node->methods->destroy)
			node->methods->destroy(node);
	}
	list_remove_all(l);
}

void tree_destroy_all(tree * t)
{
	list_remove_all(&t->flat);
	list_destroy_all(&t->tnode.children);
}

void tree_node_touch(void *a)
{
	tree_node *node = a;

	if (node->pretty) {
		g_free(node->pretty);
		node->pretty = NULL;
	}

	list_touch_all(&node->children);
}

void list_touch_all(list * l)
{
	int i;

	g_assert(l);

	for (i = 0; i < l->num; i++) {
		tree_node *node = list_index(l, i);

		if (node)
			tree_node_touch(node);
	}
}

void tree_touch_all(tree * t)
{
	list_touch_all(&t->tnode.children);
}

void tree_insort(void *parent, void *item)
{
	tree_node *t = parent;
	tree_node *node = item;

	/* the node is not in the tree */
	g_assert(node->pretty == NULL);

	node->parent = t;
	if (t->children.order == (CmpFunc) compare_sortkey) {
		tree *tr = (tree *) t;

		g_assert(tr->sort_order);
		g_assert(node->sortkey == NULL);
		make_sortkey(&node->sortkey, node, tr->sort_order, node->methods->getattr);
	}
	if (t->children.order)
		list_insort(&t->children, node);
	else
		list_append(&t->children, node);

	topology_change(t);
}

int tree_hold(void *item)
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

void tree_release(void *item, int index)
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
			make_sortkey(&node->sortkey, node, ((tree *) parent)->sort_order,
						 node->methods->getattr);
	if (changed)
		changed = list_resort(&parent->children, index);

	if (changed)
		topology_change(parent);

	tree_node_touch(node);

#ifndef NDEBUG
	parent->owner = NULL;
#endif
}

void tree_sort(tree * t, const char *sortstring)
{
	int i;

	t->sort_order = sortstring;
	/* recalculate the sort string for all items */
	for (i = 0; i < t->tnode.children.num; i++) {
		tree_node *node = list_index(&t->tnode.children, i);

		g_free(node->sortkey);
		node->sortkey = NULL;
		make_sortkey(&node->sortkey, node, sortstring, node->methods->getattr);
	}

	list_sort(&t->tnode.children, (CmpFunc) compare_sortkey);
	topology_change(t);
}

void tree_select_top(tree * t, int i)
{
	tree_select_item(t, list_index(&t->tnode.children, i));
}

void tree_select_item(tree * t, void *item)
{
	t->flat.sel = list_find(&t->flat, item);
}

void tree_destroy_item(tree * t, void *item)
{
	int i = list_find(&t->tnode.children, item);
	tree_node *node = item;

	list_remove_entry(&t->tnode.children, i);
	if (node->methods->destroy)
		node->methods->destroy(node);
	topology_change(t);
}

void tree_filter(tree * t, int (*keep) (void *))
{
	int i;
	list *l = tree_children(t);

	for (i = l->num - 1; i >= 0; i--) {
		tree_node *item = list_index(l, i);

		if (!keep(item)) {
			if (item->methods->destroy)
				item->methods->destroy(item);
			list_remove_entry(l, i);
		}
	}
	topology_change(t);
}

void tree_node_destroy(void *a)
{
	tree_node *node = a;

	g_free(node->pretty);
	g_free(node->sortkey);
	g_free(node);
}

void tree_remove(void *item)
{
	int i;
	tree_node *child = item;
	tree_node *parent = child->parent;

	g_assert(parent);
	i = list_find(&parent->children, child);
	g_assert(i >= 0);
	list_remove_entry(&parent->children, i);
	topology_change(parent);
}
