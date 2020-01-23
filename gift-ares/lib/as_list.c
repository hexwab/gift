/*
 * $Id: as_list.c,v 1.13 2004/11/19 21:16:13 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* insert new link between prev and next, returns new link */
static List *insert_new_link (List *prev, List *next, void *data)
{
	List *new_link;

	new_link = malloc (sizeof (List));
	assert (new_link != NULL);

	new_link->prev = NULL;
	new_link->next= NULL;
	new_link->data = data;

	if (prev == NULL && next == NULL)
	{
		new_link->prev = NULL;
		new_link->next = NULL;
	}
	else if (prev == NULL)
	{
		assert (next->prev == NULL);

		new_link->prev = NULL;
		new_link->next = next;
		next->prev = new_link;
	}
	else if (next == NULL)
	{
		assert (prev->next == NULL);

		new_link->prev = prev;
		new_link->next = NULL;
		prev->next = new_link;
	}
	else
	{
		assert (prev->next == next);
		assert (next->prev == prev);

		new_link->prev = prev;
		new_link->next = next;
		prev->next = new_link;
		next->prev = new_link;
	}

	return new_link;
}

/*****************************************************************************/

List *list_append (List *head, void *data)
{
	if (!head)
		return insert_new_link (NULL, NULL, data);
	
	insert_new_link (list_last (head), NULL, data);
	return head;
}

List *list_prepend (List *head, void *data)
{
	return insert_new_link (NULL, head, data);
}

List *list_insert (List *head, int index, void *data)
{
	List *nth;

	assert (index >= 0);

	if (!head || index == 0)
		return list_prepend (head, data);

	if ((nth = list_nth (head, index)))
		insert_new_link (nth, nth->next, data);
	else
		list_append (head, data); /* O(n*2) total! */

	return head;
}

List *list_insert_sorted (List *head, CompareFunc func, void *data)
{
	List *link;

	assert (func);

	if (!head)
		return list_prepend (head, data);

	if (func (head->data, data) >= 0)
		return list_prepend (head, data);

	for (link = head; link->next; link = link->next)
	{
		if (func (link->next->data, data) >= 0)
		{
			insert_new_link (link, link->next, data);
			return head;
		}
	}

	/* insert as last element */
	insert_new_link (link, NULL, data);

	return head;
}

/* Make a copy of the entire list. Both lists will point to the same usre
 * data.
 */
List *list_copy (List *head)
{
	List *new_head;
	List *new_link;

	if (!head)
		return NULL;

	new_head = new_link = insert_new_link (NULL, NULL, head->data);

	for (head = head->next; head; head = head->next)
	{
		assert (head->prev->next == head);
		new_link = insert_new_link (new_link, NULL, head->data);
		assert (new_link->prev->next == new_link);
	}

	return new_head;
}

/*****************************************************************************/

/* Search and remove link containing data */
List *list_remove (List *head, void *data)
{
	return list_remove_link (head, list_find (head, data));
}

/* Same as list_remove but you have to pass the correct link instead of it
 * being searched 
 */
List *list_remove_link (List *head, List *link)
{
	if (!head)
		return NULL;

	if (!link)
		return head;

	if (link == head)
		head = link->next;

	if (link->prev)
		link->prev->next = link->next;

	if (link->next)
		link->next->prev = link->prev;

	free (link);

	return head;	
}

/* Free the entire list. Doesn't touch any user data. */
List *list_free (List *head)
{
	List *link;

	while (head)
	{
		link = head;
		head = head->next;
		free (link);
	}

	return NULL;
}

/*****************************************************************************/

/* Find the first link which points to data */
List *list_find (List *head, void *data)
{
	for (; head; head = head->next)
		if (head->data == data)
			return head;

	return NULL;
}

/* Find the first link for which func returns 0 */
List *list_find_custom (List *head, void *data, CompareFunc func)
{
	assert (func);
	
	for (; head; head = head->next)
		if (func (head->data, data) == 0)
			return head;

	return NULL;
}

/*****************************************************************************/

/* Iterate through the list and call func for each node */
void list_foreach (List *head, ListForeachFunc func, void *udata)
{
	assert (func);

	for (; head; head = head->next)
		func (head->data, udata);
}

static int remove_free (void *data, void *udata)
{
	free (data);
	return 1;
}

/* Iterate through the entire list and remove all links for which func returns
 * TRUE. If func is NULL all nodes are removed and the data member is freed.
 */
List *list_foreach_remove (List *head, ListForeachFunc func, void *udata)
{
	List *link;
	List *next;

	if (!func)
		func = (ListForeachFunc) remove_free;

	for (link = head; link;)
	{
		next = link->next;

		if (func (link->data, udata)) 
			head = list_remove_link (head, link);

		link = next;
	}

	return head;
}

/*****************************************************************************/

/* Return the link at the nth postition */
List *list_nth (List *head, int nth)
{
	assert (nth >= 0);

	if (!head)
		return NULL;

	for (; nth > 0 && head->next; nth--)
		head = head->next;

	return nth ? NULL : head;
}

/* Same as list_nth but instead of the link the user data is returne */
void *list_nth_data (List *head, int nth)
{
	List *link;

	if ((link = list_nth (head, nth)))
		return link->data;

	return NULL;
}

/* Walk through the entire list and return last link. */
List *list_last (List *head)
{
	if (!head)
		return NULL;

	for (; head->next; head = head->next);

	return head;
}

/* Return the length of the list by walking through it. */
int list_length (List *head)
{
	int i;
		
	for (i = 0; head; head = head->next, i++);

	return i;
}

/*****************************************************************************/

/*
 * Copyright 2001 Simon Tatham.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Somewhat modifed.
 */

List *list_sort (List *head, CompareFunc func)
{
	List *p, *q, *e, *tail;
    int insize, nmerges, psize, qsize, i;

    if (!head)
		return NULL;

	insize = 1;

	/* iterative merge sort */
	while (1)
	{
		p = head;
		head = NULL;
		tail = NULL;

		nmerges = 0;  /* count number of merges we do in this pass */

		while (p)
		{
			nmerges++;  /* there exists a merge to be done */
			/* step `insize' places along from p */
			q = p;
			psize = 0;
			for (i = 0; i < insize; i++)
			{
				psize++;
				q = q->next;
				if (!q)	break;
            }

			/* if q hasn't fallen off end, we have two lists to merge */
			qsize = insize;

			/* now we have two lists; merge them */
			while (psize > 0 || (qsize > 0 && q))
			{
				/* decide whether next element of merge comes from p or q */
				if (psize == 0)
				{
					/* p is empty; e must come from q. */
					e = q; q = q->next; qsize--;
				}
				else if (qsize == 0 || !q)
				{
					/* q is empty; e must come from p. */
					e = p; p = p->next; psize--;
				}
				else if (func (p->data, q->data) <= 0)
				{
					/* First element of p is lower (or same);
					 * e must come from p. */
					e = p; p = p->next; psize--;
				}
				else
				{
					/* First element of q is lower; e must come from q. */
					e = q; q = q->next; qsize--;
				}

				/* add the next element to the merged list */
				if (tail)
					tail->next = e;
				else
					head = e;
				
				/* Maintain reverse pointers in a doubly linked list. */
				e->prev = tail;

				tail = e;
			}
	
			/* now p has stepped `insize' places along, and q has too */
			p = q;
		}

		tail->next = NULL;

		/* If we have done only one merge, we're finished. */
		if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
			return head;

		/* Otherwise repeat, merging lists twice the size */
		insize *= 2;
	}
}

/*****************************************************************************/
