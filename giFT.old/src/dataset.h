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

struct _hash_table;
typedef struct _hash_table Dataset;

/*****************************************************************************/

Dataset   *dataset_new          ();

void      *dataset_lookup       (Dataset *dataset, char *string);
void       dataset_insert_real  (Dataset *dataset, char *key, void *value);
void       dataset_remove       (Dataset *dataset, char *string);

/*****************************************************************************/

#define dataset_insert(dataset,key,value) \
do { \
	if (!dataset) \
		dataset = dataset_new (); \
	dataset_insert_real (dataset, key, (void*)value); \
} while (0)

#define dataset_clear(dataset) \
	hash_table_destroy(dataset)

#define dataset_clear_free(dataset) \
	hash_table_destroy_free(dataset)

#endif /* __DATASET_H */
