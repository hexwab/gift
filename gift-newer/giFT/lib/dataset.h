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
 * @brief Abstracted hash table structure
 *
 * Dataset is a wrapper around the \ref HashTable routines that allows for
 * keying of strings.
 */

/*****************************************************************************/

struct _hash_table;

/**
 * Dataset is simply a wrapper around the \ref HashTable to provide keying
 * by strings
 */
typedef struct _hash_table Dataset;

/*****************************************************************************/

/**
 * Allocates a new dataset manually
 */
Dataset   *dataset_new          ();

/**
 * Lookup data by key
 *
 * @param string key to lookup
 *
 * @return data found
 */
void      *dataset_lookup       (Dataset *dataset, char *string);

/**
 * Don't use this directly
 *
 * @see dataset_insert
 */
void       dataset_insert_real  (Dataset *dataset, char *key, void *value);

/**
 * Remove data by key
 *
 * @param string key to remove
 */
void       dataset_remove       (Dataset *dataset, char *string);

/*****************************************************************************/

/**
 * Insert data into the dataset
 *
 * @param key string key to insert
 * @param value arbitrary data to insert
 *
 * @see hash_table_insert
 */
#define dataset_insert(dataset,key,value) \
do { \
	if (!dataset) \
		dataset = dataset_new (); \
	dataset_insert_real (dataset, key, (void*)value); \
} while (0)

/**
 * Destroy the dataset
 *
 * @see hash_table_destroy
 */
#define dataset_clear(dataset) \
	hash_table_destroy(dataset)

/**
 * Destroy the dataset and free each node's value
 *
 * @see hash_table_destroy_free
 */
#define dataset_clear_free(dataset) \
	hash_table_destroy_free(dataset)

/*****************************************************************************/

#endif /* __DATASET_H */
