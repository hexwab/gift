/*
 * $Id: asp_hashmap.c,v 1.2 2004/12/19 00:50:14 mkern Exp $
 *
 * Copyright (C) 2004 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#include "asp_plugin.h"

/*****************************************************************************/

typedef struct
{
	char *name;
	size_t size;
} MapEntry;

static ASHashTable *map = NULL;

/*****************************************************************************/

/* Init hash map. */
void asp_hashmap_init (void)
{
	assert (!map);

	map = as_hashtable_create_mem (TRUE);
}

static as_bool free_entry (ASHashTableEntry *entry, void *udata)
{
	MapEntry *e;

	e = entry->val;
	
	free (e->name);
	free (e);

	return TRUE;
}

/* Free hash map. */
void asp_hashmap_destroy (void)
{
	assert (map);

	as_hashtable_foreach (map, (ASHashTableForeachFunc)free_entry, NULL);

	as_hashtable_free (map, FALSE);
}

/*****************************************************************************/

/* Lookup file name and size by hash. Returns TRUE if an entry was found and
 * sets name and size. Caller must not modify returned name.
 */
as_bool asp_hashmap_lookup (ASHash *hash, char **name, size_t *size)
{
	MapEntry *e;

	if (!map || !(e = as_hashtable_lookup (map, hash->data, AS_HASH_SIZE)))
		return FALSE;

	AS_HEAVY_DBG_3 ("found hashmap entry for '%s': '%s', %u",
	                as_hash_str (hash), STRING_NOTNULL (e->name), e->size);

	if (name)
		*name = e->name;
	if (size)
		*size = e->size;

	return TRUE;
}

/* Insert file name and size for given hash. */
void asp_hashmap_insert (ASHash *hash, char *name, size_t size)
{
	MapEntry *e, oe;
	
	if (!map)
		return;

	if (asp_hashmap_lookup (hash, &oe.name, &oe.size))
	{
		if (oe.size != size)
		{
			AS_WARN_4 ("cached size %u for hash %s ('%s') "
			           "differs from inserted size %u", 
			           oe.size, as_hash_str (hash),
			           STRING_NOTNULL (name ? name : oe.name), size);
		}

		return;
	}

	if (!(e = malloc (sizeof(MapEntry))))
		return;

	e->name = STRDUP (name);
	e->size = size;
	
	if (!as_hashtable_insert (map, hash->data, AS_HASH_SIZE, e))
	{
		free (e->name);
		free (e);
	}
}

/*****************************************************************************/
