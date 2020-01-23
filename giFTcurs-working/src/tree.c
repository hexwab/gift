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
 * $Id: tree.c,v 1.15 2002/11/23 16:23:56 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "parse.h"
#include "tree.h"
#include "screen.h"

static void add_entries(list * flat, list * viflo)
{
	/* recursive helper function */
	int i;

	for (i = 0; i < viflo->num; i++) {
		rendering **r = viflo->entries[i];
		list *(*exp) (void *) = (*r)->expansion;

		list_append(flat, r);
		if (exp && exp(r))
			add_entries(flat, exp(r));
	}
}

void tree_flatten(tree * t)
{
	int i, offset;
	void *selected;

	selected = t->flat.num == 0 || t->flat.sel == -1 ? NULL : list_selected(&t->flat);
	offset = t->flat.sel - t->flat.start;

	list_remove_all(&t->flat);
	add_entries(&t->flat, &t->top);

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

/* Find the parent to the child item */
void *tree_find_parent(tree * t, void *item)
{
	int i;

	for (i = 0; i < t->top.num; i++) {
		rendering **r = (rendering **) list_index(&t->top, i);

		list *children = (*r)->expansion ? (*r)->expansion(r) : NULL;

		if (children && list_find(children, item) >= 0)
			return r;
	}
	return NULL;
}

void tree_initialize(tree * t)
{
	list_initialize(&t->flat);
	list_initialize(&t->top);
}

void list_destroy_all(list * l)
{
	int i;

	for (i = 0; i < l->num; i++) {
		rendering **r = (rendering **) l->entries[i];

		if (r && (*r)->destroy)
			(*r)->destroy(r);
	}
	list_remove_all(l);
}

void tree_destroy_all(tree * t)
{
	list_remove_all(&t->flat);
	list_destroy_all(&t->top);
}

void list_touch_all(list * l)
{
	int i;

	assert(l);

	for (i = 0; i < l->num; i++) {
		rendering **r = (rendering **) l->entries[i];

		if (r) {
			if ((*r)->touch)
				(*r)->touch(r);

			if ((*r)->expansion) {
				list *expansion = (*r)->expansion(r);

				if (expansion)
					list_touch_all(expansion);
			}
		}
	}
}

void tree_touch_all(tree * t)
{
	list_touch_all(&t->top);
}

void tree_sort(tree * t, CmpFunc cmp)
{
	list_sort(&t->top, cmp);
	tree_flatten(t);
}

void tree_select_top(tree * t, int i)
{
	tree_select_item(t, list_index(&t->top, i));
}

void tree_select_item(tree * t, void *item)
{
	t->flat.sel = list_find(&t->flat, item);
}

void tree_destroy_item(tree * t, void *item)
{
	int i = list_find(&t->top, item);

	list_remove_entry(&t->top, i);
	tree_flatten(t);
	if ((*(rendering **) item)->destroy)
		(*(rendering **) item)->destroy(item);
}

void tree_append(tree * t, void *item)
{
	list_append(&t->top, item);
}

void tree_prepend(tree * t, void *item)
{
	list_prepend(&t->top, item);
}

void tree_filter(tree * t, int (*keep) (void *))
{
	int i;
	list *l = &t->top;

	for (i = l->num - 1; i >= 0; i--) {
		rendering **item = l->entries[i];

		if (!keep(item)) {
			if ((*item)->destroy)
				(*item)->destroy(item);
			list_remove_entry(l, i);
		}
	}
	tree_flatten(t);
}
