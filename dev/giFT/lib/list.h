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

#ifndef __LIST_H
#define __LIST_H

/*****************************************************************************/

/**
 * @file list.h
 *
 * @brief Singly linked list routines
 *
 * Implements a standard doubly linked list.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#define list_next(n) ((n) ? (n)->next : NULL)
#define list_prev(n) ((n) ? (n)->prev : NULL)

/*****************************************************************************/

/**
 * Node comparison.
 *
 * @retval >0 \a a is greater than \a b
 * @retval 0  \a a is equal to \a b
 * @retval <0 \a a is less than \a b
 */
typedef int (*CompareFunc) (void *a, void *b);

/**
 * Linked list structure.  This represents all chains in the list.
 */
typedef struct _list
{
	void         *data;                /**< Data inserted */

	struct _list *prev;                /**< Previous chain */
	struct _list *next;                /**< Next chain */
} List;

/**************************************************************************/

/**
 * Foreach iteration function.  Return value depends on the usage.
 *
 * @param data  List data at this chain
 * @param udata Arbitrary data passed along
 */
typedef int (*ListForeachFunc) (void *data, void *udata);

List *list_append         (List *list, void *data);
List *list_prepend        (List *list, void *data);
List *list_insert         (List *list, int index, void *data);
List *list_insert_sorted  (List *list, CompareFunc func, void *data);
List *list_remove         (List *list, void *data);
List *list_remove_link    (List *list, List *link);
List *list_free           (List *list);

List *list_sort           (List *list, CompareFunc func);
List *list_copy           (List *list);
List *list_find           (List *list, void *data);
List *list_find_custom    (List *list, void *data, CompareFunc func);
void  list_foreach        (List *list, ListForeachFunc func, void *udata);
List *list_foreach_remove (List *list, ListForeachFunc func, void *udata);

List *list_nth            (List *list, int n);
void *list_nth_data       (List *list, int n);
List *list_last           (List *list);
int   list_length         (List *list);

/**************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __LIST_H */
