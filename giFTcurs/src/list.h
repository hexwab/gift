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
 * $Id: list.h,v 1.50 2003/06/27 11:20:13 weinholt Exp $
 */
#ifndef _LIST_H
#define _LIST_H

typedef struct {
	void **entries;				/* The base address of the array */
	int num;					/* So many elements in this list */
	int allocated;				/* number of allocated entries */
	int sel;					/* The index of the selected item */
	int start;					/* This item is the first displayed in GUI */
	CmpFunc order;				/* compare function if sorted, NULL if unsorted */
} list;

typedef struct {
	void **entries;				/* The base address of the array */
	int num;					/* So many elements in this list */
	int allocated;				/* number of allocated entries */
} dynarray;

#define LIST_INITIALIZER { NULL, 0, -1, 0, 0, NULL }

#define list_index(l,i)		((l)->entries)[i]

/* initialize a list */
void list_initialize(list *);

/* free the list entries only */
void list_free_entries(list *);

/* append an entry to a list. Unsorted lists only */
void list_append(list *, void *entry);

/* insert an entry at given position. Unsorted lists only */
void list_insert(list *, void *entry, int pos);

/* remove an entry from a list */
void list_remove_entry(list *, int idx);
void list_remove_all(list *);

/* do a sanity check of the internal list members */
void list_check_values(list *, int h);

/* for lists that don't use selected item */
void list_check_values_simple(list *, int h);

/* return the selected entry */
void *list_selected(list *);

/* return the entry below selected entry */
void *below_selected(list *);

/* Sorts a list and keep it sorted */
/* If compare funcion takes a third parameter, snafu->udata is sent */
void list_sort(list * snafu, CmpFunc order);

/* simple CmpFunc that sorts after address */
int compare_pointers(const void *a, const void *b);

/* insert an entry into a sorted list */
int list_resort(list * snafu, int pos);
void list_insort(list * snafu, void *item);

typedef void (*LFunc) (void *data);
void list_foreach(list * snafu, LFunc func);

/* list_filter returns the number of deleted items */
typedef gboolean(*LFilter) (void *data, void *udata);
int list_filter(list * snafu, LFilter func, void *udata, void (*destroy) (void *));

/* unsorted list: find the pointer in list
 * sorted list: find element equal to list according to the compare function
 */
int list_find(list * snafu, const void *elem);

/* set scroll offset */
void list_set_scrolloff(int val);

/* dynarray funcs - similar to glib's */
void dynarray_append(dynarray * arry, void *item);
void dynarray_remove(dynarray * arry, void *item);
void dynarray_remove_index(dynarray * arry, int idx);
void dynarray_remove_index_fast(dynarray * arry, int idx);
void dynarray_remove_fast(dynarray * arry, void *item);
void dynarray_foreach(const dynarray * arry, LFunc func);
void dynarray_removeall(dynarray * arry);
int dynarray_find(const dynarray * arry, const void *item);

#endif
