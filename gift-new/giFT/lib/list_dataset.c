/*
 * list_dataset.c - string dataset implemented using a linked list
 * NOTE: the interface here doesn't perfectly model the dataset because the
 * dataset's interface sucks and needs to be changed as well :)
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
#include "list_dataset.h"

/*****************************************************************************/

struct _pair
{
	char *key;
	void *value;
};

/*****************************************************************************/

static struct _pair *pair_new (char *key, void *value)
{
	struct _pair *pair;

	if (!(pair = malloc (sizeof (struct _pair))))
		return NULL;

	pair->key   = STRDUP (key);
	pair->value = value;

	return pair;
}

static void pair_free (struct _pair *pair)
{
	assert (pair != NULL);

	free (pair->key);
	free (pair);
}

/*****************************************************************************/

static ListDataset *list_dataset_new ()
{
	ListDataset *list_ds;

	if (!(list_ds = malloc (sizeof (ListDataset))))
		return NULL;

	memset (list_ds, 0, sizeof (ListDataset));

	return list_ds;
}

static int free_pair (struct _pair *pair, int *free_value)
{
	/* just a convenience for lazy developers who do basic key/value
	 * pairs */
	if (P_INT (free_value))
		free (pair->value);

	pair_free (pair);
	return TRUE;
}

static void list_dataset_free (ListDataset *list_ds, int free_value)
{
	list_foreach_remove (list_ds->list, (ListForeachFunc) free_pair,
	                     I_PTR (free_value));
	free (list_ds);
}

static int check_alloc (ListDataset **list_ds)
{
	if (!list_ds)
		return FALSE;

	/* check to see if there is something allocated */
	if (*list_ds)
		return TRUE;

	/* nope, allocate it */
	*list_ds = list_dataset_new ();
	return (*list_ds ? TRUE : FALSE);
}

/*****************************************************************************/

static int lookup_key (struct _pair *pair, char *key)
{
	return strcmp (pair->key, key);
}

/* returns a list link for efficiency */
static List *lookup (ListDataset **list_ds, char *key)
{
	if (!list_ds || !(*list_ds) || !key)
		return NULL;

	return list_find_custom ((*list_ds)->list, key, (CompareFunc) lookup_key);
}

/*****************************************************************************/

void *list_dataset_lookup (ListDataset **list_ds, char *key)
{
	List         *link;
	struct _pair *pair;

	if (!(link = lookup (list_ds, key)))
		return NULL;

	pair = link->data;

	return pair->value;
}

/* funny we still name it insert though ;) */
void list_dataset_insert (ListDataset **list_ds, char *key, void *value)
{
	struct _pair *pair;

	if (!(check_alloc (list_ds)))
		return;

	/* let the developer know he fucked up */
	assert (list_dataset_lookup (list_ds, key) == NULL);

	if (!(pair = pair_new (key, value)))
		return;

	(*list_ds)->list = list_prepend ((*list_ds)->list, pair);
}

void list_dataset_remove (ListDataset **list_ds, char *key)
{
	List *link;

	/* lookup the matching link */
	if (!(link = lookup (list_ds, key)))
		return;

	pair_free (link->data);
	(*list_ds)->list = list_remove_link ((*list_ds)->list, link);
}

/*****************************************************************************/

static void clear_free (ListDataset **list_ds, int free_value)
{
	if (!list_ds || !(*list_ds))
		return;

	list_dataset_free (*list_ds, free_value);
	*list_ds = NULL;
}

void list_dataset_clear (ListDataset **list_ds)
{
	clear_free (list_ds, FALSE);
}

void list_dataset_clear_free (ListDataset **list_ds)
{
	clear_free (list_ds, TRUE);
}

/*****************************************************************************/

#if 0
int main ()
{
	ListDataset *list_ds = NULL;

	list_dataset_insert (&list_ds, "foo", STRDUP ("bar"));
	list_dataset_insert (&list_ds, "bar", STRDUP ("foo"));

	printf ("bar: %s\n", (char *) list_dataset_lookup (&list_ds, "bar"));
	printf ("foo: %s\n", (char *) list_dataset_lookup (&list_ds, "foo"));

	list_dataset_remove (&list_ds, "bar");
	printf ("bar: %s\n", (char *) list_dataset_lookup (&list_ds, "bar"));

	list_dataset_clear_free (&list_ds);

	return 0;
}
#endif
