/*
 * $Id: ft_bloom.h,v 1.6 2004/09/04 21:22:51 hexwab Exp $
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
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

#ifndef __FT_BLOOM_H
#define __FT_BLOOM_H

/*****************************************************************************/

/**
 * @file ft_bloom.h
 *
 * @brief Simple Bloom filter implementation.
 */

/*****************************************************************************/

struct _bloom
{
	uint8_t  *table;
	uint8_t  *count;
	int       bits;
	int       mask;
	int       nhash;                   /* number of hash functions */
	int       keylen;
};

typedef struct _bloom BloomFilter;

/*****************************************************************************/

/**
 * Allocate a new Bloom filter.
 *
 * @param bits     log2 of the filter size.
 * @param nhash    number of hash functions.
 * @param keylen   key length in bits (all keys must be this long)
 * @param count    whether refcounting should be done
 */
BloomFilter *ft_bloom_new (int bits, int nhash, int keylen, BOOL count);

/**
 * Deallocate a Bloom filter.
 */
void ft_bloom_free (BloomFilter *bf);

/**
 * Adds a key. Assumed to be at least keylen bits long (where keylen
 * was specified at creation time). Keys are expected to be evenly
 * distributed, otherwise efficiency will suffer.
 */
void ft_bloom_add (BloomFilter *bf, void *key);

/**
 * Adds an integer key. Similar to passing the address of the key to
 * ft_bloom_add, except that it gives the same results portably.
 */
void ft_bloom_add_int (BloomFilter *bf, int key);

/**
 * Checks if a given key might be a member of the set.
 *
 * @retval FALSE if not a member, TRUE if possibly a member (false
 * positives are permitted)
 */
BOOL ft_bloom_lookup (BloomFilter *bf, void *key);

/**
 * Analogous to ::ft_bloom_add_int.
 */
BOOL ft_bloom_lookup_int (BloomFilter *bf, int key);

/**
 * Removes a key. Only permitted if refcounting was enabled at
 * creation time.
 *
 * @retval TRUE on success.
 */
BOOL ft_bloom_remove (BloomFilter *bf, void *key);

BOOL ft_bloom_remove_int (BloomFilter *bf, int key);

/**
 * Removes all keys.
 */
void ft_bloom_clear (BloomFilter *bf);

/**
 * Clones a Bloom filter. Refcounting is disabled on the clone.
 */
BloomFilter *ft_bloom_clone (BloomFilter *bf);

/**
 * Compares two Bloom filters. The data in old is replaced with the
 * bits that differ between new and old.
 *
 * @retval TRUE on success.
 */
BOOL ft_bloom_diff (BloomFilter *new, BloomFilter *old);

/**
 * Merges two Bloom filters. For every bit set in new, the appropriate
 * bit is set in old. Reference counts are only updated by one
 * regardless of the refcount state of new (this is a feature, not a
 * bug, and is useful for creating heirarchies).
 *
 * @retval TRUE on success.
 */
BOOL ft_bloom_merge (BloomFilter *new, BloomFilter *old);

/**
 * Reverses a merge. For every bit set in new, the appropriate bit is
 * unset in old. This requires refcounting to be enabled for old.
 *
 * @retval TRUE on success.
 */
BOOL ft_bloom_unmerge (BloomFilter *new, BloomFilter *old);

/**
 * Checks if a filter has no bits set.
 *
 * @retval TRUE if empty.
 */
BOOL ft_bloom_empty (BloomFilter *bf);

/**
 * Returns what proportion of bits are set in the filter. 
 *
 * @retval density between 0 and 1.
 */
double ft_bloom_density (BloomFilter *bf);

/*****************************************************************************/

#endif /* __FT_BLOOM_H */
