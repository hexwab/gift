/*
 * dataset.c
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

#include "dataset.h"

/*****************************************************************************/

static const unsigned int primes[] =
{
	11,       19,       37,       73,       109,      163,
	251,      367,      557,      823,      1237,     1861,
	2777,     4177,     6247,     9371,     14057,    21089,
	31627,    47431,    71143,    106721,   160073,   240101,
	360163,   540217,   810343,   1215497,  1823231,  2734867,
	4102283,  6153409,  9230113,  13845163,
};

static size_t nprimes = sizeof (primes) / sizeof (primes[0]);

#define HASH_TABLE_MIN_SIZE 11
#define HASH_TABLE_MAX_SIZE 13845163

/*****************************************************************************/

static int assign_dt (void **out, size_t *out_len, void *in, size_t in_len)
{
	/* determine whether or not the caller wishes for us to make an internally
	 * managed copy of the memory provided
	 * NOTE: len of 0 indicates that this memory is not managed by dataset.c */
	if (in_len == 0)
	{
		*out     = in;
		*out_len = 0;
	}
	else
	{
		if (!(*out = malloc (in_len)))
			return FALSE;

		memcpy (*out, in, in_len);
		*out_len = in_len;
	}

	return TRUE;
}

static void dataset_free_node (Dataset *d, DatasetNode *node)
{
	if (!d || !node)
		return;

	if (node->key_len)
		free (node->key);

	if (node->value_len)
		free (node->value);

	free (node);
}

static DatasetNode *dataset_new_node (Dataset *d,
                                      void *key, size_t key_len,
                                      void *value, size_t value_len)
{
	DatasetNode *node;

	if (!d || !key || !key_len)
		return NULL;

	if (!(node = malloc (sizeof (DatasetNode))))
		return NULL;

	if (!assign_dt (&node->key, &node->key_len, key, key_len) ||
		!assign_dt (&node->value, &node->value_len, value, value_len))
	{
		dataset_free_node (d, node);
		return NULL;
	}

	memset (&node->tdata, 0, sizeof (node->tdata));

	return node;
}

static int dataset_cmp_node (DatasetNode *node, void *key, size_t key_len)
{
	if (node->key_len != key_len)
		return (node->key_len - key_len);

	return memcmp (node->key, key, node->key_len);
}

static void dataset_set_node (Dataset *d, DatasetNode *node,
                              void *key, size_t key_len,
                              void *value, size_t value_len)
{
	if (dataset_cmp_node (node, key, key_len))
	{
		if (node->key_len != key_len)
		{
			void *key_dup;

			if (!(key_dup = realloc (node->key, key_len)))
				return;

			node->key     = key_dup;
			node->key_len = key_len;
		}

		memcpy (node->key, key, key_len);
	}

	/* TODO: opt */
	if (node->value_len)
		free (node->value);

	assign_dt (&node->value, &node->value_len, value, value_len);
}

/*****************************************************************************/

/* TODO -- provide a much better generic hashing function */
static unsigned long hash_direct (Dataset *d, void *key, size_t key_len)
{
	unsigned long key_hash = 0;

	memcpy (&key_hash, key, MIN (key_len, sizeof (key_hash)));

	return key_hash;
}

static int hash_cmp (Dataset *d, DatasetNode *node, void *key, size_t key_len)
{
	return dataset_cmp_node (node, key, key_len);
}

static unsigned int closest_prime (unsigned int num)
{
	unsigned int i;

	for (i = 0; i < nprimes; i++)
		if (primes[i] > num)
			return primes[i];

	return primes[i - 1];
}

static void d_hash_resize (Dataset *d)
{
	struct _hash_table *table = d->tdata.hash;
	DatasetNode **new_nodes;
	DatasetNode *node;
	DatasetNode *next;
	float nodes_per_list;
	unsigned int hash_val;
	int new_size;
	unsigned int i;

	nodes_per_list = (float) table->items / (float) table->size;

	if ((nodes_per_list > 0.3 || table->size <= HASH_TABLE_MIN_SIZE) &&
		(nodes_per_list < 3.0 || table->size >= HASH_TABLE_MAX_SIZE))
		return;

	new_size = closest_prime (table->items);

	if (new_size < HASH_TABLE_MIN_SIZE)
		new_size = HASH_TABLE_MIN_SIZE;

	if (new_size > HASH_TABLE_MAX_SIZE)
		new_size = HASH_TABLE_MAX_SIZE;

	new_nodes = calloc (sizeof (DatasetNode *), new_size);

	for (i = 0; i < table->size; i++)
	{
		for (node = table->nodes[i]; node; node = next)
		{
			next = node->tdata.hash_next;

			hash_val = table->hash (d, node->key, node->key_len) % new_size;

			node->tdata.hash_next = new_nodes[hash_val];
			new_nodes[hash_val] = node;
		}
	}

	free (table->nodes);
	table->nodes = new_nodes;
	table->size  = new_size;
}

