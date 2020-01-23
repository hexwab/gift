/*
 * utils.c
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

#include "gift-fe.h"

#include "utils.h"
#include "share.h"

#include <ctype.h>

/*****************************************************************************/

static int generic_column_sort_str (GtkCList *clist, const void *p1,
									const void *p2);
static int generic_column_sort_int (GtkCList *clist, const void *p1,
									const void *p2);

/*****************************************************************************/

char *calculate_transfer_info (size_t transmit, size_t total,
                               size_t diff)
{
	char *info;

	info =
		g_strdup_printf ("%.02f%% (%s/%s) @ %.02fk/s",
		                 (((float)transmit / (float)total) * 100.0),
		                 format_size_disp (0, transmit),
						 format_size_disp (0, total),
		                 ((float)diff / 1024.0));

	return info;
}

/*****************************************************************************/

char *format_size_disp (char start_unit, size_t size)
{
	float human = size;
	char *unit = "\0kMGt  "; /* padding, for safety */

	while (*unit != start_unit)
		unit++;

	while (human > 1024.0)
	{
		unit++;
		human /= 1024.0;
	}

	return g_strdup_printf ("%.02f%c", human, *unit);
}

/* user@??? -> user */
char *format_user_disp (char *user)
{
	char *str, *ptr;

	if (!user)
		return NULL;

	str = strdup (user);

	if ((ptr = strstr (str, "@???")))
		*ptr = 0;

	return str;
}

char *format_href_disp (char *href)
{
	char *str, *ptr;
	register char *return_to; /* %2x -> char value */
	register int oct_val;     /* ...               */
	int str_len;              /* ...               */

	if (!href)
		return NULL;

	if ((ptr = strrchr (href, '/')))
		ptr++;
	else
		return NULL;

	/* make sure we are using our own memory here ... */
	ptr = strdup (ptr);
	str_len = strlen (ptr);

	/* convert '+' -> ' ' and %2x -> char value */
	for (str = ptr; *ptr; ptr++)
	{
		if (*ptr == '+')
			*ptr = ' ';
		else if (*ptr == '%')
		{
			return_to = ptr;

			/* next please... */
			ptr++;

			if (*ptr && ptr[1] &&
				isxdigit (*ptr) && isxdigit (ptr[1]))
			{
				oct_val = oct_value_from_hex (*ptr) * 16;
				oct_val += oct_value_from_hex (ptr[1]);

				/* 'bla%2Fbla\0' -> 'bla%bla\0' */
				memmove (return_to + 1, return_to + 3,
						 str_len - (return_to - str) - 2);
				str_len -= 2;

				*return_to = (char) oct_val;
			}

			ptr = return_to; /* return the ptr to the char we just translated
							  * to */
		}
	}

	return str;
}

int oct_value_from_hex (char hex_char)
{
	if (!isxdigit (hex_char))
		return 0;

	if (hex_char >= '0' && hex_char <= '9')
		return (hex_char - '0');

	hex_char = toupper (hex_char);

	return ((hex_char - 'A') + 10);
}

/*****************************************************************************/

/* locate a node to insert the given hash and/or user into the supplied tree
 * and optionally below the supplied GtkCTreeNode */
GtkCTreeNode *find_share_node (char *hash, char *user, unsigned long size,
                               GtkWidget *tree, GtkCTreeNode *node)
{
	void *obj;
	SharedFile *shr;
	GtkCTreeNode *snode;

	if (!hash && !user) /* wow wasnt that easy to find */
		return node;

	/* search the toplevel for an entry matching the hash ... */
	if (!node)
		snode = GTK_CTREE_NODE (GTK_CLIST (tree)->row_list);
	else
		snode = GTK_CTREE_ROW (node)->children;

	for (; snode; snode = GTK_CTREE_ROW (snode)->sibling)
	{
		obj = gtk_ctree_node_get_row_data (GTK_CTREE (tree), snode);

		if (!obj)
			continue;

		/* this is bad.  we know that all objects have the same basic makeup,
		 * and the objects that will enter this functions will have the
		 * SharedFile structure after the initial Object structure. */
		shr = (SharedFile *)(((char *)obj) + sizeof (Object));

		if (!shr || !shr->hash || !shr->user)
			continue;

		if (size && shr->filesize != size)
			continue;
		if (hash && strcmp (shr->hash, hash))
			continue;
		if (user && strcmp (shr->user, user))
			continue;

		/* matched both supplied arguments */
		return snode;
	}

	/* if they supplied a node to search under, just give them that node back
	 * and have them insert there */
	return node;
}

int children_length (GtkCTreeNode *parent)
{
	GtkCTreeNode *node;
	int len = 0;

	node = GTK_CTREE_ROW (parent)->children;

	for (; node; node = GTK_CTREE_ROW (node)->sibling)
		len++;

	return len;
}

/*****************************************************************************/

/* take in a clist/ctree/text widget and wrap it in a scrolled window */
GtkWidget *ft_scrolled_window (GtkWidget *widget, GtkPolicyType hpolicy,
							   GtkPolicyType vpolicy)
{
	GtkWidget *s_window;

	s_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (s_window),
									hpolicy, vpolicy);

	gtk_container_add (GTK_CONTAINER (s_window), widget);

	return s_window;
}

/*****************************************************************************/

void generic_column_sort (GtkWidget *w, int column, char *sort_info)
{
	GtkCList *clist;
	void *sort_func;
	int str_len;

	trace ();

	assert (sort_info);

	str_len = strlen (sort_info);

	if (column >= str_len || column < 0)
		return;

	clist = GTK_CLIST (w);

	switch (sort_info[column])
	{
	 case 's':
		sort_func = generic_column_sort_str;
		break;
	 case 'i':
		sort_func = generic_column_sort_int;
		break;
	 case ' ':
	 default:
		sort_func = NULL;
		break;
	}

	/* if we clicked the same column twice, reverse the sort order */
	if (column == clist->sort_column)
		clist->sort_type = !clist->sort_type;
	else if (sort_func)
		gtk_clist_set_compare_func (clist, sort_func);

	gtk_clist_set_sort_column (clist, column);

	if (GTK_IS_CTREE (w))
		gtk_ctree_sort_recursive (GTK_CTREE (w), NULL);
	else
		gtk_clist_sort (clist);
}

static int generic_column_sort_str (GtkCList *clist,
									const void *p1, const void *p2)
{
	char *str1, *str2;

	str1 = ROW_GET_TEXT (clist, p1);
	str2 = ROW_GET_TEXT (clist, p2);

	if (!str1 || !str2)
		return 1;

	return strcmp (str1, str2);
}

static int generic_column_sort_int (GtkCList *clist,
									const void *p1, const void *p2)
{
	char *str1, *str2;
	int int1, int2;

	str1 = ROW_GET_TEXT (clist, p1);
	str2 = ROW_GET_TEXT (clist, p2);

	/* uh oh. */
	if (!str1 || !str2)
		return 0;

	int1 = ATOI (str1);
	int2 = ATOI (str2);

	if (int1 > int2)
		return -1;

	return (int1 <= int2);
}
