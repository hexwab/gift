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
 * $Id: list.c,v 1.83 2003/06/27 11:20:13 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"

static int scrolloff = 0;
static void list_resize(dynarray * snafu, int nmemb);

/* dynarray funcs - similar to glib's */
void dynarray_append(dynarray * arry, void *item)
{
	if (arry->allocated <= arry->num)
		list_resize(arry, 1 + arry->num * 2);

	arry->entries[arry->num++] = item;
}

void dynarray_remove(dynarray * arry, void *item)
{
	int i = dynarray_find(arry, item);

	if (i >= 0)
		dynarray_remove_index(arry, i);
}

void dynarray_remove_index(dynarray * arry, int idx)
{
	g_assert(0 <= idx && idx < arry->num);
	if (arry->num == 1) {
		dynarray_removeall(arry);
	} else {
		void **base = arry->entries + idx;

		memmove(base, base + 1, (--arry->num - idx) * sizeof base[0]);
		if (arry->allocated > 4 * arry->num)
			/* a quarter is used, shrink the size to the half */
			list_resize(arry, 2 * arry->num);
	}
}

void dynarray_remove_index_fast(dynarray * arry, int idx)
{
	g_assert(0 <= idx && idx < arry->num);
	if (arry->num == 1) {
		dynarray_removeall(arry);
	} else {
		arry->entries[idx] = arry->entries[--arry->num];

		if (arry->allocated > 4 * arry->num)
			/* a quarter is used, shrink the size to the half */
			list_resize(arry, 2 * arry->num);
	}
}

void dynarray_remove_fast(dynarray * arry, void *item)
{
	int i = dynarray_find(arry, item);

	if (i >= 0)
		dynarray_remove_index_fast(arry, i);
}

void dynarray_foreach(const dynarray * arry, LFunc func)
{
	int i;

	for (i = 0; i < arry->num; i++)
		func(arry->entries[i]);
}

void dynarray_removeall(dynarray * arry)
{
	g_free(arry->entries);
	arry->entries = NULL;
	arry->num = arry->allocated = 0;
}

int dynarray_find(const dynarray * arry, const void *elem)
{
	int i;

	for (i = 0; i < arry->num; i++)
		if (arry->entries[i] == elem)
			return i;
	return -1;
}

void list_initialize(list * snafu)
{
	memset(snafu, 0, sizeof(list));
	snafu->sel = -1;
}

void list_remove_all(list * snafu)
{
	dynarray_removeall((dynarray *) snafu);
	snafu->start = 0;
	snafu->sel = -1;
}

void list_free_entries(list * snafu)
{
	dynarray_foreach((dynarray *) snafu, g_free);
	list_remove_all(snafu);
}

void list_append(list * snafu, void *entry)
{
	g_assert(!snafu->order);
	dynarray_append((dynarray *) snafu, entry);
}

void list_insert(list * snafu, void *entry, int pos)
{
	g_assert(!snafu->order);
	if (snafu->allocated <= snafu->num)
		list_resize((dynarray *) snafu, 1 + snafu->num * 2);

	memmove(snafu->entries + pos + 1, snafu->entries + pos, (snafu->num - pos) * sizeof(void *));
	snafu->num++;
	snafu->entries[pos] = entry;
}

void list_remove_entry(list * snafu, int idx)
{
	dynarray_remove_index((dynarray *) snafu, idx);
}

static void list_resize(dynarray * snafu, int nmemb)
{
	snafu->entries = g_renew(void *, snafu->entries, nmemb);

	snafu->allocated = nmemb;
}

void list_check_values(list * snafu, int h)
{
	int soff;

	/* Do sanity check on values */
	if (snafu->start < 0)
		snafu->start = 0;
	if (snafu->start > snafu->num - 1)
		snafu->start = snafu->num - 1;
	if (snafu->sel < 0)
		snafu->sel = 0;
	if (snafu->sel > snafu->num - 1)
		snafu->sel = snafu->num - 1;

	soff = scrolloff;

	/* Check against list frame */
	if (h <= soff * 2)
		soff = (h - 1) / 2;
	/* check against bottom */
	if (snafu->start < snafu->sel - h + 1 + soff)
		snafu->start = snafu->sel - h + 1 + soff;
	if (snafu->start > snafu->num - h)
		snafu->start = snafu->num - h;
	/* check against top */
	if (snafu->start > snafu->sel - soff)
		snafu->start = snafu->sel - soff;
	if (snafu->start < 0)
		snafu->start = 0;
}

/* This is for list that don't have a selector */
void list_check_values_simple(list * snafu, int h)
{
	/* Do sanity check on values */
	if (snafu->start > snafu->num - h)
		snafu->start = snafu->num - h;
	if (snafu->start < 0)
		snafu->start = 0;
}

static void *list_member(list * snafu, int i)
{
	if (!snafu->num)
		return NULL;

	g_assert(0 <= i && i < snafu->num);

	return snafu->entries[i];
}

