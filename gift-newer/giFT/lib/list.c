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

	assert (list->head != NULL);

	/* tail will be unset by the sorting algorithms...recalculate it */
	if (!list->head->tail)
	{
		List *ptr;

		for (ptr = list; ptr->next; ptr = ptr->next);

		list->head->tail = ptr;
	}

	return list->head->tail;
}

int list_length (List *list)
{
	if (!list)
		return 0;

	assert (list->head != NULL);

	/* TODO -- we are not very strict in maintaining the precalculated
	 * length so we can't trust it...length will never be set */
	if (list->head->length < 0)
	{
		int len = 0;

		while (list)
		{
			len++;
			list = list->next;
		}

		return len;
	}

	return list->head->length;
}

/*****************************************************************************/

List *list_lock (List *list)
{
	if (!list)
		return NULL;

	assert (list->head != NULL);

	/* increment lock count */
	list->head->locked++;

	return list;
}

List *list_unlock (List *list)
{
	List *ptr;
	List *next;

	if (!list)
		return NULL;

	assert (list->head != NULL);

	/* descrement the locking count */
	if (list->head->locked > 0)
		list->head->locked--;

	if (list->head->locked > 0)
		return list;

	/* it's now unlocked, proceed */

	/* merge the pending lists back into the now unlocked list
	 *
	 * WARNING: DO NOT use any of the list_* functions on these lists!  You
	 * will end up creating unpredictable recursive behaviour.  You must
	 * manipulate the lists manually */

	for (ptr = list->head->pending_add; ptr; )
	{
		next = ptr->next;
		list = list_append (list, ptr->data);

		free (ptr);
		ptr = next;
	}

	list->head->pending_add = NULL;

	for (ptr = list->head->pending_remove; ptr; )
	{
		next = ptr->next;
		list = list_remove (list, ptr->data);

		/* pending remove just unlinked the whole list...bail */
		if (!list)
			return NULL;

		free (ptr);
		ptr = next;
	}

	list->head->pending_remove = NULL;

	return list;
}

/*****************************************************************************/

static List *list_new_entry (List *list, void *data)
{
	List *new_ent;

	new_ent = malloc (sizeof (List));

	new_ent->data = data;

	new_ent->next = NULL;

	if (list)
		new_ent->head = list->head;
	else
	{
		ListHead *head;

		head = malloc (sizeof (ListHead));

		head->locked         = FALSE;
		head->pending_add    = NULL;
		head->pending_remove = NULL;
		head->length         = -1;
		head->tail           = new_ent;

		/* this entry is created a new list...create a new head with it */
		new_ent->head = head;
	}

	return new_ent;
}

static List *lock_prepend (List *list, List *entry)
{
	entry->head = NULL;

	entry->next = list->head->pending_add;

	list->head->pending_add = entry;

	return list;
}

List *list_append (List *list, void *data)
{
	List *entry;
	List *tail;

	entry = list_new_entry (list, data);

	if (!list)
		return entry;

	if (list->head->locked)
		return lock_prepend (list, entry);

	tail = list_last (list);

	tail->next       = entry;
	list->head->tail = entry;

	return list;
}

List *list_prepend (List *list, void *data)
{
	List *entry;

	entry = list_new_entry (list, data);

	if (!list)
		return entry;

	if (list->head->locked)
		return lock_prepend (list, entry);

	/* NOTE: tail is unaffected */
	entry->next = list;

	return entry;
}

/*****************************************************************************/

List *list_copy (List *list)
{
	List *new_list = NULL;
	List *ptr;

	for (ptr = list; ptr; ptr = list_next (ptr))
		new_list = list_append (new_list, ptr->data);

	return new_list;
}

/*****************************************************************************/

/* my $data = shift @list;
 * NOTE: this function doesn't work with locked lists */
List *list_shift (List *list, void **data)
{
	List *next;

	if (!list)
	{
		if (data)
			*data = NULL;

		return NULL;
	}

	assert (list->head);
	assert (list->head->locked == 0); /* TODO */

	if (data)
		*data = list->data;

	next = list->next;

	if (!next)
	{
		list_free (list);
		return NULL;
	}

	/* NOTE: tail is unaffected */

	free (list);

	return next;
}

/* unshift @list, $data; */
List *list_unshift (List *list, void *data)
{
	return list_prepend (list, data);
}

/*****************************************************************************/

static List *lock_remove (List *list, void *data)
{
	List *entry;

	entry = list_new_entry (list, data);

	entry->head = NULL;
	entry->next = list->head->pending_remove;

	list->head->pending_remove = entry;

	return list;
}

