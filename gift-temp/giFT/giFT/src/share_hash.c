/*
 * $Id: share_hash.c,v 1.14 2003/05/05 11:55:26 jasta Exp $
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

#include "gift.h"

#include "file.h"
#include "share_file.h"

#include "share_hash.h"

/*****************************************************************************/

/*
 * Please note that a similar data structure is also maintained on the
 * protocol itself.  This is here only to avoid complication of colliding
 * algorithms.
 */
static Dataset *algos = NULL;

/*****************************************************************************/

static HashAlgo *algo_new (const char *type, int opt,
                           HashFn algofn, HashDspFn dspfn)
{
	HashAlgo *algo;

	if (!(algo = MALLOC (sizeof (HashAlgo))))
		return NULL;

	assert (type != NULL);
	assert (algofn != NULL);

	algo->ref    = 0;
	algo->opt    = opt;
	algo->type   = type;
	algo->algofn = algofn;
	algo->dspfn  = dspfn;

	if (algo->opt & HASH_PRIMARY)
		algo->opt |= HASH_LOCAL;

	return algo;
}

static void algo_free (HashAlgo *algo)
{
	assert (algo->ref <= 0);
	free (algo);
}

/*****************************************************************************/

Hash *hash_new (HashAlgo *algo, unsigned char *data, size_t len)
{
	Hash *hash;

	if (!(hash = MALLOC (sizeof (Hash))))
		return NULL;

	assert (algo != NULL);

	hash->algo = algo;
	hash->data = data;
	hash->len  = len;

	return hash;
}

void hash_free (Hash *hash)
{
	if (!hash)
		return;

	free (hash->data);
	free (hash);
}

Hash *hash_dup (Hash *hash)
{
	Hash *dup;

	if (!hash)
		return NULL;

	if (!(dup = MALLOC (sizeof (Hash))))
		return NULL;

	dup->algo = hash->algo;
	dup->data = gift_memdup (hash->data, hash->len);
	dup->len  = hash->len;

	return dup;
}

Hash *hash_calc (HashAlgo *algo, const char *path)
{
	Hash          *hash = NULL;
	unsigned char *data;
	size_t         len = 0;

	if (!path)
		return NULL;

	assert (algo != NULL);

	if ((data = algo->algofn (path, &len)))
		hash = hash_new (algo, data, len);

	return hash;
}

char *hash_dsp (Hash *hash)
{
	HashAlgo *algo;
	char     *dsp;
	String   *dsphack;

	if (!hash || !(algo = hash->algo))
		return NULL;

	assert (algo->dspfn != NULL);      /* TODO */

	if (!(dsp = algo->dspfn (hash->data)))
		return NULL;

	/* display strings are special serialized representations of the data
	 * and the algorithm name */
	if ((dsphack = string_new (NULL, 0, 0, TRUE)))
		string_appendf (dsphack, "%s:%s", algo->type, dsp);

	free (dsp);

	return string_free_keep (dsphack);
}

/*****************************************************************************/

HashAlgo *hash_algo_lookup (const char *type)
{
	return dataset_lookupstr (algos, (char *)type);
}

unsigned int hash_algo_register (Protocol *p, const char *type, int opt,
                                 HashFn algofn, HashDspFn dspfn)
{
	HashAlgo *algo;

	if (!p || !type || !algofn)
		return 0;

	if (!(algo = hash_algo_lookup (type)))
	{
		if (!(algo = algo_new (type, opt, algofn, dspfn)))
			return 0;

		/* add the newly allocated algorithm to the global list, then fall
		 * through and assert that it is in place in the protocol structure
		 * as well */
		dataset_insert (&algos, (char *)type, STRLEN_0(type), algo, 0);
	}

	/* use a reference counting system to know when it's safe to unload */
	algo->ref++;

#if 0
	/* this is a safe operation if this algo was already registered */
	dataset_insert (&p->hashes, type, STRLEN_0(type), algo, 0);
#endif

	return algo->ref;
}

unsigned int hash_algo_unregister (Protocol *p, const char *type)
{
	HashAlgo *algo;

	algo = hash_algo_lookup (type);
	assert (algo != NULL);

#if 0
	dataset_removestr (p->hashes, type);
#endif

	/* this algorithm still has references out there, keep it around */
	if (--algo->ref > 0)
		return algo->ref;

	/* proceed with its destruction */
	dataset_removestr (algos, (char *)type);
	algo_free (algo);

	return 0;
}

/*****************************************************************************/

