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

/*****************************************************************************/

/**
 * @file list.h
 *
 * @brief Singly linked list routines
 *
 * Implements a not-so-basic singly linked list.  This list is optimized for
 * fast appends and has the ability to freeze write access and merge buffered
 * writes back in when thawed.
 */

/*****************************************************************************/

#define list_next(n) (n ? n->next : NULL)
#define list_prev(n) (n ? n->prev : NULL)

/*****************************************************************************/

struct _list;

/**
 * Head structure.  Used for optimizations and locking
 */
typedef struct
{
	/**
	 * @brief Locked count
	 *
	 * If locked, write access will be tracked and
	 * buffered until completely unlocked, in which case the data will be
	 * merged back to the main structure
	 */
	unsigned char locked;

	struct _list *pending_add;         /**< Tracked additions */
	struct _list *pending_remove;      /**< Tracked deletions */

	/* efficiency improvements */
	int           length;              /**< Total number of chains in this
										*   list.  \todo This does nothing */
	struct _list *tail;                /**< Pointer to the last chain */
} ListHead;

/**
 * Main linked list structure.  This represents all chains in the list.
 */
typedef struct _list
{
	void         *data;                /**< Data inserted */

	struct _list *next;                /**< Next chain */

	/**
	 * @brief Head refence
	 *
	 * Holds a reference to our parent.  This will tell us if we are locked,
	 * how large we are, where our tail is, etc etc.  It is done to improve
	 * efficiency while maintaining a compatible interface */
	ListHead     *head;
} List;

/**************************************************************************/

/**
 * Node comparison
 *
 * @retval >0 \a a is greater than \a b
 * @retval 0  \a a is equal to \a b
 * @retval <0 \a a is less than \a b
 */
typedef int (*CompareFunc)     (void *a, void *b);

/**
 * Foreach iteration function.  Return value depends on the usage.
 *
 * @param data  List data at this chain
 * @param udata Arbitrary data passed along
 */
typedef int (*ListForeachFunc) (void *data, void *udata);

List *list_last           (List *list);
int   list_length         (List *list);
List *list_lock           (List *list);
List *list_unlock         (List *list);
List *list_append         (List *list, void *data);
List *list_prepend        (List *list, void *data);
List *list_copy           (List *list);
List *list_shift          (List *list, void **data);
List *list_unshift        (List *list, void *data);
List *list_remove         (List *list, void *data);
List *list_find           (List *list, void *data);
List *list_free           (List *list);
void *list_nth_data       (List *list, int nth);
List *list_find_custom    (List *list, void *data, CompareFunc func);
#if 0
List *list_remove_custom  (List *list, void *data, CompareFunc func);
#endif
List *list_sort           (List *list, CompareFunc func);
void  list_foreach        (List *list, ListForeachFunc func, void *udata);
List *list_foreach_remove (List *list, ListForeachFunc func, void *udata);

/**************************************************************************/

#endif /* __LIST_H__ */
