/*
 * $Id: as_hashtable.c,v 1.9 2004/12/04 11:54:15 mkern Exp $
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

#include <math.h>
#include "as_ares.h"

/*****************************************************************************/

/* Credit for primes table: Aaron Krowne
 * http://br.endernet.org/~akrowne/
 * http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 */
static const unsigned int primes[] =
{
	53, 97, 193, 389,
	769, 1543, 3079, 6151,
	12289, 24593, 49157, 98317,
	196613, 393241, 786433, 1572869,
	3145739, 6291469, 12582917, 25165843,
	50331653, 100663319, 201326611, 402653189,
	805306457, 1610612741
};

const unsigned int prime_table_length = sizeof(primes)/sizeof(primes[0]);
const double max_load_factor = 0.65;

/* indexFor */
#if 0
static unsigned int indexFor (unsigned int tablelength,
                              unsigned int hashvalue)
{
    return (hashvalue % tablelength);
};
#else
#define indexFor(tablelength,hashvalue) ((hashvalue) % (tablelength))
#endif

/*****************************************************************************/

/* from: http://www.cs.yorku.ca/~oz/hash.html */
static unsigned int default_hash_func (ASHashTableEntry *entry)
{
	unsigned int hash = 5381;
	unsigned int i;
	unsigned char *p = (unsigned char *) entry->key;

	for (i = 0; i < entry->key_len; i++, p++)
		hash = ((hash << 5) + hash) + *p; /* hash * 33 + c */

	return hash;
}

static unsigned int default_hash_func_int (ASHashTableEntry *entry)
{
	unsigned int hash = 5381;

	/* is this good? */
	hash = ((hash << 5) + hash) + (entry->int_key & 0xFF);
	hash = ((hash << 5) + hash) + ((entry->int_key >> 8) & 0xFF);
	hash = ((hash << 5) + hash) + ((entry->int_key >> 16) & 0xFF);
	hash = ((hash << 5) + hash) + ((entry->int_key >> 24) & 0xFF);

	return hash;
}

static int default_cmp_func (ASHashTableEntry *a, ASHashTableEntry *b)
{
	if (a->key_len < b->key_len)
		return -1;
	else if (a->key_len > b->key_len)
		return 1;
	else
		return memcmp (a->key, b->key, a->key_len);
}

static int default_cmp_func_int (ASHashTableEntry *a, ASHashTableEntry *b)
{
	if (a->int_key < b->int_key)
		return -1;
	else if (a->int_key > b->int_key)
		return 1;
	else
		return 0;
}

/*****************************************************************************/

static ASHashTableEntry *hashtable_entry (void *key, unsigned int key_len,
                                          void *val, as_bool copy)
{
	ASHashTableEntry *entry;
	
	if (!(entry = malloc (sizeof (ASHashTableEntry))))
		return NULL;

	entry->next = NULL;
	entry->h = 0;
	entry->key = NULL;
	entry->int_key = 0;
	entry->key_len = key_len;

	if (copy)
	{
		if (!(entry->key = malloc (key_len)))
		{
			free (entry);
			return NULL;
		}
		memcpy (entry->key, key, key_len);
	}
	else
	{
		entry->key = key;
	}

	entry->val = val;

	return entry;
}

static ASHashTableEntry *hashtable_entry_int (as_uint32 int_key, void *val)
{
	ASHashTableEntry *entry;
	
	if (!(entry = malloc (sizeof (ASHashTableEntry))))
		return NULL;

	entry->next = NULL;
	entry->h = 0;
	entry->int_key = int_key;
	entry->val = val;

	return entry;
}