List *list_remove (List *list, void *data)
{
	List *ptr;
	List *prev = NULL;
	List *last;

	if (!list)
		return NULL;

	assert (list->head);

	if (list->head->locked)
		return lock_remove (list, data);

	last = list_last (list);

	for (ptr = list; ptr; ptr = list_next (ptr))
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
					list->head->tail = last;
			}
			/* tail */
			else if (ptr == last)
				list->head->tail = prev ? prev : list;

			if (!list)
			{
				list_free (ptr);
				return NULL;
			}

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

	for (ptr = list; ptr; ptr = list_next (ptr))
	{
		if (data == ptr->data)
			return ptr;
	}

	return NULL;
}

/*****************************************************************************/

static void head_free (ListHead *head)
{
	free (head);
}

List *list_free (List *list)
{
	List *ptr = list;
	List *next;

	if (!list)
		return NULL;

	assert (list->head);
	assert (list->head->locked == 0); /* TODO */

	while (ptr)
	{
		next = ptr->next;

		if (!next)
			head_free (ptr->head);

		free (ptr);

		ptr = next;
	}

	return NULL;
}

/*****************************************************************************/

List *list_find_custom (List *list, void *data, CompareFunc func)
{
	if (!func)
		return list_find (list, data);

	/* TODO - verify this is correct. */
	for (; list; list = list_next (list))
		if (func (list->data, data) == 0)
			return list;

	return NULL;
}

/*****************************************************************************/

#if 0
List *list_remove_custom (List *list, void *data, CompareFunc func)
{
	List *original = list;

	if (!func)
		return list_remove (list, data);

	for(; list; list = list_next (list))
		if (func (list->data, data) == 0)
			original = list_remove (original, list->data);

	return original;
}
#endif

/*****************************************************************************/

void *list_nth_data (List *list, int nth)
{
	if (nth < 0)
		return NULL;

	for (; list && nth; list = list_next (list), nth--)
		/* */;

	return (list ? list->data : NULL);
}

/*****************************************************************************/
/* copied from GLib w/ modifications */

static int list_sort_default (char *a, char *b)
{
	return strcmp (a, b);
}

static List *list_sort_merge (List *l1, List *l2, CompareFunc compare_func)
{
	List  list, *l;
	List *last, *front = NULL;
	List *l1_last = NULL;
	List *l2_last = NULL;

	l = &list;

	if (l1)
		l1_last = l1->head->tail;
	else
		return l2;

	if (l2)
		l2_last = l2->head->tail;
	else
		return l1;

	if (compare_func (l1->data, l2->data) < 0)
		front = l1;
	else
		front = l2;

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
    }

	if (l1)
	{
		l->next = l1;
		last = l1_last;
	}
	else
	{
		l->next = l2;
		last = l2_last;
	}

	if (last)
		last->next = NULL;

	front->head->tail = NULL;
#if 0
	front->last = last;
#endif

	return front;
}

