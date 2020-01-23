/*
 * list.c
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

#include "gift.h"

#include "list.h"

/*****************************************************************************/

List *list_last (List *list)
{
	if (!list)
		return NULL;

	if (!list->last)
	{
		List *ptr;

		for (ptr = list; ptr->next; ptr = ptr->next);

		return ptr;
	}

	return list->last;
}

int list_length (List *list)
{
	int len = 0;

	while (list)
	{
		len++;
		list = list->next;
	}

	return len;
}

static List *list_new_entry (List *head, void *data)
{
	List *list_entry;

	list_entry = malloc (sizeof (List));

	list_entry->data = data;

	list_entry->next = NULL;
	list_entry->last = head ? head->last : list_entry;

	return list_entry;
}

/*****************************************************************************/
List *list_append (List *list, void *data)
{
	List *list_entry;
	List *last;

	list_entry = list_new_entry (list, data);

	if (!list)
		return list_entry;

	last = list_last (list);

	last->next = list_entry;
	list->last = list_entry;

	return list;
}

List *list_prepend (List *list, void *data)
{
	List *list_entry;

	list_entry = list_new_entry (list, data);

	if (!list)
		return list_entry;

	list_entry->next = list;

	return list_entry;
}

/*****************************************************************************/

#if 0
List *list_link_append (List *list, List *link)
{
	List *last;

	if (!list)
		return link;

	if (!link)
		return list;

	assert (link->last);
	assert (list->last);

	list->last->next = link;
	list->last = link->last;

	return list;
}

List *list_link_prepend (List *list, List *link)
{
	if (!list)
		return link;

	if (!link)
		return list;

	assert (link->last);
	assert (list->last);

	link->last->next = list;
	link->last = list->last;

	return link;
}
#endif

/*****************************************************************************/
List *list_copy (List *list)
{
	List *new_list = NULL, *ptr;

	for (ptr = list; ptr; ptr = ptr->next)
		new_list = list_append (new_list, ptr->data);

	return new_list;
}

/*****************************************************************************/
/* do _NOT_ use this function on anything but the head! */
List *list_shift (List *list, void **data)
{
	List *next;

	if (!list)
	{
		if (data)
			*data = NULL;

		return NULL;
	}

	if (data)
		*data = list->data;

	next = list->next;

	if (next)
		next->last = list->last;

	free (list);

	return next;
}

/*****************************************************************************/

List *list_remove (List *list, void *data)
{
	List *ptr, *prev = NULL, *last;

	if (!list)
		return NULL;

	if (!(last = list_last (list)))
		return NULL;

	for (ptr = list; ptr; ptr = ptr->next)
	{
		if (data == ptr->data)
		{
			if (prev)
				prev->next = ptr->next;

			/* head */
			if (ptr == list)
			{
				list = list->next;
				if (list)
					list->last = last;
			}
			/* tail */
			else if (ptr == last)
				list->last = prev ? prev : list;

			free (ptr);

			break;
		}
		prev = ptr;
	}

	return list;
}

/*****************************************************************************/

List *list_find (List *list, void *data)
{
	List *ptr;

	for (ptr=list; ptr; ptr=ptr->next)
	{
		if (data == ptr->data)
			return ptr;
	}

	return NULL;
}

/*****************************************************************************/

List *list_free (List *list)
{
	List *ptr = list, *next;

	while (ptr)
	{
		next = ptr->next;
		free (ptr);
		ptr = next;
	}

	return NULL;
}

/*****************************************************************************/

List *list_find_custom (List *list, void *data, CompareFunc func)
{
	if (!func)
		return NULL;

	/* TODO - verify this is correct. */
	for (; list; list = list_next (list))
		if (func (list->data, data) == 0)
			return list;

	return NULL;
}

/*****************************************************************************/

List *list_remove_custom (List *list, void *data, CompareFunc func)
{
   List *original = list;

   if (!func)
		return NULL;

   for(; list; list = list_next (list))
		if (func (list->data, data) == 0)
			original = list_remove (original, list->data);

   return original;
}

/*****************************************************************************/

void *list_nth_data (List *list, int nth)
{
	if (nth < 0)
		return (void *) NULL;

	/* TODO - optimize */
	for (; list && nth; list = list_next (list), nth--)
		;

	if (list)
		return list->data;

	return (void *)NULL;
}

/*****************************************************************************/
/* copied from GLib */

static int list_sort_default (char *a, char *b)
{
	return strcmp (a, b);
}

static List *list_sort_merge (List *l1, List *l2, CompareFunc compare_func)
{
	List  list, *l;
	List *front = NULL;

	l = &list;

	while (l1 && l2)
    {
		if (compare_func (l1->data, l2->data) < 0)
        {
			l = l->next = l1;
			l1 = l1->next;
        }
		else
		{
			l = l->next = l2;
			l2 = l2->next;
        }

		if (!front)
			front = l;
    }

	l->next           = l1 ? l1 : l2;
	front->last       = l->next->last;
	front->last->next = NULL;

	return front;
}

List *list_sort (List *list, CompareFunc compare_func)
{
	List *l1, *l2;

	if (!list)
		return NULL;

	if (!list->next)
	{
		list->last = list;
		return list;
	}

	if (!compare_func)
		compare_func = (CompareFunc) list_sort_default;

	l1 = list;
	l2 = list->next;

	while ((l2 = l2->next) != NULL)
    {
		if ((l2 = l2->next) == NULL)
			break;

		l1 = l1->next;
    }

	l2 = l1->next;
	l1->next = NULL;

	return list_sort_merge (list_sort (list, compare_func),
	                        list_sort (l2,   compare_func),
	                        compare_func);
}

/*****************************************************************************/

void list_foreach (List *list, ListForeachFunc func, void *udata)
{
	List *ptr;

	for (ptr = list; ptr; ptr = list_next (ptr))
	{
		(*func) (ptr->data, udata);
	}
}

static int remove_free (void *data, void *udata)
{
	free (data);

	return 1;
}

List *list_foreach_remove (List *list, ListForeachFunc func, void *udata)
{
	List *ptr;
	List *prev = NULL;
	List *last;
	List *next;

	if (!list)
		return list;

	if (!(last = list_last (list)))
		return list;

	if (!func)
		func = (ListForeachFunc) remove_free;

	ptr = list;

	while (ptr)
	{
		next = ptr->next;

		if ((*func) (ptr->data, udata))
		{
			if (prev)
				prev->next = ptr->next;

			/* head */
			if (ptr == list)
			{
				list = list->next;

				if (list)
					list->last = last;
			}
			/* tail */
			else if (ptr == last)
				list->last = prev ? prev : list;

			free (ptr);
		}

		ptr  = next;
		prev = ptr;
	}

	return list;
}
