/*
 * $Id: as_hash.c,v 1.8 2004/09/11 18:33:24 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* Buffer size used when hashing files. */
#define BLOCK_SIZE (8*1024)

/*****************************************************************************/

static as_bool hash_file (ASHash *hash, const char *file);

/*****************************************************************************/

/* create hash from raw data */
ASHash *as_hash_create (const as_uint8 *src, unsigned int src_len)
{
	ASHash *hash;

	if (!(hash = malloc (sizeof (ASHash))))
		return NULL;

	if (!src || src_len == 0)
	{
		memset (hash->data, 0, AS_HASH_SIZE);
	}
	else
	{
		if (src_len != AS_HASH_SIZE)
		{
			/* we currently only know about one hash (size) */
			assert (src_len == AS_HASH_SIZE);
			free (hash);
			return NULL;
		}

		memcpy (hash->data, src, src_len);
	}

	return hash;
}

/* create copy of hash */
ASHash *as_hash_copy (ASHash *hash)
{
	return as_hash_create (hash->data, AS_HASH_SIZE);
}

/* create hash from file */
ASHash *as_hash_file (const char *file)
{
	ASHash *hash;

	if (!(hash = as_hash_create (NULL, 0)))
		return NULL;

	if (!hash_file (hash, file))
	{
		as_hash_free (hash);
		return NULL;
	}

	return hash;
}

/* free hash */
void as_hash_free (ASHash *hash)
{
	if (!hash)
		return;

	free (hash);
}

/*****************************************************************************/

/* return TRUE if the hashes are equal */
as_bool as_hash_equal (ASHash *a, ASHash *b)
{
	if (!a || !b)
		return FALSE;

	return (memcmp (a->data, b->data, AS_HASH_SIZE) == 0);
}

/*****************************************************************************/

/* create hash from base64 encoded string */
ASHash *as_hash_decode (const char *encoded)
{
	ASHash *hash;
	unsigned char *bin;
	int len;

	if (!(bin = as_base64_decode (encoded, &len)))
		return NULL;

	if (len != AS_HASH_SIZE)
	{
		free (bin);
		return NULL;
	}

	hash = as_hash_create (bin, len);

	free (bin);

	return hash;
}

/* return base64 encoded string of hash. caller frees result. */
char *as_hash_encode (ASHash *hash)
{
	char *str;

	if (!(str = as_base64_encode (hash->data, AS_HASH_SIZE)))
		return NULL;

	return str;
}

/* Return static string of hash for _debugging_ purposes. Do not use for
 * anything critical because threading may corrupt buffer. */
char *as_hash_str (ASHash *hash)
{
	char *encoded;
	static char buf[AS_HASH_BASE64_SIZE + 16];
	
	/* This implementation is far from efficient */
	if (!(encoded = as_hash_encode (hash)))
		return NULL;

	gift_strncpy (buf, encoded, sizeof (buf) - 1);
	free (encoded);

	return buf;
}

/*****************************************************************************/

static as_bool hash_file (ASHash *hash, const char *file)
{
	FILE *fp;
	ASSHA1State sha1;
	struct stat st;
	as_uint8 buf[BLOCK_SIZE];
	size_t size, len;

	if (stat (file, &st) < 0)
		return FALSE;

	size = st.st_size;

	if (!(fp = fopen (file, "rb")))
		return FALSE;

	as_sha1_init (&sha1);

	while (size > BLOCK_SIZE)
	{
		len = fread (buf, 1, BLOCK_SIZE, fp);

		if (len != BLOCK_SIZE)
			break;

		as_sha1_update (&sha1, buf, len);
		size -= len;
	}

	/* hash final segment */
	if (size <= BLOCK_SIZE)
	{
		len = fread (buf, 1, size, fp);

		if (len == size)
		{
			as_sha1_update (&sha1, buf, len);
			size -= len;
		}
	}

	fclose (fp);

	if (size != 0)
		return FALSE;

	assert (sizeof (hash->data) == SHA_DIGESTSIZE);
	as_sha1_final (&sha1, hash->data);

	return TRUE;
}

/*****************************************************************************/
