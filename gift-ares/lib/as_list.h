/*
 * $Id: as_list.h,v 1.8 2004/11/19 21:16:13 HEx Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_LIST_H
#define __AS_LIST_H

/*****************************************************************************/

#define list_next(l) ((l) ? (l)->next : NULL)
#define list_prev(l) ((l) ? (l)->prev : NULL)

/*****************************************************************************/

/* Node comparison callback. Return value:
 * >0 a is greater than b 
 *  0 a is equal b 
 * <0 a is less than b 
 */
typedef int (*CompareFunc) (void *a, void *b);

/* iteration callback */
typedef int (*ListForeachFunc) (void *data, void *udata);

typedef struct listlink
{
	void *data;

	struct listlink *prev;
	struct listlink *next;
} List;

/*****************************************************************************/

/* Very inefficient since it reaquires a traversal of the entire list. Use
 * list_prepend whenever possible. 
 */
List *list_append (List *head, void *data);

List *list_prepend (List *head, void *data);

/* Insert a new link immediately before the index node */
List *list_insert (List *head, int index, void *data);

/* Insert a new link immediately before the node found by func. */
List *list_insert_sorted (List *head, CompareFunc func, void *data);

/* Make a copy of the entire list. Both lists will point to the same user
 * data.
 */
List *list_copy (List *head);

/*****************************************************************************/

/* Search and remove link containing data */
List *list_remove (List *head, void *data);

/* Same as list_remove but you have to pass the correct link instead of it
 * being searched 
 */
List *list_remove_link (List *head, List *link);

/* Free the entire list. Doesn't touch any user data. */
List *list_free (List *head);

/*****************************************************************************/

/* Find the first link which points to data */
List *list_find (List *head, void *data);

/* Find the first link for which func returns 0 */
List *list_find_custom (List *head, void *data, CompareFunc func);

/*****************************************************************************/

/* Iterate through the list and call func for each node */
void list_foreach (List *head, ListForeachFunc func, void *udata);

/* Iterate through the entire list and remove all links for which func returns
 * TRUE. If func is NULL all nodes are removed and the data member is freed.
 */
List *list_foreach_remove (List *head, ListForeachFunc func, void *udata);

/*****************************************************************************/

/* Return the link at the nth postition */
List *list_nth (List *head, int nth);

/* Same as list_nth but instead of the link the user data is returne */
void *list_nth_data (List *head, int nth);

/* Walk through the entire list and return last link. */
List *list_last (List *head);

/* Return the length of the list by walking through it. */
int list_length (List *head);

/*****************************************************************************/

/* Sort list using func for comparison */
List *list_sort (List *head, CompareFunc func);

/*****************************************************************************/

#endif /* __AS_LIST_H */
