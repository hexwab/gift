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
 * $Id: list.c,v 1.67 2002/11/10 12:58:04 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "parse.h"
#include "list.h"
#include "screen.h"
#include "settings.h"

#ifdef TEST_LISTS
# undef FATAL
# define FATAL(x...) fprintf(stderr, x);
#endif

static void list_resize(list * snafu, int nmemb);

void list_initialize(list * snafu)
{
	memset(snafu, 0, sizeof(list));
	snafu->sel = -1;
}

list *list_new(void)
{
	list *foo = malloc(sizeof(list));

	if (!foo)
		FATAL("malloc");
	list_initialize(foo);
	return foo;
}

void list_remove_all(list * snafu)
{
	list_resize(snafu, 0);
	snafu->num = 0;
	snafu->start = 0;
	snafu->sel = -1;
}

void list_free_entries(list * snafu)
{
	int i;

	for (i = 0; i < snafu->num; i++) {
		free(snafu->entries[i]);
		snafu->entries[i] = NULL;
	}
	list_remove_all(snafu);
}

void list_append(list * snafu, void *entry)
{
	assert(!snafu->order);

	if (snafu->allocated <= snafu->num)
		list_resize(snafu, 1 + snafu->num * 2);

	snafu->entries[snafu->num++] = entry;
}

void list_insert(list * snafu, void *entry, int pos)
{
	assert(!snafu->order);
	if (snafu->allocated <= snafu->num)
		list_resize(snafu, 1 + snafu->num * 2);

	memmove(snafu->entries + pos + 1, snafu->entries + pos, (snafu->num - pos) * sizeof(void *));
	snafu->num++;
	snafu->entries[pos] = entry;
}

void list_prepend(list * snafu, void *entry)
{
	assert(!snafu->order);
	list_insert(snafu, entry, 0);
}

void list_remove_entry(list * snafu, int index)
{
	assert(0 <= index && index < snafu->num);
	if (snafu->num == 1) {
		list_remove_all(snafu);
	} else {
		void **base = snafu->entries + index;

		memmove(base, base + 1, (--snafu->num - index) * sizeof base[0]);
		if (snafu->allocated > 4 * snafu->num)
			/* a quarter is used, shrink the size to the half */
			list_resize(snafu, 2 * snafu->num);
	}
}

static void list_resize(list * snafu, int nmemb)
{
	if (!nmemb) {
		free(snafu->entries);
		snafu->entries = NULL;
	} else if (!snafu->allocated) {
		snafu->entries = malloc(sizeof(void *) * nmemb);
		assert(snafu->entries);
	} else {
		snafu->entries = realloc(snafu->entries, sizeof(void *) * nmemb);
		assert(snafu->entries);
	}

	snafu->allocated = nmemb;
}

