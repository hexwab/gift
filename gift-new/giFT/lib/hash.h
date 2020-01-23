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

/**
 * @file hash.h
 *
 * @brief Hash table routines
 *
 * Basic interface to a hash table.  See dataset.h if you wish to use
 * character arrays (rather than longs) as keys.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/**
 * Called by the foreach functions to iterate a table.  Return \ref TRUE if
 * you wish for the table to continue iterating and \ref FALSE otherwise
 */
typedef int (*HashFunc) (unsigned long key, void *value, void *udata);

/**
 * Compare two node values, not necessarily hash keys.
 */
typedef int (*HashEqualFunc) (unsigned long key1, unsigned long key2);

/**
 * Create a new hash key
 */
typedef unsigned long (*HashKeyFunc) (unsigned long key);

typedef struct _hash_node
{
	unsigned long      key;
	void              *value;

	struct _hash_node *next;
} HashNode;

/**
 * Generic hash table routine copied from GLib
 */
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

/**
 * Creates a new hash table
 */
HashTable *hash_table_new ();

/**
 * Destroy the hash table.  This does not affect any data inserted into the
 * table
 */
void hash_table_destroy (HashTable *table);

/**
 * Destroy the hash table and free each hash node value
 */
void hash_table_destroy_free (HashTable *table);

/*****************************************************************************/

/**
 * Lookup a value in the table by its key
 *
 * @return Data found
 */
void *hash_table_lookup  (HashTable *table, unsigned long key);

/**
 * Insert a new value into the hash table
 *
 * @note If you wish to insert an integer type as value please use
 * \ref I_PTR
 */
void hash_table_insert  (HashTable *table, unsigned long key, void *value);

/**
 * Remove a value by its key
 */
void hash_table_remove  (HashTable *table, unsigned long key);

/**
 * Loop through each hash node in the table
 *
 * @param func Hash foreach function to call for each valid node found
 * @param udata Arbitrary data you wish to have passed along
 */
void hash_table_foreach        (HashTable *table, HashFunc func, void *udata);

/**
 * Attempt to locate a node's value.  \a func should return \ref TRUE when
 * found
 *
 * @param func Hash foreach function to call for each valid node found
 * @param udata Arbitrary data you wish to have passed along
 *
 * @return Value inserted into the database if found, otherwise NULL
 */
void *hash_table_find           (HashTable *table, HashFunc func, void *udata);

/**
 * Same as \ref hash_table_find except that it returns the nodes key, rather
 * than its value
 */
unsigned long hash_table_find_key (HashTable *table, HashFunc func, void *udata);

/**
 * Remove hash table nodes while iterating.  \a func should return \ref TRUE
 * if you wish to remove the node supplied to it
 *
 * @see hash_table_foreach
 *
 * @return Total number of nodes removed
 */
int   hash_table_foreach_remove (HashTable *table, HashFunc func, void *udata);

/*****************************************************************************/

/**
 * Hashes a string before it is inserted into the table.  Avoid calling this
 * function directly, instead see dataset.h
 *
 * @param str String to hash
 *
 * @return New hash key
 */
unsigned long  hash_string        (char *str);

/**
 * Hashes an integer for more efficient hash table lookup.  This is currently
 * unused and should probably stay that way
 */
unsigned long  hash_direct        (unsigned long key);

/**
 * "Flatten" a hash table into a simple linked list so that it may be
 * sorted.  Only use this function if you're \em really lazy :)
 *
 * @return copy of the table as a linked list
 */
List          *hash_flatten       (HashTable *table);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __HASH_H */