List *list_sort (List *list, CompareFunc compare_func)
{
	List *l1, *l2;

	if (!list)
		return NULL;

	if (!list->next)
	{
		list->head->tail = NULL;
#if 0
		list->last = list;
#endif
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

	l2               = l1->next;
	l2->head->tail   = NULL;
	list->head->tail = NULL;
#if 0
	l2->last         = list->last;
	list->last       = l1;
#endif
	l1->next         = NULL;

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

	if (!func)
		func = (ListForeachFunc) remove_free;

	last = list->head->tail;
	ptr  = list;

	while (ptr)
	{
		next = ptr->next;

		if ((*func) (ptr->data, udata))
		{
			if (list->head->locked)
			{
				lock_remove (list, ptr->data);
				continue;
			}

			if (prev)
				prev->next = ptr->next;

			/* head */
			if (ptr == list)
			{
				list = list->next;

				if (list)
					list->head->tail = last;
			}
			/* tail */
			else if (ptr == last)
				list->head->tail = prev ? prev : list;

			if (!list)
			{
				list_free (ptr);
				return NULL;
			}

			free (ptr);
		}

		ptr  = next;
		prev = ptr;
	}

	return list;
}

/*****************************************************************************/

#if 0
static List *x1 = NULL;
static List *x2 = NULL;

static void my_foreach (List **list, ListForeachFunc func)
{
	List *ptr;

	printf ("my_foreach (%p)\n{\n", list);

	*list = list_lock (*list);

	for (ptr = *list; ptr; ptr = list_next (ptr))
	{
		printf ("\tmy_foreach: %p: %p: '%s'\n", list, ptr, (char *) ptr->data);
		(*func) (list, ptr);
	}

	*list = list_unlock (*list);

	printf ("}\n");
}


static int x1_print (List **list, List *node)
{
	printf ("x1_print: %p: '%s'\n", list, (char *) node->data);
	return 0;
}

static int x1_move (List **list, List *node)
{
	if (node->data == "x1insert3")
	{
		printf ("moving '%s'\n", (char *) node->data);
		*list = list_remove (*list, node->data);
		x2 = list_append (x2, node->data);
	}

	return 0;
}

static int x1_fuckup (List **list, List *node)
{
	printf ("removing '%s'\n", (char *) node->data);
	*list = list_remove (*list, node->data);

	printf ("attempting print...\n");
	my_foreach (list, (ListForeachFunc) x1_print);

	printf ("attempting move...\n");
	my_foreach (list, (ListForeachFunc) x1_move);

	return 0;
}

int main ()
{
	x1 = list_append (x1, "x1insert1");
	x1 = list_append (x1, "x1insert2");
	x1 = list_append (x1, "x1insert3");

	x2 = list_append (x2, "x2insert1");
	x2 = list_append (x2, "x2insert2");
	x2 = list_append (x2, "x2insert3");

	my_foreach (&x1, (ListForeachFunc) x1_fuckup);
	my_foreach (&x1, (ListForeachFunc) x1_print);

	return 0;
}
#endif

#if 0
int main ()
{
	List *x1 = NULL;
	List *x2 = NULL;
	List *x3 = NULL;
	List *ptr;

	x1 = list_append (x1, "x1insert1");
	x1 = list_append (x1, "x1insert2");
	x1 = list_append (x1, "x1insert3");
	x1 = list_append (x1, "x1insert4");

	x2 = list_append (x2, "x2insert1");
	x2 = list_append (x2, "x2insert2");

	x3 = list_append (x3, "x3insert1");
	x3 = list_append (x3, "x3insert2");
	x3 = list_append (x3, "x3insert3");
	x3 = list_append (x3, "x3insert4");
	x3 = list_append (x3, "x3insert5");
	x3 = list_append (x3, "x3insert6");
	x3 = list_append (x3, "x3insert7");
	x3 = list_append (x3, "x3insert8");
	x3 = list_append (x3, "x3insert9");
	x3 = list_append (x3, "x3insert10");

	x1 = list_lock (x1);

	for (ptr = x1; ptr; ptr = list_next (ptr))
	{
		if (ptr->data == "x1insert1" || ptr->data == "x1insert4")
		{
			printf ("x1: removing '%s'\n", (char *) ptr->data);
			x1 = list_remove (x1, ptr->data);
			printf ("x2: adding   '%s'\n", (char *) ptr->data);
			x2 = list_append (x2, ptr->data);
		}
	}

	x1 = list_unlock (x1);

	printf ("\n");

	for (ptr = x1; ptr; ptr = list_next (ptr))
		printf ("x1: ptr->data = '%s'\n", (char *) ptr->data);

	printf ("\n");

	x2 = list_lock (x2);

	for (ptr = x2; ptr; ptr = list_next (ptr))
	{
		if (ptr->data == "x2insert1")
		{
			printf ("x2: removing '%s'\n", (char *) ptr->data);
			x2 = list_remove (x2, ptr->data);
			printf ("x1: adding   '%s'\n", (char *) ptr->data);
			x1 = list_append (x1, ptr->data);
		}
	}

	x2 = list_unlock (x2);

	printf ("\n");

	for (ptr = x2; ptr; ptr = list_next (ptr))
		printf ("x2: ptr->data = '%s'\n", (char *) ptr->data);

	printf ("\n");

	x3 = list_lock (x3);

	for (ptr = x3; ptr; ptr = list_next (ptr))
	{
		if (ptr->data == "x3insert1" || ptr->data == "x3insert2" ||
			ptr->data == "x3insert6" || ptr->data == "x3insert7" ||
			ptr->data == "x3insert8" || ptr->data == "x3insert10")
		{
			printf ("x3: removing '%s'\n", (char *) ptr->data);
			x3 = list_remove (x3, ptr->data);
			printf ("x1: adding   '%s'\n", (char *) ptr->data);
			x1 = list_append (x1, ptr->data);
		}
	}

	x3 = list_unlock (x3);

	printf ("\n");

	for (ptr = x3; ptr; ptr = list_next (ptr))
		printf ("x3: ptr->data = '%s'\n", (char *) ptr->data);

	printf ("\n");

	printf ("\n\nSUMMARY:\n");

	for (ptr = x1; ptr; ptr = list_next (ptr))
		printf ("x1: ptr->data = '%s'\n", (char *) ptr->data);

	for (ptr = x2; ptr; ptr = list_next (ptr))
		printf ("x2: ptr->data = '%s'\n", (char *) ptr->data);

	for (ptr = x3; ptr; ptr = list_next (ptr))
		printf ("x3: ptr->data = '%s'\n", (char *) ptr->data);

	return 0;
}
#endif