static void d_hash_new (Dataset *d)
{
	struct _hash_table *table;
	size_t tsize;

	if (!(table = malloc (sizeof (struct _hash_table))))
		return;

	table->size   = HASH_TABLE_MIN_SIZE;
	table->items  = 0;
	table->frozen = FALSE;

	tsize = (table->size * (sizeof (DatasetNode *)));

	if (!(table->nodes = malloc (tsize)))
	{
		free (table);
		return;
	}

	memset (table->nodes, 0, tsize);

	table->hash = hash_direct;
	table->cmp  = hash_cmp;

	d->tdata.hash = table;
}

static void d_hash_free (Dataset *d)
{
	free (d->tdata.hash->nodes);
	free (d->tdata.hash);
}

/*****************************************************************************/

static void d_list_free (Dataset *d)
{
	list_free (d->tdata.list);
}

/*****************************************************************************/

Dataset *dataset_new (DatasetType type)
{
	Dataset *d;

	if (!(d = malloc (sizeof (Dataset))))
		return NULL;

	memset (d, 0, sizeof (Dataset));

	d->type = (type == DATASET_DEFAULT) ? DATASET_LIST : type;

	switch (d->type)
	{
	 case DATASET_HASH:
		d_hash_new (d);
		break;
	 default:
		break;
	}

	return d;
}

static int clear (Dataset *d, DatasetNode *node, int *free_value)
{
#if 0
	if (*free_value)
		free (node->value);
#endif

	return TRUE;
}

static void clear_free (Dataset *d, int free_value)
{
	if (!d)
		return;

	dataset_foreach (d, DATASET_FOREACH (clear), &free_value);

	switch (d->type)
	{
	 case DATASET_HASH:
		d_hash_free (d);
		break;
	 case DATASET_LIST:
		d_list_free (d);
		break;
	 default:
		break;
	}

	free (d);
}

void dataset_clear (Dataset *d)
{
	clear_free (d, FALSE);
}

#if 0
void dataset_clear_free (Dataset *d)
{
	clear_free (d, TRUE);
}
#endif

/*****************************************************************************/

static int d_list_lookup_by_key (DatasetNode *node, void **args)
{
	void  *key     =              args[0];
	size_t key_len = *((size_t *) args[1]);

	return dataset_cmp_node (node, key, key_len);
}

static DatasetNode *d_list_lookup_node (Dataset *d, void *key, size_t key_len)
{
	void *args[] = { key, &key_len };
	List *link;

	link = list_find_custom (d->tdata.list, args, (CompareFunc) d_list_lookup_by_key);

	return (link ? link->data : NULL);
}

static DatasetNode **d_hash_lookup_node (Dataset *d, void *key, size_t key_len)
{
	DatasetNode **node;
	unsigned long hash;

	hash = d->tdata.hash->hash (d, key, key_len);
	node = &d->tdata.hash->nodes [hash % d->tdata.hash->size];

	while (*node && d->tdata.hash->cmp (d, *node, key, key_len))
		node = & (*node)->tdata.hash_next;

#if 0
	/* if we're attempting to lookup data we don't want to find a useless
	 * node */
	if (lookup && !(*node))
		return NULL;
#endif

	return node;
}

DatasetNode *dataset_lookup_node (Dataset *d, void *key, size_t key_len)
{
	DatasetNode *node = NULL;

	if (!d || !key || !key_len)
		return NULL;

	switch (d->type)
	{
	 case DATASET_LIST:
		node = d_list_lookup_node (d, key, key_len);
		break;
	 case DATASET_HASH:
		node = *d_hash_lookup_node (d, key, key_len);
		break;
	 default:
		break;
	}

	return node;
}

/*****************************************************************************/

static void d_list_insert (Dataset *d, DatasetNode *node)
{
	d->tdata.list = list_prepend (d->tdata.list, node);
	node->tdata.list_link = d->tdata.list;
}

static void d_hash_insert (Dataset *d, DatasetNode *node)
{
	DatasetNode **placement;

	placement = d_hash_lookup_node (d, node->key, node->key_len);
	*placement = node;

	d->tdata.hash->items++;

	if (!d->tdata.hash->frozen)
		d_hash_resize (d);
}

