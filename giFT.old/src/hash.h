/*
 * hash.h
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

#ifndef __HASH_H
#define __HASH_H

#include "dataset.h"
#include "list.h"

/*****************************************************************************/

typedef int (*HashFunc) (unsigned long key, void *value, void *udata);
typedef int (*HashEqualFunc) (unsigned long key1, unsigned long key2);
typedef unsigned long (*HashKeyFunc) (unsigned long key);

typedef struct _hash_node
{
	unsigned long  key;
	void          *value;

	struct _hash_node *next;
} HashNode;

typedef struct _hash_table
{
	size_t        size;
	unsigned long items;
	int           frozen;

	HashNode    **nodes;

	HashKeyFunc   hash_func;
	HashEqualFunc key_equal_func;
	HashFunc      key_remove_func;
} HashTable;

/*****************************************************************************/

HashTable     *hash_table_new     ();
void           hash_table_destroy (HashTable *table);
void           hash_table_destroy_free (HashTable *table);
List          *hash_flatten       (HashTable *table);
void          *hash_table_lookup  (HashTable *table, unsigned long key);
unsigned long  hash_string        (char *str);
unsigned long  hash_direct        (unsigned long key);
void           hash_table_insert  (HashTable *table, unsigned long key,
                                   void *value);
void           hash_table_remove  (HashTable *table, unsigned long key);

void  hash_table_foreach        (HashTable *table, HashFunc func, void *udata);
void *hash_table_find           (HashTable *table, HashFunc func, void *udata);
int   hash_table_foreach_remove (HashTable *table, HashFunc func, void *udata);

/*****************************************************************************/

#endif /* __HASH_H */