static int hashtable_expand(ASHashTable *h)
{
    /* Double the size of the table to accomodate more entries */
    ASHashTableEntry **newtable;
	ASHashTableEntry *e;
    ASHashTableEntry **pE;
    unsigned int newsize, i, index;

    /* Check we're not hitting max capacity */
    if (h->primeindex == (prime_table_length - 1)) return 0;
    newsize = primes[++(h->primeindex)];

    newtable = (ASHashTableEntry **)malloc(sizeof(ASHashTableEntry*) * newsize);
    if (NULL != newtable)
    {
        memset(newtable, 0, newsize * sizeof(ASHashTableEntry *));
        /* This algorithm is not 'stable'. ie. it reverses the list
         * when it transfers entries between the tables */
        for (i = 0; i < h->tablelength; i++) {
            while (NULL != (e = h->table[i])) {
                h->table[i] = e->next;
                index = indexFor(newsize,e->h);
                e->next = newtable[index];
                newtable[index] = e;
            }
        }
        free(h->table);
        h->table = newtable;
    }
    /* Plan B: realloc instead */
    else 
    {
        newtable = (ASHashTableEntry **)
                   realloc(h->table, newsize * sizeof(ASHashTableEntry *));
        if (NULL == newtable) { (h->primeindex)--; return 0; }
        h->table = newtable;
        memset(newtable[h->tablelength], 0, newsize - h->tablelength);
        for (i = 0; i < h->tablelength; i++) {
            for (pE = &(newtable[i]), e = *pE; e != NULL; e = *pE) {
                index = indexFor(newsize,e->h);
                if (index == i)
                {
                    pE = &(e->next);
                }
                else
                {
                    *pE = e->next;
                    e->next = newtable[index];
                    newtable[index] = e;
                }
            }
        }
    }
    h->tablelength = newsize;
    h->loadlimit   = (unsigned int) ceil(newsize * max_load_factor);
    return -1;
}


static ASHashTable *hashtable_new (unsigned int minsize,
                                   ASHashTableHashFunc hashf,
                                   ASHashTableCmpFunc eqf)
{
	ASHashTable *h;
    unsigned int pindex, size = primes[0];

    /* Check requested hashtable isn't too large */
    if (minsize > (1u << 30))
		return NULL;

    /* Enforce size as prime */
    for (pindex=0; pindex < prime_table_length; pindex++)
	{
        if (primes[pindex] > minsize)
		{
			size = primes[pindex];
			break;
		}
    }

    if (!(h = malloc (sizeof (ASHashTable))))
		return NULL;
    
    h->table = (ASHashTableEntry **) malloc (sizeof (ASHashTableEntry *) * size);

    if (!h->table)
	{
		free(h);
		return NULL;
	}

    memset (h->table, 0, size * sizeof (ASHashTableEntry *));
    h->tablelength  = size;
    h->primeindex   = pindex;
    h->entrycount   = 0;
    h->hashfn       = hashf;
    h->eqfn         = eqf;
    h->loadlimit    = (unsigned int) ceil(size * max_load_factor);
	h->copy_keys    = FALSE;
	h->int_keys     = FALSE;

    return h;
}

static void hashtable_free(ASHashTable *h, as_bool free_values)
{
    unsigned int i;
    ASHashTableEntry *e, *f;
    ASHashTableEntry **table = h->table;

	for (i = 0; i < h->tablelength; i++)
    {
		e = table[i];
        while (NULL != e)
        {
			f = e;
			e = e->next;
				
			/* free key if we copied it */
			if (h->copy_keys)
				free(f->key);

			/* free values if requested */
			if (free_values)
				free(f->val);

			free(f);
		}
	}

    free(h->table);
    free(h);
}

static void hashtable_insert (ASHashTable *h, ASHashTableEntry *in)
{
	ASHashTableEntry *e;
    unsigned int index;

    if (++(h->entrycount) > h->loadlimit)
    {
        /* Ignore the return value. If expand fails, we should
         * still try cramming just this value into the existing table
         * -- we may not have memory for a larger table, but one more
         * element may be ok. Next time we insert, we'll try expanding again.*/
        hashtable_expand(h);
    }

    in->h = h->hashfn (in);
    index = indexFor(h->tablelength,in->h);

	/* look for already present key */
    e = h->table[index];
    while (NULL != e)
    {
        /* Check hash value to short circuit heavier comparison */
        if ((in->h == e->h) && (h->eqfn(in, e) == 0))
        {
			/* found key, replace value and key pointer */
			e->val = in->val;
	
			if (!h->int_keys)
			{
				if (h->copy_keys)
					free (e->key);
				e->key = in->key;
			}

			/* free new entry since we used the old */
			free (in);

			/* entry count hasn't changed */
			h->entrycount--;
            return;
        }
        e = e->next;
    }

	/* no key present insert new one */
    in->next = h->table[index];
    h->table[index] = in;
}

void *hashtable_remove (ASHashTable *h, ASHashTableEntry *rm)
{
    /* TODO: consider compacting the table when the load factor drops enough,
     *       or provide a 'compact' method. */

    ASHashTableEntry *e;
    ASHashTableEntry **pE;
    void *v;
    unsigned int hashvalue, index;

    hashvalue = h->hashfn (rm);
    index = indexFor(h->tablelength,hashvalue);
    pE = &(h->table[index]);
    e = *pE;
    while (NULL != e)
    {
        /* Check hash value to short circuit heavier comparison */
        if ((hashvalue == e->h) && (h->eqfn(rm, e) == 0))
        {
            *pE = e->next;
            h->entrycount--;
            v = e->val;
			if (h->copy_keys)
				free(e->key);
            free(e);
            return v;
        }
        pE = &(e->next);
        e = e->next;
    }
    return NULL;
}

