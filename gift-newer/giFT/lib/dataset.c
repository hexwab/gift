/*
 * dataset.c - a set that maps strings to arbitrary values
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

/*****************************************************************************/

static int dataset_key_remove (unsigned long key, void *value, void *udata)
{
	char *string_key = (char *) key;

	free (string_key);

	return 1;
}

static int invert_strcmp (char *a, char *b)
{
	return (strcmp (a, b) == 0);
}

/*****************************************************************************/

Dataset *dataset_new ()
{
	HashTable *dataset;

	dataset                  = hash_table_new ();
	dataset->hash_func       = (HashKeyFunc) hash_string;
	dataset->key_equal_func  = (HashEqualFunc) invert_strcmp;
	dataset->key_remove_func = dataset_key_remove;

	return dataset;
}

void *dataset_lookup (Dataset *dataset, char *string)
{
	return hash_table_lookup (dataset, P_INT (string));
}

void dataset_insert_real (Dataset *dataset, char *key, void *value)
{
	hash_table_insert (dataset, P_INT (STRDUP (key)),
	                   value);
}

void dataset_remove (Dataset *dataset, char *string)
{
	hash_table_remove (dataset, P_INT (string));
}
