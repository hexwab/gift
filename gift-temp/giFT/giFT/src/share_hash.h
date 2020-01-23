/*
 * $Id: share_hash.h,v 1.7 2003/05/04 06:55:49 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifndef __SHARE_HASH_H
#define __SHARE_HASH_H

/*****************************************************************************/

/**
 * @file hashing.h
 *
 * @brief Manage hashing algorithms provided by protocol plugins and applied
 *        to locally shared files.
 */

/*****************************************************************************/

struct _file_share;                     /* share_file.h:FileShare */

/*****************************************************************************/

/**
 * Holds data received from the protocol plugin which describes how to
 * construct a hash from a given path.  Please note that the algofn and dspfn
 * members are of a type defined in plugin/protocol.h.  This is done so that
 * the protocol plugin can share the type definition.
 */
typedef struct
{
	unsigned char ref;                 /**< Referencing counting system */
	int           opt;                 /**< Options provided by the protocol */
	const char   *type;                /**< Hashing algorith name.  Such as,
	                                    *   MD5, SHA1, etc. */
	HashFn      algofn;
	HashDspFn   dspfn;
} HashAlgo;

/**
 * Data structure used to represent an individual hash from the protocol.
 */
typedef struct
{
	HashAlgo      *algo;               /**< Refers to the algorithm type
	                                    *   used to generate this hash */
	unsigned char *data;               /**< Actual hash data.  Must point to
	                                    *   dynamically allocated memory at
	                                    *   least `size' long */
	size_t         len;                /**< Length of data */
} Hash;

/*****************************************************************************/

/**
 * Raw interface to allocate a new hash object.  Please avoid using this
 * directly at all costs.
 *
 * @param algo
 * @param data  Pointer to the beginning of a hash at least `len' bytes
 *              long.  This data must be allocated, and will not be
 *              copied by this function call.
 * @param len   Length of `data'.
 *
 * @return Dynamically allocated hash object.
 */
Hash *hash_new (HashAlgo *algo, unsigned char *data, size_t len);

/**
 * Destroy the result from ::hash_new.  Please note that this will free the
 * original data argument.
 */
void hash_free (Hash *hash);

/**
 * Duplicate all entries in a hash object.  This includes the original data
 * argument.
 */
Hash *hash_dup (Hash *hash);

/**
 * Calculate a new hash from the supplied algorithm and file path.  This is
 * used as a work-around to the structured share_hash API, which requires a
 * constructed FileShare to operate.
 */
Hash *hash_calc (HashAlgo *algo, const char *path);

/**
 * Convert a calculated hash (see ::hash_calc) into a NUL-terminated ASCII
 * representation.
 *
 * @return Dynamically allocated string in the form TYPE:ASCII_DATA.
 */
char *hash_dsp (Hash *hash);

/*****************************************************************************/

/**
 * Register a new hashing algorithm.  See the documentation available at
 * Protocol::hash_handler.
 *
 * @return Number of references to the supplied hashing algorithm or zero on
 *         error.
 */
unsigned int hash_algo_register (Protocol *p, const char *type, int opt,
                                 HashFn algofn, HashDspFn dspfn);

/**
 * Unregister a hashing algorithm.  This is used when a protocol is unloaded
 * so that we can gracefully update the internal data structure without
 * interfering with currently loaded protocols.
 *
 * @return Number of references remaining.  If zero, the algorithm has been
 *         completely removed and it's memory free'd.
 */
unsigned int hash_algo_unregister (Protocol *p, const char *type);

/**
 * Lookup a currently registered algorithm by name.
 */
HashAlgo *hash_algo_lookup (const char *type);

/*****************************************************************************/

/**
 * Assign a pre-calculated hash.
 *
 * @param file
 * @param type
 * @param data  Actual hash.  This must be allocated memory and ownership will
 *              be absorbed into the file structure.
 * @param len   Length of `data'.
 *
 * @return Boolean success or failure.  It is considered a success if the
 *         previous hash was overwritten.
 */
int share_hash_set (struct _file_share *file, const char *type,
                    unsigned char *data, size_t len);

/**
 * Access the previously set hash.  See ::share_hash_set.
 */
Hash *share_hash_get (struct _file_share *file, const char *type);

/**
 * Clear all hashes associated with the given file.
 */
void share_hash_clear (struct _file_share *file);

/**
 * Iterate through all hashes currently associated with this file.
 * Internally uses ::dataset_foreach.
 */
void share_hash_foreach (struct _file_share *file, DatasetForeach func,
                         void *udata);

/**
 * Build a hash of the supplied file for all currently registered hashing
 * algorithms.  Keep in mind that only those primary or local algos will be
 * used here.  Secondary algorithms will be used mainly for data integrity.
 *
 * @return Number of hashes successfully created.
 */
unsigned int share_hash_run (struct _file_share *file);

/**
 * Return the ASCII representation of the hash described after lookup.  See
 * ::share_hash_get.
 *
 * @return Pointer to a dynamically allocated NUL-terminated string or NULL
 *         on error.
 */
char *share_hash_dsp (struct _file_share *file, const char *type);

/*****************************************************************************/

/**
 * Given a giFT-specific hash string return a static reference to ALGOTYPE.
 * The protocol has no awareness of this hack, and it is only used to
 * serialize hashes over the interface protocol.
 *
 * @param hashstr  Serialized hash string in the form ALGOTYPE:ASCIIDATA
 *
 * @return Pointer to a static NUL-terminated string representation of the
 *         algorithm name or NULL on error.
 */
char *hashstr_algo (const char *hashstr);

/**
 * Access the data portion of the hash string.  See ::hashstr_algo.
 *
 * @return Pointer into the supplied hashstr object.
 */
char *hashstr_data (const char *hashstr);

/*****************************************************************************/

#endif /* __SHARE_HASH_H */
