/*
 * share_hash.c
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

#include "share_file.h"
#include "share_hash.h"

#include "file.h"

/*****************************************************************************/

static Dataset *algorithms = NULL;

/*****************************************************************************/

ShareHash *share_hash_new (unsigned char *hash, int len)
{
	ShareHash *sh;

	if (!hash || len <= 0)
		return NULL;

	if (!(sh = malloc (sizeof (ShareHash))))
		return NULL;

	sh->hash = hash;
	sh->len  = len;

	return sh;
}

ShareHash *share_hash_dup (ShareHash *sh)
{
	ShareHash *dup;

	if (!sh || !(dup = share_hash_new (sh->hash, sh->len)))
		return NULL;

	dup->hash = malloc (sh->len);
	memcpy (dup->hash, sh->hash, sh->len);

	return dup;
}

void share_hash_free (ShareHash *sh)
{
	free (sh->hash);
	free (sh);
}

/*****************************************************************************/

int hash_algo_register (Protocol *p, char *type,
                        HashAlgorithm algo, HashHuman human)
{
	if (!p || !type || !algo || !human)
		return FALSE;

	if (dataset_lookup (algorithms, type, STRLEN_0 (type)))
	{
		TRACE (("%s: ignoring %s", p->name, type));
		return FALSE;
	}

	dataset_insert (&algorithms, type, STRLEN_0 (type), algo, 0);
	return TRUE;
}

/* TODO -- dont unregister the last hashing routine if a duplicate was seen
 * above ... need to implement a waiting list of some kind */
void hash_algo_unregister (Protocol *p, char *type)
{
	dataset_remove (algorithms, type, STRLEN_0 (type));
}

/*****************************************************************************/

static int algo_calc (Dataset *d, DatasetNode *node, FileShare *file)
{
	char          *type = node->key;
	HashAlgorithm  func = node->value;
	unsigned char *hash = NULL;
	int            len  = 0;
	char          *host_path;

	/* check if we already have a hash stored here */
	if (share_hash_get (file, type))
		return FALSE;

	if ((host_path = file_host_path (SHARE_DATA(file)->path)))
	{
		hash = func (host_path, type, &len);
		free (host_path);
	}

	share_hash_set (file, type, hash, len);
	return FALSE;
}

int hash_algo_run (FileShare *file)
{
	if (!file || file->p)
	{
		TRACE (("eep!  what are you doing!?"));
		return FALSE;
	}

	dataset_foreach (algorithms, DATASET_FOREACH (algo_calc), file);
	return TRUE;
}

/*****************************************************************************/

ShareHash *share_hash_get (FileShare *file, char *type)
{
	ShareHash *sh;

	if (!file)
		return NULL;

	if (file->p)
		sh = SHARE_DATA(file)->hash.hash;
	else
	{
		/* TODO -- select the _first_ registered hashing algorithm */
		if (!type)
			type = "MD5";

		sh = dataset_lookup (SHARE_DATA(file)->hash.hashes,
		                     type, STRLEN_0 (type));
	}

	return sh;
}

int share_hash_set (FileShare *file, char *type,
                    unsigned char *hash, int len)
{
	ShareHash *sh;

	if (!file || !type)
		return FALSE;

	assert (share_hash_get (file, type) == NULL);

	if (!(sh = share_hash_new (hash, len)))
		return FALSE;

	if (file->p)
		SHARE_DATA(file)->hash.hash = sh;
	else
	{
		dataset_insert (&SHARE_DATA(file)->hash.hashes,
		                type, STRLEN_0 (type), sh, 0);
	}

	return TRUE;
}

static int sh_clear (Dataset *d, DatasetNode *node, FileShare *file)
{
	ShareHash *sh = node->value;

	share_hash_free (sh);
	return TRUE;
}

void share_hash_clear (FileShare *file)
{
	if (!file || !SHARE_DATA(file))
		return;

	if (file->p)
		share_hash_free (SHARE_DATA(file)->hash.hash);
	else
	{
		dataset_foreach (SHARE_DATA(file)->hash.hashes,
						 DATASET_FOREACH (sh_clear), file);
		dataset_clear (SHARE_DATA(file)->hash.hashes);
	}

	memset (&SHARE_DATA(file)->hash, 0, sizeof (SHARE_DATA(file)->hash));
}
