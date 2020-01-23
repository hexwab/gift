/* giFTcurs - curses interface to giFT
 * Copyright (C) 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
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
 * $Id: test_list.c,v 1.2 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "list.h"

int unsorted(list * snafu)
{
	int i;

	g_assert(snafu->order);

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
	list l;
	const int N = 100000;
	const int M = 100;
	clock_t date;
	int i;
	int ints[N];

	printf("-----Test with all elements unique---------\n");
	srand(time(NULL));
	srandom(time(NULL));

	list_initialize(&l);
	printf("Generating %d random elements...", N);
	START;
	for (i = 0; i < N; i++)
		ints[i] = random();
	STOP;

	printf("Inserting %d elements in list...", N);
	START;
	for (i = 0; i < N; i++)
		list_append(&l, &ints[i]);
	STOP;

	printf("Finding %d elements...", N / M);
	START;
	for (i = 0; i < N; i += M) {
		if (list_find(&l, &ints[i]) != i) {
			printf("FAILED on element %d (%d)\n", i, list_find(&l, &ints[i]));
			return 1;
		}
	}
	STOP;

	printf("Sorting list...");
	START;
	list_sort(&l, (CmpFunc) int_compare);
	PAUSE;
	if ((i = unsorted(&l))) {
		printf("FAILED between elements %d-%d (%d %d).\n", i - 1, i, *(int *) list_index(&l, i - 1),
			   *(int *) list_index(&l, i));
		return 1;
	}
	REPORT;

	printf("Finding %d elements...", N);
	START;
	for (i = 0; i < N; i++) {
		int j = list_find(&l, &ints[i]);

		if (j == -1 || list_index(&l, j) != &ints[i]) {
			printf("FAILED on element %d (%d)\n", i, j);
			return 1;
		}
	}
	STOP;

	printf("Changing and resorting %d elements...", M);
	START;
	for (i = 0; i < N; i += N / M) {
		*(int *) list_index(&l, i) = random();
		list_resort(&l, i);
	}
	PAUSE;
	if (unsorted(&l)) {
		printf("FAILED.\n");
		return 1;
	}
	REPORT;

	list_remove_all(&l);
	list_sort(&l, NULL);
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
		list_append(&l, &ints[i]);
	STOP;

	printf("Finding %d elements...", N / M);
	START;
	for (i = 0; i < N; i += M) {
		if (list_find(&l, &ints[i]) != i) {
			printf("FAILED on element %d\n", i);
			return 1;
		}
	}
	STOP;

	printf("Sorting list...");
	START;
	list_sort(&l, (CmpFunc) byte_compare);
	PAUSE;
	if (unsorted(&l)) {
		printf("FAILED.\n");
		return 1;
	}
	REPORT;

	printf("Finding %d elements...", N);
	START;
	for (i = 0; i < N; i++) {
		int j = list_find(&l, &ints[i]);

		if (j == -1 || *(int *) list_index(&l, j) != ints[i]) {
			printf("FAILED on element %d (%d)\n", i, j);
			return 1;
		}
	}
	STOP;

	printf("Changing and resorting %d elements...", M);
	START;
	for (i = 0; i < N; i += N / M) {
		*(int *) list_index(&l, i) = rand() % 16;
		list_resort(&l, i);
	}
	PAUSE;
	if (unsorted(&l)) {
		printf("FAILED.\n");
		return 1;
	}
	REPORT;
	return 0;
}
