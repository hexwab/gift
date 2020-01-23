/*
 * hash.c - shameless glib ripoff
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

/*****************************************************************************/

#define HASH_TABLE_MIN_SIZE 11
#define HASH_TABLE_MAX_SIZE 13845163

/*****************************************************************************/

static HashNode *hash_node_destroy (HashNode *node, HashTable *table)
{
	HashNode *next = NULL;

	if (node && node->next)
		next = node->next;

	if (table->key_remove_func)
		table->key_remove_func (node->key, node->value, NULL);

	free (node);

	return next;
}

static void hash_nodes_destroy (HashNode *node, HashTable *table)
{
	if (!node)
		return;

	while (node)
		node = hash_node_destroy (node, table);
}

HashTable *hash_table_new ()
{
	HashTable *table;
	size_t tsize;

	table = malloc (sizeof (HashTable));
	table->size   = 11;
	table->items  = 0;
	table->frozen = 0;

	tsize = (table->size * (sizeof (HashNode *)));
	table->nodes = malloc (tsize);
	memset (table->nodes, 0, tsize);

	table->hash_func = hash_direct;
	table->key_equal_func = NULL;
	table->key_remove_func = NULL;

	return table;
}

void hash_table_destroy (HashTable *table)
{
	unsigned int i;

	if (!table)
		return;

	for (i = 0; i < table->size; i++)
		hash_nodes_destroy (table->nodes[i], table);

	free (table->nodes);
	free (table);
}

static int destroy_free (unsigned long key, char *value, void *udata)
{
	free (value);

	return TRUE;
}

void hash_table_destroy_free (HashTable *table)
{
	if (table)
	{
		hash_table_foreach_remove (table, (HashFunc) destroy_free, NULL);
		hash_table_destroy (table);
	}
}

static int hash_flatten_node (unsigned long key, void *value, List **list)
{
	*list = list_append (*list, value);

	return 1;
}

/* converts a hash table into a linked list (temporary) */
List *hash_flatten (HashTable *table)
{
	List *list = NULL;

	hash_table_foreach (table, (HashFunc) hash_flatten_node, &list);

	return list;
}

/*****************************************************************************/

static HashNode **hash_table_lookup_node (HashTable *table,
                                          unsigned long key)
{
	HashNode **node;
	unsigned long hash;

	hash = table->hash_func (key);
	node = &table->nodes [hash % table->size];

	/* Optimize compare outside of loop */
	if (table->key_equal_func)
		while (*node && !table->key_equal_func ((*node)->key, key))
			node = & (*node)->next;
	else
		while (*node && (*node)->key != key)
			node = & (*node)->next;

	return node;
}

void *hash_table_lookup (HashTable *table, unsigned long key)
{
	HashNode *node;

	if (!table)
		return NULL;

	node = *hash_table_lookup_node (table, key);

	return node ? node->value : NULL;
}

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

static unsigned int closest_prime (unsigned int num)
{
	unsigned int i;

	for (i = 0; i < nprimes; i++)
		if (primes[i] > num)
			return primes[i];

	return primes[i - 1];
}

static void hash_table_resize (HashTable *table)
{
	HashNode **new_nodes;
	HashNode *node;
	HashNode *next;
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

	new_nodes = calloc (sizeof (HashNode *), new_size);

	for (i = 0; i < table->size; i++)
	{
		for (node = table->nodes[i]; node; node = next)
		{
			next = node->next;

			hash_val = table->hash_func (node->key) % new_size;

			node->next = new_nodes[hash_val];
			new_nodes[hash_val] = node;
		}
	}

	free (table->nodes);
	table->nodes = new_nodes;
	table->size  = new_size;
}

static HashNode *hash_node_new (unsigned long key, void *value)
{
	HashNode *node;

	node = malloc (sizeof (HashNode));

	node->key   = key;
	node->value = value;
	node->next  = NULL;

	return node;
}

unsigned long hash_string (char *str)
{
	unsigned long hash;

	if (!str)
		return 0;

	hash = *str++;

	if (hash == 0)
		return 0;

	while (*str)
		hash = (hash << 5) - hash + *str++;

	return hash;
}

unsigned long hash_direct (unsigned long key)
{
	/* TODO: Put good integer hash function here */
	return key;
}

void hash_table_insert (HashTable *table, unsigned long key, void *value)
{
	HashNode **node;

	if (!table)
		return;

	node = hash_table_lookup_node (table, key);

	if (*node)
		(*node)->value = value;
	else
	{
		/* add it */
		*node = hash_node_new (key, value);
		table->items++;
		if (!table->frozen)
			hash_table_resize (table);
	}
}

void hash_table_remove (HashTable *table, unsigned long key)
{
	HashNode **node, *dest;

	if (!table)
		return;

	node = hash_table_lookup_node (table, key);

	if (!*node)
		return;

	dest = *node;
	(*node) = dest->next;
	hash_node_destroy (dest, table);
	table->items--;

	if (!table->frozen)
		hash_table_resize (table);
}

/*****************************************************************************/

void hash_table_foreach (HashTable *table, HashFunc func, void *udata)
{
	HashNode *node;
	unsigned int i;

	if (!table || !func)
		return;

	/* TODO - FALSE from this HashFunc should stop iteration */
	for (i = 0; i < table->size; i++)
		for (node = table->nodes[i]; node; node = node->next)
			(*func) (node->key, node->value, udata);
}

void *hash_table_find (HashTable *table, HashFunc func, void *udata)
{
	HashNode *node;
	unsigned int i;
	int ret;

	if (!table || !func)
		return NULL;

	for (i = 0; i < table->size; i++)
	{
		for (node = table->nodes[i]; node; node = node->next)
		{
			ret = (*func) (node->key, node->value, udata);

			if (ret)
				return node->value;
		}
	}

	return NULL;
}

unsigned long hash_table_find_key (HashTable *table, HashFunc func, void *udata)
{
	HashNode *node;
	unsigned int i;
	int ret;

	if (!table || !func)
		return 0;

	for (i = 0; i < table->size; i++)
	{
		for (node = table->nodes[i]; node; node = node->next)
		{
			ret = (*func) (node->key, node->value, udata);

			if (ret)
				return node->key;
		}
	}

	return 0;
}

int hash_table_foreach_remove (HashTable *table, HashFunc func, void *udata)
{
	HashNode *node, *prev;
	unsigned int i;
	int deleted = 0;

	if (!table || !func)
		return 0;

	for (i = 0; i < table->size; i++)
	{
restart:

		prev = NULL;

		for (node = table->nodes[i]; node; prev = node, node = node->next)
		{
			if ((*func) (node->key, node->value, udata))
			{
				deleted++;
				table->items--;

				if (prev)
				{
					prev->next = node->next;
					hash_node_destroy (node, table);
					node = prev;
				}
				else
				{
					table->nodes[i] = node->next;
					hash_node_destroy (node, table);
					goto restart;
				}
			}
		}
	}

	if (!table->frozen)
		hash_table_resize (table);

	return deleted;
}
