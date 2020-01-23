/*
 * list.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __LIST_H__
#define __LIST_H__

#define list_next(n) (n ? n->next : NULL)
#define list_prev(n) (n ? n->prev : NULL)

/**************************************************************************/

typedef struct _list
{
	void *data;

	struct _list *next;
	struct _list *last;  /* last node, for optimized append */
} List;

/**************************************************************************/

typedef int (*CompareFunc)     (void *a, void *b);
typedef int (*ListForeachFunc) (void *data, void *udata);

List *list_last           (List *list);
int   list_length         (List *list);
List *list_append         (List *list, void *data);
List *list_prepend        (List *list, void *data);
List *list_copy           (List *list);
List *list_shift          (List *list, void **data);
List *list_remove         (List *list, void *data);
List *list_find           (List *list, void *data);
List *list_free           (List *list);
void *list_nth_data       (List *list, int nth);
List *list_find_custom    (List *list, void *data, CompareFunc func);
List *list_remove_custom  (List *list, void *data, CompareFunc func);
List *list_sort           (List *list, CompareFunc func);
void  list_foreach        (List *list, ListForeachFunc func, void *udata);
List *list_foreach_remove (List *list, ListForeachFunc func, void *udata);

/**************************************************************************/

#endif /* __LIST_H__ */
