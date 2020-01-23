/*
 * $Id: as_hashtable.h,v 1.5 2004/10/03 18:00:20 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

/*
 * Adapted from: http://www.cl.cam.ac.uk/users/cwc22/hashtable/
 *
 * Copyright (C) 2002 Christopher Clark <firstname.lastname@cl.cam.ac.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __AS_HASHTABLE_H
#define __AS_HASHTABLE_H

/*****************************************************************************/

typedef struct as_hashtable_entry_t
{
	void *key;
	as_uint32 int_key;
	unsigned int key_len;

	void *val;

	/* private */
	unsigned int h;
	struct as_hashtable_entry_t *next;

} ASHashTableEntry;

typedef int (*ASHashTableCmpFunc) (ASHashTableEntry *a, ASHashTableEntry *b);
typedef unsigned int (*ASHashTableHashFunc) (ASHashTableEntry *entry);
typedef as_bool (*ASHashTableForeachFunc) (ASHashTableEntry *entry,
                                           void *udata);

typedef struct as_hashtable_t
{
	unsigned int tablelength;
	ASHashTableEntry **table;
	unsigned int entrycount;
	unsigned int loadlimit;
	unsigned int primeindex;
	ASHashTableHashFunc hashfn;
	ASHashTableCmpFunc eqfn;

	as_bool copy_keys;
	as_bool int_keys;

} ASHashTable;

/*****************************************************************************/

/* Create new hash table with cmp_func for comparing keys and hash_func for
 * hashing keys. If copy_keys is TRUE a copy of the key will be made and freed
 * automatically later.
 */
ASHashTable *as_hashtable_create (ASHashTableCmpFunc cmp_func,
                                  ASHashTableHashFunc hash_func,
                                  as_bool copy_keys);

/* Create new hash table which does a memcmp(a,b,key_len) for key comparisons
 * and uses a default hash function. If copy_keys is TRUE a copy of the key
 * will be made and freed automatically later.
 */
ASHashTable *as_hashtable_create_mem (as_bool copy_keys);

/* Create new hash table which uses 32 bit integers as keys. No hashing takes
 * place and the keys are compared as unsigned integers. This is an 
 * optimization and convenience for a common case.
 */
ASHashTable *as_hashtable_create_int ();

/* Free hash table. keys are freed as well if they were copied, values are
 * freed if specified.
 */
void as_hashtable_free (ASHashTable *table, as_bool free_values);

/*****************************************************************************/

/* Insert value into table under key. key_len is ignored if an external
 * compare and hash function have been specified. If there already is an
 * element with the same key it is updated.
 */
as_bool as_hashtable_insert (ASHashTable *table, void *key,
                             unsigned int key_len, void *value);

/* Like as_hashtable_insert but uses strlen(key)+1 as key_len. */
as_bool as_hashtable_insert_str (ASHashTable *table, unsigned char *key,
                                 void *value);

/* Insert value into hash table keyed by integers. */
as_bool as_hashtable_insert_int (ASHashTable *table, as_uint32 key,
                                 void *value);

/*****************************************************************************/

/* Remove entry with key from table. key_len is ignored if external compare
 * and hashing funtions have been specified. Returns data associated with
 * removed entry or NULL on failure.
 */
void *as_hashtable_remove (ASHashTable *table, void *key,
                           unsigned int key_len);

/* Like as_hashtable_remove but uses strlen(key)+1 as key_len. */
void *as_hashtable_remove_str (ASHashTable *table, unsigned char *key);

/* Remove value from hash table keyed by integers. */
void *as_hashtable_remove_int (ASHashTable *table, as_uint32 key);

/*****************************************************************************/

/* Return value indexed by key or NULL if there is none. key_len is ignored
 * if external compare and hashing funtions have been specified.
 */
void *as_hashtable_lookup (ASHashTable *table, void *key,
                           unsigned int key_len);

/* Like as_hashtable_find but uses strlen(key)+1 as key_len. */
void *as_hashtable_lookup_str (ASHashTable *table,
                               unsigned char *key);

/* Return value indexed by integer key */
void *as_hashtable_lookup_int (ASHashTable *table, as_uint32 key);

/*****************************************************************************/

/* Call func for each entry. If func returns TRUE the entry is removed. */
void as_hashtable_foreach (ASHashTable *table, ASHashTableForeachFunc func,
                           void *udata);

/* Returns the value of the entry for which func returned TRUE. Or NULL if
 * there was no match.
 */
void *as_hashtable_find (ASHashTable *table, ASHashTableForeachFunc func,
                         void *udata);

/*****************************************************************************/

/* Return number of entries in table */
unsigned int as_hashtable_size (ASHashTable *table);

/*****************************************************************************/

#endif /* __AS_HASHTABLE_H */