void *hashtable_search (ASHashTable *h, ASHashTableEntry *sr)
{
    ASHashTableEntry *e;
    unsigned int hashvalue, index;
    hashvalue = h->hashfn (sr);
    index = indexFor(h->tablelength,hashvalue);
    e = h->table[index];
    while (NULL != e)
    {
        /* Check hash value to short circuit heavier comparison */
        if ((hashvalue == e->h) && (h->eqfn(sr, e) == 0))
			return e->val;
        e = e->next;
    }
    return NULL;
}

/*****************************************************************************/

/* Create new hash table with cmp_func for comparing keys and hash_func for
 * hashing keys. If copy_keys is TRUE a copy of the key will be made and freed
 * automatically later.
 */
ASHashTable *as_hashtable_create (ASHashTableCmpFunc cmp_func,
                                  ASHashTableHashFunc hash_func,
							      as_bool copy_keys)
{
	ASHashTable *table;

	if (!(table = hashtable_new (1, hash_func, cmp_func)))
		return NULL;

	table->copy_keys = copy_keys;

	return table;
}

/* Create new hash table which does a memcmp(a,b,key_len) for key comparisons
 * and uses a default hash function. If copy_keys is TRUE a copy of the key
 * will be made and freed automatically later.
 */
ASHashTable *as_hashtable_create_mem (as_bool copy_keys)
{
	ASHashTable *table;

	if (!(table = hashtable_new (1, default_hash_func, default_cmp_func)))
		return NULL;

	table->copy_keys = copy_keys;

	return table;
}

/* Create new hash table which uses 32 bit integers as keys. No hashing takes
 * place and the keys are compared as unsigned integers. This is an 
 * optimization and convenience for a common case.
 */
ASHashTable *as_hashtable_create_int ()
{
	ASHashTable *table;

	if (!(table = hashtable_new (1, default_hash_func_int,
	                             default_cmp_func_int)))
		return NULL;

	table->copy_keys = FALSE;
	table->int_keys = TRUE;

	return table;
}

/* Free hash table. keys are freed as well if they were copied, values remain
 * untouched.
 */
void as_hashtable_free (ASHashTable *table, as_bool free_values)
{
	if (!table)
		return;

	if (free_values && table->int_keys)
	{
		assert (table->int_keys == FALSE);
		free_values = FALSE;
	}

	hashtable_free (table, free_values);
}

/*****************************************************************************/

/* Insert value into table under key. key_len is ignored if an external
 * compare and hash function have been specified. If there already is an
 * element with the same key it is updated.
 */
as_bool as_hashtable_insert (ASHashTable *table, void *key,
                             unsigned int key_len, void *value)
{
	ASHashTableEntry *entry;
	
	assert (table->int_keys == FALSE);

	if (!(entry = hashtable_entry (key, key_len, value, table->copy_keys)))
		return FALSE;

	hashtable_insert (table, entry);

	return TRUE;
}

/* Like as_hashtable_insert but uses strlen(key)+1 as key_len. */
as_bool as_hashtable_insert_str (ASHashTable *table, unsigned char *key,
                                 void *value)
{
	return as_hashtable_insert (table, key, strlen (key) + 1, value);
}

/* Insert value into hash table keyed by integers. */
as_bool as_hashtable_insert_int (ASHashTable *table, as_uint32 key,
                                 void *value)
{
	ASHashTableEntry *entry;
	
	assert (table->int_keys == TRUE);

	if (!(entry = hashtable_entry_int (key, value)))
		return FALSE;

	hashtable_insert (table, entry);

	return TRUE;
}

/*****************************************************************************/

/* Remove entry with key from table. key_len is ignored if external compare
 * and hashing funtions have been specified. Returns data associated with
 * removed entry or NULL on failure.
 */
void *as_hashtable_remove (ASHashTable *table, void *key,
                           unsigned int key_len)
{
	ASHashTableEntry entry;
	
	assert (table->int_keys == FALSE);

	entry.key = key;
	entry.key_len = key_len;

	return hashtable_remove (table, &entry);
}