static void insert (Dataset **d,
                    void *key, size_t key_len,
                    void *value, size_t value_len)
{
	DatasetNode *node;

	if (!d || !key || !key_len)
		return;

	if (!(*d) && !(*d = dataset_new (DATASET_DEFAULT)))
		return;

	/* create the node structure we will be inserting
	 * NOTE: if we already find a node we will just modify its values */
	if (!(node = dataset_lookup_node (*d, key, key_len)))
		node = dataset_new_node (*d, key, key_len, value, value_len);
	else
	{
		dataset_set_node (*d, node, key, key_len, value, value_len);
		return;
	}

	if (!node)
		return;

	switch ((*d)->type)
	{
	 case DATASET_LIST:
		d_list_insert (*d, node);
		break;
	 case DATASET_HASH:
		d_hash_insert (*d, node);
		break;
	 default:
		break;
	}
}

void dataset_insert (Dataset **d,
                     void *key, size_t key_len,
                     void *value, size_t value_len)
{
	assert (key != NULL);
	assert (key_len > 0);

	insert (d, key, key_len, value, value_len);
}

void dataset_insertstr (Dataset **d, char *key, char *value)
{
	assert (key != NULL);
	assert (value != NULL);

	insert (d, (void *)key, STRLEN_0 (key), (void *)value, STRLEN_0 (value));
}

/*****************************************************************************/

static void d_list_remove (Dataset *d, DatasetNode *node)
{
	if (!node->tdata.list_link)
		d->tdata.list = list_remove (d->tdata.list, node);
	else
	{
		d->tdata.list = list_remove_link (d->tdata.list, node->tdata.list_link);
		node->tdata.list_link = NULL;
	}
}

static void d_hash_remove (Dataset *d, DatasetNode *node)
{
	DatasetNode *dest;
	DatasetNode **placement;

	/* this is very inefficient, fix later */
	placement = d_hash_lookup_node (d, node->key, node->key_len);

	dest = *placement;
	*placement = dest->tdata.hash_next;

	d->tdata.hash->items--;

	if (!d->tdata.hash->frozen)
		d_hash_resize (d);
}

void dataset_remove (Dataset *d, void *key, size_t key_len)
{
	DatasetNode *node;

	if (!(node = dataset_lookup_node (d, key, key_len)))
		return;

	switch (d->type)
	{
	 case DATASET_LIST:
		d_list_remove (d, node);
		break;
	 case DATASET_HASH:
		d_hash_remove (d, node);
		break;
	 default:
		break;
	}

	dataset_free_node (d, node);
}

void dataset_removestr (Dataset *d, char *key)
{
	dataset_remove (d, key, STRLEN_0 (key));
}

/*****************************************************************************/

void *dataset_lookup (Dataset *d, void *key, size_t key_len)
{
	DatasetNode *node;

	if (!(node = dataset_lookup_node (d, key, key_len)))
		return NULL;

	return node->value;
}

void *dataset_lookupstr (Dataset *d, char *key)
{
	return dataset_lookup (d, key, STRLEN_0 (key));
}

/*****************************************************************************/

static int d_list_foreach_remove (DatasetNode *node, void **args)
{
	Dataset       *d     = args[0];
	DatasetForeach func  = args[1];
	void          *udata = args[2];
	int            ret;

	if ((ret = func (d, node, udata)))
		dataset_free_node (d, node);

	return ret;
}

static int d_hash_foreach_remove (Dataset *d, DatasetForeach func, void *udata)
{
	DatasetNode *node;
	DatasetNode *prev, *next;
	unsigned int i;

	for (i = 0; i < d->tdata.hash->size; i++)
	{
restart:
		prev = NULL;

		for (node = d->tdata.hash->nodes[i]; node; )
		{
			next = node->tdata.hash_next;

			if (func (d, node, udata))
			{
				d->tdata.hash->items--;

				if (prev)
				{
					prev->tdata.hash_next = node->tdata.hash_next;
					dataset_free_node (d, node);
					node = prev;
				}
				else
				{
					d->tdata.hash->nodes[i] = node->tdata.hash_next;
					dataset_free_node (d, node);
					goto restart;
				}
			}

			prev = node;
			node = next;
		}
	}

	if (!d->tdata.hash->frozen)
		d_hash_resize (d);

	return TRUE;
}

void dataset_foreach (Dataset *d, DatasetForeach func, void *udata)
{
	void *args[] = { d, func, udata };

	if (!d || !func)
		return;

	switch (d->type)
	{
	 case DATASET_LIST:
		d->tdata.list = list_foreach_remove (d->tdata.list, (ListForeachFunc) d_list_foreach_remove, args);
		break;
	 case DATASET_HASH:
		d_hash_foreach_remove (d, func, udata);
		break;
	 default:
		break;
	}
}

/*****************************************************************************/

