/*
 * $Id: trie.c,v 1.5 2003/09/18 08:05:08 hipnod Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifdef STANDALONE
#include <libgift/libgift.h>
#else
#include "gt_gnutella.h"
#endif /* STANDALONE */

#include "trie.h"

/*****************************************************************************/

static Trie *trie_alloc (char c)
{
	Trie *trie;

	if (!(trie = MALLOC (sizeof (Trie))))
		return NULL;

	trie->c = c;

	return trie;
}

Trie *trie_new (void)
{
	/*
	 * Null won't match any character in the trie, so use 
	 * that as a sentinel in the root node.
	 */
	return trie_alloc (0);
}

static int free_children (Trie *trie, void *udata)
{
	trie_free (trie);
	return TRUE;
}
		
void trie_free (Trie *trie)
{
	List *children;

	if (!trie)
		return;

	children = trie->children;

	if (trie->terminal_node)
	{
		/* the first data item is the data item for this node */
		children = list_next (children);
	}

	list_foreach_remove (children, (ListForeachFunc)free_children, NULL);
	free (trie);
}

static Trie *find_node (Trie *trie, char c)
{
	List   *children;
	List   *ptr;

	children = trie->children;

	/*
	 * If this is a terminal node, skip the data list
	 * that is in the first position.
	 */
	if (trie->terminal_node)
		children = children->next;

	/*
	 * Could use list_find_custom here, but we want
	 * this to be as fast as possible.
	 *
	 * Should probably use a realloc'd array for the 
	 * child list instead.
	 */
	for (ptr = children; ptr; ptr = ptr->next)
	{
		Trie *node = ptr->data;

		if (node->c == c)
			break;
	}

#undef TEST_MOVE
#ifdef TEST_MOVE
	if (ptr)
	{
		void *data = ptr->data;
		int   index;

		/* remove from the old position */
		trie->children = list_remove_link (trie->children, ptr);

		/* insert at the beginning of the list, taking into account data
		 * list at the head if this is a terminal node */
		index = trie->terminal_node;
		trie->children = list_insert (trie->children, index, data);

		return data;
	}

	return NULL;
#else
	/* we directly access result->data here for efficiency */
	return ptr ? ptr->data : NULL;
#endif
}

/*
 * Find a node in the Trie. 
 *
 * @param trie  The Trie to look in
 * @param s     The string to search for
 * @param alloc Whether to allocate space for non-existent nodes during
 *              lookup
 */
static Trie *t_node_lookup (Trie *trie, char *s, int alloc)
{
	Trie   *result;
	char    c;

	while ((c = *s++))
	{
		if (!trie)
			break;

		result = find_node (trie, c);

		if (!result && alloc)
		{
			if (!(result = trie_alloc (c)))
				return NULL;

			trie->children = list_append (trie->children, result);
		}

		trie = result;
	}

	return trie;
}

void *trie_lookup (Trie *trie, char *s)
{
	Trie *node;

	node = t_node_lookup (trie, s, FALSE);

	if (!node)
		return NULL;

	if (node->terminal_node)
		return list_nth_data (node->children, 0);

	return NULL;
}

void trie_insert (Trie *trie, char *s, void *value)
{
	Trie   *node;
	void   *data;
	List   *head;

	node = t_node_lookup (trie, s, TRUE);

	if (!node)
	{
		/* must be memory allocation error */
		assert (0);
		return;
	}

	if (!node->terminal_node)
	{
		/* could be a mem allocation error here... */
		node->children = list_prepend (node->children, value);

		/* convert this node to a terminal node, with the data list
		 * in the first position of ->children */
		node->terminal_node = TRUE;
		return;
	}
	
	/* 
	 * This should not happen unless the user didn't call 
	 * remove first. That may leak memory, so assert here temporarily.
	 */
	assert (0);

	head = list_nth (node->children, 0);
	data = list_nth_data (node->children, 0);

	/* 
	 * Remove the first item, then insert a new list.
	 */
	node->children = list_remove_link (node->children, head);

	/* insert the new data list back */
	node->children = list_prepend (node->children, value);
}

int trie_is_empty (Trie *trie)
{
	if (trie->children == NULL)
		return TRUE;

	return FALSE;
}

static void remove_if_empty (Trie *root, Trie *child)
{
	if (!trie_is_empty (child))
		return;

	root->children = list_remove (root->children, child);
	trie_free (child);
}

static void t_remove_node (Trie *trie)
{
	void   *value;
	List   *value_ptr;

	if (!trie->terminal_node)
		return;

#if 0
	/* this assertion is broken due to duplicates */
	assert (trie->terminal_node == TRUE);
#endif
	
	value_ptr = list_nth (trie->children, 0);
	value     = list_nth_data (trie->children, 0);

#if 0
	/* these will falsely trigger for files that have the same
	 * token in them twice, and end up getting removed twice */
	assert (list_length (data_list) > 0);
	assert (list_find (data_list, value) != NULL);
#endif

	/* remove the old data list */
	trie->children      = list_remove_link (trie->children, value_ptr);
	trie->terminal_node = FALSE;
}

void trie_remove (Trie *trie, char *s)
{
	Trie *child;

	/* remove the node if we found it */
	if (string_isempty (s))
	{
		t_remove_node (trie);
		return;
	}

	child = find_node (trie, *s);
	s++;

	if (!child)
		return;
#if 0
	assert (child != NULL);
#endif

	/* recursively remove all nodes */
	trie_remove (child, s);

	/* remove this node if it has no children anymore */
	remove_if_empty (trie, child);
}

static void print_children (List *children)
{
	List   *ptr;
	int     printed_open = FALSE;
	Trie   *trie;

	for (ptr = children; ptr; ptr = list_next (ptr))
	{
		if (!printed_open)
		{
			printf ("{ ");
			printed_open = TRUE;
		}

		trie = list_nth_data (ptr, 0);
		trie_print (trie);

		if (list_next (ptr))
			printf(",");
	}

	if (children)
		printf (" }");
}

void trie_print (Trie *trie)
{
	List *children;

	if (trie->c)
		printf ("%c", trie->c);

	children = trie->children;

	if (trie->terminal_node)
	{
		printf ("*");
		children = list_next (children);
	}

	print_children (children);
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
	List *args = NULL;
	List *ptr;
	Trie *trie;
	int   i;

	trie = trie_new ();

	for (i = 1; i < argc; i++)
	{
		trie_insert (trie, argv[i], argv[i]);
		args = list_prepend (args, argv[i]);
	}

	trie_print (trie);
	printf ("\n");

	while ((ptr = args))
	{
		trie_remove (trie, ptr->data);
		args = list_remove_link (args, ptr);
	}

	trie_insert (trie, "book", "book");
	trie_insert (trie, "boo", "boo");
	trie_print (trie);
	printf ("\n");
	trie_remove (trie, "book");
	trie_remove (trie, "boo");

	trie_print (trie);
	printf ("\n");
#if 0
	trie_free (trie);
#endif

	return 0;
}
#endif /* STANDALONE */