/* Like as_hashtable_remove but uses strlen(key)+1 as key_len. */
void *as_hashtable_remove_str (ASHashTable *table, unsigned char *key)
{
	return as_hashtable_remove (table, key, strlen (key) + 1);
}

/* Remove value from hash table keyed by integers. */
void *as_hashtable_remove_int (ASHashTable *table, as_uint32 key)
{
	ASHashTableEntry entry;
	
	assert (table->int_keys == TRUE);

	entry.int_key = key;
	
	return hashtable_remove (table, &entry);
}

/*****************************************************************************/

/* Return entry indexed by key or NULL if there is none. key_len is ignored
 * if external compare and hashing funtions have been specified.
 */
void *as_hashtable_lookup (ASHashTable *table, void *key,
                           unsigned int key_len)
{
	ASHashTableEntry entry;
	
	assert (table->int_keys == FALSE);

	entry.key = key;
	entry.key_len = key_len;

	return hashtable_search (table, &entry);
}

/* Like as_hashtable_find but uses strlen(key)+1 as key_len. */
void *as_hashtable_lookup_str (ASHashTable *table,
                               unsigned char *key)
{
	return as_hashtable_lookup (table, key, strlen (key) + 1);
}

/* Return entry indexed by integer key */
void *as_hashtable_lookup_int (ASHashTable *table, as_uint32 key)
{
	ASHashTableEntry entry;
	
	assert (table->int_keys == TRUE);

	entry.int_key = key;

	return hashtable_search (table, &entry);
}

/*****************************************************************************/

/* Call func for each entry. If func returns TRUE the entry is removed. */
void as_hashtable_foreach (ASHashTable *table, ASHashTableForeachFunc func,
                           void *udata)
{
	ASHashTableEntry *e = NULL;
    ASHashTableEntry *parent = NULL;
    ASHashTableEntry *next;
	ASHashTableEntry *remember_e = NULL, *remember_parent = NULL;
	unsigned int index = table->tablelength;
	unsigned int i, j,tablelength;
	as_bool remove;

    if (table->entrycount == 0)
		return;

	/* Needs a cleanup, but should work now. */

    for (i = 0; i < table->tablelength; i++)
    {
        if (table->table[i])
        {
            e = table->table[i];
            index = i;
            break;
        }
    }

	for (;;)
	{
		remove = func (e, udata);

		if (remove)
		{
			if (!parent)
				table->table[index] = e->next;
			else
				parent->next = e->next;

			/* itr->e is now outside the hashtable */
			remember_e = e;
			table->entrycount--;
			if (table->copy_keys)
				free (remember_e->key);
	
		    /* Advance the iterator, correcting the parent */
			remember_parent = parent;
		}

		/* move on to next entry */
		next = e->next;
		if (next)
		{
	        parent = e;
			e = next;

			if (remove)
			{
				if (parent == remember_e)
					parent = remember_parent;
				free (remember_e);
			}
			continue;
		}

		tablelength = table->tablelength;
		parent = NULL;

		if (tablelength <= (j = ++(index)))
			return;

		while (!(next = table->table[j]))
		{
	        if (++j >= tablelength)
				return;
		}
		index = j;
		e = next;

		if (remove)
		{
			if (parent == remember_e)
				parent = remember_parent;
			free (remember_e);
		}
	}
}

/* Returns the value of the entry for which func returned TRUE. Or NULL if
 * there was no match.
 */
void *as_hashtable_find (ASHashTable *table, ASHashTableForeachFunc func,
                         void *udata)
{
	ASHashTableEntry *e = NULL;
    ASHashTableEntry *parent = NULL;
    ASHashTableEntry *next;
    unsigned int index = table->tablelength;
	unsigned int i, j,tablelength;

    if (table->entrycount == 0)
		return NULL;

    for (i = 0; i < table->tablelength; i++)
    {
        if (table->table[i])
        {
            e = table->table[i];
            index = i;
            break;
        }
    }

	for (;;)
	{
		if (func (e, udata))
			return e->val;

		/* move on to next entry */
		next = e->next;
		if (next)
		{
	        parent = e;
			e = next;
			continue;
		}
		tablelength = table->tablelength;
		parent = NULL;

		if (tablelength <= (j = ++(index)))
			return NULL;

		while (!(next = table->table[j]))
		{
	        if (++j >= tablelength)
				return NULL;
		}
		index = j;
		e = next;
	}
}

/*****************************************************************************/

/* Return number of entries in table */
unsigned int as_hashtable_size (ASHashTable *table)
{
	return table->entrycount;
}

/*****************************************************************************/