static int d_list_foreach (DatasetNode *node, void **args)
{
	Dataset       *d     = args[0];
	DatasetForeach func  = args[1];
	void          *udata = args[2];

	return func (d, node, udata);
}

static int d_hash_foreach (Dataset *d, DatasetForeach func, void *udata)
{
	DatasetNode *node;
	unsigned int i;

	for (i = 0; i < d->tdata.hash->size; i++)
	{
		for (node = d->tdata.hash->nodes[i]; node;
		     node = node->tdata.hash_next)
		{
			if (!(func (d, node, udata)))
				return FALSE;
		}
	}

	return TRUE;
}

void dataset_foreach_ex (Dataset *d, DatasetForeach func, void *udata)
{
	void *args[] = { d, func, udata };

	if (!d || !func)
		return;

	switch (d->type)
	{
	 case DATASET_LIST:
		list_foreach (d->tdata.list, (ListForeachFunc) d_list_foreach, args);
		break;
	 case DATASET_HASH:
		d_hash_foreach (d, func, udata);
		break;
	 default:
		break;
	}
}

/*****************************************************************************/

static int find_wrap (Dataset *d, DatasetNode *node, void **args)
{
	DatasetForeach func  = args[0];
	void          *udata = args[1];

	if (args[2])
		return FALSE;

	if (func (d, node, udata))
	{
		args[2] = node;
		return FALSE;
	}

	return TRUE;
}

DatasetNode *dataset_find_node (Dataset *d, DatasetForeach func, void *udata)
{
	void *args[] = { func, udata, NULL };

	if (!d || !func)
		return NULL;

	dataset_foreach_ex (d, DATASET_FOREACH (find_wrap), args);

	return args[2];
}

void *dataset_find (Dataset *d, DatasetForeach func, void *udata)
{
	DatasetNode *node;

	if (!(node = dataset_find_node (d, func, udata)))
		return NULL;

	return node->value;
}

/*****************************************************************************/

unsigned long dataset_length (Dataset *d)
{
	unsigned long count = 0;

	if (!d)
		return count;

	switch (d->type)
	{
	 case DATASET_LIST:
		count = list_length (d->tdata.list);
		break;
	 case DATASET_HASH:
		count = d->tdata.hash->items;
		break;
	 default:
		break;
	}

	return count;
}

/*****************************************************************************/

static int flatten_node (Dataset *d, DatasetNode *node, List **list)
{
	*list = list_append (*list, node->value);
	return FALSE;
}

List *dataset_flatten (Dataset *d)
{
	List *list = NULL;

	dataset_foreach (d, DATASET_FOREACH (flatten_node), &list);

	return list;
}

/*****************************************************************************/

ft_uint32 dataset_uniq32 (Dataset *d, ft_uint32 *counter)
{
	ft_uint32 cnt = 0;

	if (counter)
		cnt = *counter;

	cnt++;

	while (cnt == 0 || dataset_lookup (d, &cnt, sizeof (cnt)))
		cnt++;

	/* store the counter for the next call */
	if (counter)
		*counter = cnt;

	return cnt;
}

/*****************************************************************************/

#if 0
int main ()
{
	Dataset *d   = NULL;
	struct _foo { int x; int y; } foo;
	char   *bar = "bar";

	d = dataset_new (DATASET_LIST);

	foo.x = 10;
	foo.y = 15;
	dataset_insert (&d, &foo, sizeof (foo), bar, STRLEN_0 (bar));

	foo.x = 5;
	assert (dataset_lookup (d, &foo, sizeof (foo)) == NULL);

	foo.x = 10;
	bar = dataset_lookup (d, &foo, sizeof (foo));

	printf ("%i,%i = %s\n", foo.x, foo.y, bar);

	dataset_insert (&d, &foo, sizeof (foo), bar, 0);
	dataset_clear (d);

	return 0;
}
#endif

#if 0
static int foreach (Dataset *d, DatasetNode *node, int *cmp)
{
	if (memcmp (node->key, cmp, sizeof (*cmp)))
		return FALSE;

	printf ("removing %i\n", *cmp);
	dataset_remove (d, node->key, node->key_len);
	return FALSE;
}

int main ()
{
	Dataset *d = dataset_new (DATASET_HASH);
	int foo;

	foo = 1;
	dataset_insert (&d, &foo, sizeof (foo), "1", 2);

	foo = 2;
	dataset_insert (&d, &foo, sizeof (foo), "2", 2);

	foo = 3;
	dataset_insert (&d, &foo, sizeof (foo), "3", 2);

	foo = 2;
	dataset_foreach (d, DATASET_FOREACH (foreach), &foo);

	assert (dataset_lookup (d, &foo, sizeof (foo)) == NULL);
	dataset_clear (d);

	return 0;
}
#endif
