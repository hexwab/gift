/*
 * dataset.h
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

#ifndef __DATASET_H
#define __DATASET_H

/*****************************************************************************/

/**
 * @file dataset.h
 *
 * @brief Arbitrary key/data association data structure.
 *
 * Flexible data structure used for management of arbitrary sets of data.
 * Currently supports a hash table and a linked list backend, btree should
 * come soon.
 */

/*****************************************************************************/

#include "list.h"

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/**
 * Selects the backend data structure implementation for the dataset to use.
 */
typedef enum
{
	DATASET_DEFAULT = 0,               /**< DATASET_LIST */
	DATASET_LIST,                      /**< Linked list */
	DATASET_HASH                       /**< Hash table */
} DatasetType;

/**
 * Dataset structure.
 */
typedef struct
{
	DatasetType type;                  /**< Backend implementation */

	union
	{
		struct _hash_table *hash;
		List *list;
	} tdata;
} Dataset;

/**
 *  Describes a specific key/value set.
 */
typedef struct _dataset_node
{
	void  *key;                        /**< Privately owned key */
	size_t key_len;                    /**< Length of supplied key data */
	void  *value;                      /**< Optionaly privately owned value */
	size_t value_len;                  /**< Length of value, if private */

	union
	{
		struct _dataset_node *hash_next;
		List *list_link;
	} tdata;
} DatasetNode;

/**
 * Dataset iterator callback.
 *
 * @param d     Dataset currently interating.
 * @param node  Key/value set.
 * @param udata Arbitrary user data.
 *
 * @retval TRUE  Deallocate and remove the current node.
 * @retval FALSE Continue iteration normally.
 */
typedef int (*DatasetForeach) (Dataset *d, DatasetNode *node, void *udata);

/**
 * Helper macro for properly typecasting iterator functions.
 */
#define DATASET_FOREACH(func) ((DatasetForeach)func)

/*****************************************************************************/

typedef unsigned long (*HashFunc) (Dataset *d, void *key, size_t key_len);
typedef int (*HashCmpFunc) (Dataset *d, DatasetNode *node,
                            void *key, size_t key_len);

struct _hash_table
{
	/**
	 * @name readonly
	 * Read-only public variables
	 */
	size_t        size;                /**< Total no buckets in nodes */
	unsigned long items;               /**< Total no items currently inserted */

	/**
	 * @name private
	 * Private variables
	 */
	unsigned char frozen;              /**< Do not resize hash table */

	DatasetNode **nodes;               /**< Bucket array */

	HashFunc    hash;                  /**< Hashing routine */
	HashCmpFunc cmp;                   /**< Hash/value comparison */
};

/*****************************************************************************/

/**
 * Allocate a new dataset.  Using this function is optional, see
 * ::dataset_insert for more information.
 *
 * @param type Backend implementation to use.
 */
Dataset *dataset_new (DatasetType type);

/**
 * Unallocate a dataset and all internally allocated variables
 */
void dataset_clear (Dataset *d);

/**
 * Insert a new key/value set.  If the value at d is NULL, a new dataset will
 * be allocated using DATASET_DEFAULT.
 *
 * @param key        Storage location of the lookup key.
 * @param key_len    Length of \em key for copying.
 * @param value      Storage location of the value.
 * @param value_len
 *    Length of \em value if you wish for the dataset to internally manage the
 *    values memory, otherwise 0 in which case dataset_clear will not free
 *    this address.
 */
void dataset_insert (Dataset **d,
                     void *key, size_t key_len, void *value, size_t value_len);

/**
 * Wrapper around ::dataset_insert for string constants.  Copies both key and
 * value.
 */
void dataset_insertstr (Dataset **d, char *key, char *value);

/**
 * Remove a key/value set by the key.
 */
void dataset_remove (Dataset *d, void *key, size_t key_len);

/**
 * Wrapped around ::dataset_remove for string constants.
 */
void dataset_removestr (Dataset *d, char *key);

/**
 * Lookup a value by the set's key.  If you need to know the value_len,
 * consider using ::dataset_lookup_node.
 */
void *dataset_lookup (Dataset *d, void *key, size_t key_len);

/**
 * Wrapper around ::dataset_lookup for aggregate string constants.
 */
void *dataset_lookupstr (Dataset *d, char *key);

/**
 * Lookup a complete set by the set's key.  Similar to ::dataset_lookup
 * except that the full set is returned.
 */
DatasetNode *dataset_lookup_node (Dataset *d, void *key, size_t key_len);

/**
 * Iterate an entire dataset.  This foreach routine implements the
 * ::DatasetForeach documentation described above.
 */
void dataset_foreach (Dataset *d, DatasetForeach func, void *udata);

/**
 * Iterate an entire dataset.  This foreach routine implements DatasetForeach
 * so that a return value of FALSE halts iteration.  TRUE continues normally.
 */
void dataset_foreach_ex (Dataset *d, DatasetForeach func, void *udata);

/**
 * Locate a dataset node through iteration.  DatasetForeach returns TRUE when
 * the set has been found.
 */
DatasetNode *dataset_find_node (Dataset *d, DatasetForeach func, void *udata);

/**
 * Locate a set value through iteration.  See ::dataset_find_node.
 */
void *dataset_find (Dataset *d, DatasetForeach func, void *udata);

/**
 * Calculate the dataset length (total number of sets currently inserted).
 * If you are using the DATASET_HASH implementation, this operation requires
 * no iteration.
 *
 * @retval Number of sets.
 */
unsigned long dataset_length (Dataset *d);

/**
 * Flatten a dataset to a reallocated list.  The values are not copied.
 */
List *dataset_flatten (Dataset *d);

/**
 * Find a unique, non-zero 32-bit value that is not contained in the
 * dataset.  Assumes that the dataset contains integers.
 *
 * @param counter        Hint variable for fast lookup.
 *
 * @retval A unique, non-zero 32-bit integer that is not a key of the dataset.
 */
ft_uint32 dataset_uniq32 (Dataset *d, ft_uint32 *counter);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __DATASET_H */