void list_check_values(list * snafu, int h)
{
	static int scrolloff = -1;
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

	if (scrolloff == -1)
		scrolloff = atoi(get_config("set", "scrolloff", "0"));
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

void *list_member(list * snafu, int i)
{
	if (!snafu->num)
		return NULL;

	assert(0 <= i && i < snafu->num);

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
static int sortpos(void **l, CmpFunc cmp, void *item, int lower, int upper)
{
	while (upper != lower) {
		int mid, res;
		void *item2;

		assert(lower < upper);

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
void list_resort(list * l, int pos)
{
	void *item = l->entries[pos];
	CmpFunc cmp = l->order;

	if (!cmp)
		return;

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
	}
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
	if (!cmp || snafu->num < 2)
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
		list_resize(snafu, 1 + snafu->num * 2);

	snafu->entries[snafu->num++] = item;

	list_resort(snafu, snafu->num - 1);
}

int list_lookup(const char *needle, list * snafu)
{
	return lookup(needle, (const char **) snafu->entries, snafu->num);
}

void list_foreach(list * snafu, LFunc func)
{
	int i;

	for (i = 0; i < snafu->num; i++)
		func(snafu->entries[i]);
}

void list_filter(list * snafu, LFilter keep, void *udata, void (*destroy) (void *))
{
	int i, j;

	for (j = i = 0; i < snafu->num; i++)
		if (keep(snafu->entries[i], udata))
			snafu->entries[j++] = snafu->entries[i];
		else
			destroy(snafu->entries[i]);
	snafu->num = j;
}

int list_find(list * snafu, void *elem)
{
	int i;
	CmpFunc cmp = snafu->order;

	if (cmp) {
		i = sortpos(snafu->entries, cmp, elem, 0, snafu->num);

		if (i < snafu->num && cmp(elem, snafu->entries[i]) == 0)
			return i;
	} else {
		for (i = 0; i < snafu->num; i++)
			if (snafu->entries[i] == elem)
				return i;
	}
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

#ifdef TEST_LISTS
/* gcc -O2 -Wall list.c parse.o -lncurses -DTEST_LISTS && ./a.out */

#include <math.h>
#include <time.h>

int unsorted(list * snafu)
{
	int i;

	assert(snafu->order);

	for (i = 1; i < snafu->num; i++) {
		if (snafu->order(snafu->entries[i - 1], snafu->entries[i]) > 0)
			return i;
		if (snafu->order(snafu->entries[i], snafu->entries[i - 1]) < 0)
			return -i;
	}
	return 0;
}

int int_compare(int *a, int *b)
{
	if (*a != *b)
		return *a - *b;
	return a - b;
}

int byte_compare(int *a, int *b)
{
	return *a - *b;
}

#define RESET fflush(stdout); date = 0
#define START RESET; UNPAUSE
#define PAUSE date += clock()
#define UNPAUSE date -= clock()
#define REPORT printf(" %.2f secs\n", (float)date/CLOCKS_PER_SEC)
#define STOP PAUSE; REPORT

int main(void)
{
	list *l;
	const int N = 100000;
	const int M = 100;
	clock_t date;
	int i;
	int ints[N];

	printf("-----Test with all elements unique---------\n");
	srand(time(NULL));
	srandom(time(NULL));

	l = list_new();
	printf("Generating %d random elements...", N);
	START;
	for (i = 0; i < N; i++)
		ints[i] = random();
	STOP;

	printf("Inserting %d elements in list...", N);
	START;
	for (i = 0; i < N; i++)
		list_append(l, &ints[i]);
	STOP;

	printf("Finding %d elements...", N / M);
	START;
	for (i = 0; i < N; i += M) {
		if (list_find(l, &ints[i]) != i) {
			printf("FAILED on element %d (%d)\n", i, list_find(l, &ints[i]));
			return 1;
		}
	}
	STOP;

	printf("Sorting list...");
	START;
	list_sort(l, (CmpFunc) int_compare);
	PAUSE;
	if ((i = unsorted(l))) {
		printf("FAILED between elements %d-%d (%d %d).\n", i - 1, i, *(int *) list_index(l, i - 1),
			   *(int *) list_index(l, i));
		return 1;
	}
	REPORT;

	printf("Finding %d elements...", N);
	START;
	for (i = 0; i < N; i++) {
		int j = list_find(l, &ints[i]);

		if (j == -1 || list_index(l, j) != &ints[i]) {
			printf("FAILED on element %d (%d)\n", i, j);
			return 1;
		}
	}
	STOP;

	printf("Changing and resorting %d elements...", M);
	START;
	for (i = 0; i < N; i += N / M) {
		*(int *) list_index(l, i) = random();
		list_resort(l, i);
	}
	PAUSE;
	if (unsorted(l)) {
		printf("FAILED.\n");
		return 1;
	}
	REPORT;

	list_remove_all(l);
	list_sort(l, NULL);
	printf("-----Test with many equal elements---------\n");
	srand(time(NULL));

	printf("Generating %d random elements...", N);
	START;
	for (i = 0; i < N; i++)
		ints[i] = rand() % 16;
	STOP;

	printf("Inserting %d elements in list...", N);
	START;
	for (i = 0; i < N; i++)
		list_append(l, &ints[i]);
	STOP;

	printf("Finding %d elements...", N / M);
	START;
	for (i = 0; i < N; i += M) {
		if (list_find(l, &ints[i]) != i) {
			printf("FAILED on element %d\n", i);
			return 1;
		}
	}
	STOP;

	printf("Sorting list...");
	START;
	list_sort(l, (CmpFunc) byte_compare);
	PAUSE;
	if (unsorted(l)) {
		printf("FAILED.\n");
		return 1;
	}
	REPORT;

	printf("Finding %d elements...", N);
	START;
	for (i = 0; i < N; i++) {
		int j = list_find(l, &ints[i]);

		if (j == -1 || *(int *) list_index(l, j) != ints[i]) {
			printf("FAILED on element %d (%d)\n", i, j);
			return 1;
		}
	}
	STOP;

	printf("Changing and resorting %d elements...", M);
	START;
	for (i = 0; i < N; i += N / M) {
		*(int *) list_index(l, i) = rand() % 16;
		list_resort(l, i);
	}
	PAUSE;
	if (unsorted(l)) {
		printf("FAILED.\n");
		return 1;
	}
	REPORT;
	printf("All test PASSED.\n");
	return 0;
}

#endif
