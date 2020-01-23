/*
 * list_dataset.h
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

#ifndef __LIST_KV_H
#define __LIST_KV_H

/*****************************************************************************/

/**
 * @file list_dataset.h
 *
 * Abstracted linked list structure to emulate a \ref Dataset interface
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

typedef struct
{
	/* TODO -- add hash_func, etc etc to provide a real mapping
	 * data structure */
	List *list;
} ListDataset;

/*****************************************************************************/

void *list_dataset_lookup (ListDataset **list_ds, char *key);
void  list_dataset_insert (ListDataset **list_ds, char *key, void *value);
void  list_dataset_remove (ListDataset **list_ds, char *key);

void  list_dataset_clear      (ListDataset **list_ds);
void  list_dataset_clear_free (ListDataset **list_ds);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __LIST_KV_H */