void *list_selected(list * snafu)
{
	return list_member(snafu, snafu->sel);
}

void *below_selected(list * snafu)
{
	return list_member(snafu, snafu->sel + 1);
}

/* This finds out where 'item' should be inserted to keep list in order
 * It keeps track of lower and upper bound, always halving the possible range.
 * Therefore it's O(log n) */
static int sortpos(void **l, CmpFunc cmp, const void *item, int lower, int upper)
{
	while (upper != lower) {
		int mid, res;
		void *item2;

		g_assert(lower < upper);

		mid = (upper + lower) / 2;
		item2 = l[mid];

		if (item == item2)
			return mid;

		res = cmp(item, l[mid]);

		if (res < 0)
			upper = mid;
		else if (res > 0)
			lower = mid + 1;
		else
			return mid;
	}
	return lower;
}

/* Call this if an entry has changed and needs to be resorted. */
/* Returns 1 if the position changes */
int list_resort(list * l, int pos)
{
	void *item = l->entries[pos];
	CmpFunc cmp = l->order;

	if (!cmp)
		return 0;

	if (pos > 0 && cmp(l->entries[pos - 1], item) > 0) {
		/* sort item towards lower index */
		int newpos = sortpos(l->entries, cmp, item, 0, pos);

		memmove(l->entries + newpos + 1, l->entries + newpos, (pos - newpos) * sizeof(void *));
		l->entries[newpos] = item;

		if (l->sel >= 0) {
			/* keep track of selected item */
			if (l->sel == pos)
				l->sel = newpos;
			else if (newpos <= l->sel && l->sel < pos)
				l->sel++;
		}
	} else if (pos < l->num - 1 && cmp(item, l->entries[pos + 1]) > 0) {
		/* sort item towards higher index */
		int newpos = sortpos(l->entries, cmp, item, pos + 1, l->num) - 1;

		memmove(l->entries + pos, l->entries + pos + 1, (newpos - pos) * sizeof(void *));
		l->entries[newpos] = item;

		if (l->sel >= 0) {
			/* keep track of selected item */
			if (l->sel == pos)
				l->sel = newpos;
			else if (pos < l->sel && l->sel <= newpos)
				l->sel--;
		}
	} else {
		return 0;
	}
	return 1;
}

static void my_qsort(void **base, void **last, CmpFunc cmp)
{
	void **lo, **hi, *pivot;

	if (last <= base)
		return;

	lo = base + (last - base) / 2;
	pivot = *lo;
	*lo = *last;
	hi = last;
	lo = base - 1;
	for (;;) {
		do
			if (++lo == hi)
				goto done;
		while (cmp(*lo, pivot) <= 0);
		*hi = *lo;
		do
			if (lo == --hi)
				goto done;
		while (cmp(pivot, *hi) <= 0);
		*lo = *hi;
	}
  done:
	*lo = pivot;
	my_qsort(lo + 1, last, cmp);
	my_qsort(base, lo - 1, cmp);
}

/* Sorts a list and keeps it sorted. If sorting should be abandoned,
 * call with cmp == NULL */
void list_sort(list * snafu, CmpFunc cmp)
{
	void *sel = NULL;

	snafu->order = cmp;
	if (!cmp)
		return;
	if (snafu->num < 2)
		return;
	if (snafu->sel >= 0)
		sel = list_selected(snafu);

	my_qsort(snafu->entries, snafu->entries + snafu->num - 1, cmp);

	/* find the selected item again */
	if (sel)
		snafu->sel = list_find(snafu, sel);
}

/* inserts an item into a sorted list at the right place. */
void list_insort(list * snafu, void *item)
{
	if (snafu->allocated <= snafu->num)
		list_resize((dynarray *) snafu, 1 + snafu->num * 2);

	snafu->entries[snafu->num++] = item;

	list_resort(snafu, snafu->num - 1);
}

void list_foreach(list * snafu, LFunc func)
{
	dynarray_foreach((dynarray *) snafu, func);
}

int list_filter(list * snafu, LFilter keep, void *udata, void (*destroy) (void *))
{
	int i, j, count = 0;

	for (j = i = 0; i < snafu->num; i++)
		if (keep(snafu->entries[i], udata))
			snafu->entries[j++] = snafu->entries[i];
		else
			destroy(snafu->entries[i]), count++;
	snafu->num = j;

	return count;
}

int list_find(list * snafu, const void *elem)
{
	int i;
	CmpFunc cmp = snafu->order;

	if (!cmp)
		return dynarray_find((dynarray *) snafu, elem);

	i = sortpos(snafu->entries, cmp, elem, 0, snafu->num);

	if (i < snafu->num && cmp(elem, snafu->entries[i]) == 0)
		return i;
	return -1;
}

int compare_pointers(const void *a, const void *b)
{
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

void list_set_scrolloff(int val)
{
	scrolloff = val;
}