/* not really the first in insertion order, but the first in the
 * dataset's internal order */
static int get_first (Dataset *d, DatasetNode *node, void *udata)
{
	Hash     *hash = node->value;
	HashAlgo *algo = hash->algo;

	if (!(algo->opt & HASH_LOCAL))
		return FALSE;

	return TRUE;
}

Hash *share_hash_get (FileShare *file, const char *type)
{
	Hash *hash;

	if (!file)
		return NULL;

	if (type)
		hash = dataset_lookupstr (SHARE_DATA(file)->hash.hashes, (char *)type);
	else
	{
		/* extract any of the hashes */
		hash = dataset_find (SHARE_DATA(file)->hash.hashes,
		                     DATASET_FOREACH(get_first), NULL);
	}

	return hash;
}

static int set_hash (FileShare *file, const char *type, Hash *hash)
{
	dataset_insert (&SHARE_DATA(file)->hash.hashes,
	                (char *)type, STRLEN_0(type), hash, 0);

	return TRUE;
}

int share_hash_set (FileShare *file, const char *type,
                    unsigned char *data, size_t len)
{
	Hash     *hash;
	HashAlgo *algo;

	if (!file || !type || !data || len == 0)
		return FALSE;

	assert (share_hash_get (file, type) == NULL);

	if (!(algo = hash_algo_lookup (type)))
		return FALSE;

	if (!(hash = hash_new (algo, data, len)))
		return FALSE;

	return set_hash (file, type, hash);
}

static int sh_clear (Dataset *d, DatasetNode *node, FileShare *file)
{
	hash_free (node->value);
	return TRUE;
}

void share_hash_clear (FileShare *file)
{
	if (!file || !SHARE_DATA(file))
		return;

	dataset_foreach (SHARE_DATA(file)->hash.hashes,
	                 DATASET_FOREACH(sh_clear), file);
	dataset_clear (SHARE_DATA(file)->hash.hashes);

	memset (&SHARE_DATA(file)->hash, 0, sizeof (SHARE_DATA(file)->hash));
}

void share_hash_foreach (FileShare *file, DatasetForeach func, void *udata)
{
	if (!file || !func)
		return;

	dataset_foreach (SHARE_DATA(file)->hash.hashes, func, udata);
}

static Hash *algo_run_calc (HashAlgo *algo, char *path)
{
	Hash *hash;

	hash = hash_calc (algo, path);
	free (path);

	return hash;
}

static int algo_run (Dataset *d, DatasetNode *node, Array **args)
{
	Hash         *hash;
	HashAlgo     *algo = node->value;
	FileShare    *file;
	unsigned int *i;

	list (args, &file, &i, NULL);

	/* not supposed to use this algorithm for local hashing, so skip it */
	if (!(algo->opt & HASH_LOCAL))
		return FALSE;

	/* hash already present, assume that it is up to date (the mtime
	 * checking code exists elsewhere in the code) */
	if (share_hash_get (file, algo->type))
		return FALSE;

	/* dirty little trick to make the allocation copy as a local arg to this
	 * function so its easier to work with */
	if ((hash = algo_run_calc (algo, file_host_path (SHARE_DATA(file)->path))))
	{
		set_hash (file, algo->type, hash);
		(*i)++;
	}

	return FALSE;
}

unsigned int share_hash_run (FileShare *file)
{
	Array       *args = NULL;
	unsigned int i = 0;

	if (!file)
		return 0;

	push (&args, file);
	push (&args, &i);

	dataset_foreach (algos, DATASET_FOREACH(algo_run), &args);

	unset (&args);

	return i;
}

char *share_hash_dsp (FileShare *file, const char *type)
{
	Hash *hash;

	if (!(hash = share_hash_get (file, type)))
		return NULL;

	return hash_dsp (hash);
}

/*****************************************************************************/

char *hashstr_algo (const char *hashstr)
{
	char       *colon;
	size_t      len;
	static char algostr[32];

	if (!hashstr || !(colon = strchr (hashstr, ':')))
		return NULL;

	/* determine the length of the text leading up to the ':', so that we
	 * can copy into our internal buffer */
	len = CLAMP ((colon - hashstr), 0, (sizeof (algostr) - 1));

	/* strncpy is evil */
	memcpy (algostr, hashstr, len);
	algostr[len] = 0;

	return algostr;
}

char *hashstr_data (const char *hashstr)
{
	char *data;

	if (!hashstr || !(data = strchr (hashstr, ':')))
		return (char *)hashstr;

	return ((char *)(hashstr + 1));
}
