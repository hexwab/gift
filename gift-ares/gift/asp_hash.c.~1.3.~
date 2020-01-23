/*
 * $Id: asp_hash.c,v 1.3 2004/12/12 16:19:32 hex Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
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

/* base32 alphabet */
static const char *ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static uint8_t base32_bits[256] = { 0 };

/* convert 5 bytes to 8 characters in base 32 */
static void bin_to_base32 (const uint8_t *in, char *out)
{
	out[0] = ALPHA[(in[0]       ) >> 3];
	out[1] = ALPHA[(in[0] & 0x07) << 2 | (in[1] & 0xc0) >> 6];
	out[2] = ALPHA[(in[1] & 0x3e) >> 1];
	out[3] = ALPHA[(in[1] & 0x01) << 4 | (in[2] & 0xf0) >> 4];
	out[4] = ALPHA[(in[2] & 0x0f) << 1 | (in[3] & 0x80) >> 7];
	out[5] = ALPHA[(in[3] & 0x7c) >> 2];
	out[6] = ALPHA[(in[3] & 0x03) << 3 | (in[4] & 0xe0) >> 5];
	out[7] = ALPHA[(in[4] & 0x1f)];
}

/* convert 8 characters in base 32 to 5 bytes */
static void base32_to_bin (const char *base32, uint8_t *out)
{
	const unsigned char *in   = base32;
	const uint8_t       *bits = base32_bits;

	out[0] = ((bits[in[0]]       ) << 3) | (bits[in[1]] & 0x1C) >> 2;
	out[1] = ((bits[in[1]] & 0x03) << 6) | (bits[in[2]]       ) << 1
	                                     | (bits[in[3]] & 0x10) >> 4;
	out[2] = ((bits[in[3]] & 0x0F) << 4) | (bits[in[4]] & 0x1E) >> 1;
	out[3] = ((bits[in[4]] & 0x01) << 7) | (bits[in[5]]       ) << 2
	                                     | (bits[in[6]] & 0x18) >> 3;
	out[4] = ((bits[in[6]] & 0x07) << 5) | (bits[in[7]]);
}

static void init_base32_bits (void)
{
	int   i;
	char *pos;

	/* set the each char's corresponding bit value in a lookup table */
	for (i = 0; i < sizeof(base32_bits); i++)
	{
		if ((pos = strchr (ALPHA, toupper (i))))
			base32_bits[i] = pos - ALPHA;
	}
}

/*****************************************************************************/

BOOL asp_base32_valid (const char *base32, size_t len)
{
	while (len > 0)
	{
		unsigned char c = toupper (*base32);

		if (!((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7')))
			break;

		base32++;
		len--;
	}

	return (len > 0 ? FALSE : TRUE);
}

unsigned char *asp_base32_encode (const uint8_t *in, size_t len)
{
	unsigned char *out, *ptr;

	assert ((len % 5) == 0);

	out = malloc (len * 8 / 5 + 1);

	if (!out)
		return NULL;

	for (ptr = out; len; in += 5, ptr += 8, len -= 5)
		bin_to_base32 (in, ptr);

	*ptr = '\0';

	return out;
}

unsigned char *asp_base32_decode (const char *in, size_t in_len,
                       size_t *out_len)
{
	unsigned char *out, *ptr;
	size_t len;

	/* initialize lookup table */
	if (base32_bits['b'] == 0)
		init_base32_bits ();

	if (in_len & 7 || !asp_base32_valid (in, in_len))
		return NULL;

	len = in_len * 5 / 8;

	if (!(out = malloc (len)))
		return NULL;

	if (out_len)
		*out_len = len;

	for (ptr = out; in_len; in += 8, ptr += 5, in_len -= 8)
		base32_to_bin (in, ptr);

	return out;
}

/*****************************************************************************/

/* Called by giFT to hash a file for this network. */
unsigned char *asp_giftcb_hash (const char *path, size_t *len)
{
	ASHash *hash;
	unsigned char *data;

	if (!(hash = as_hash_file (path)))
	{
		AS_ERR_1 ("Failed to hash file '%s'.", path);
		return NULL;
	}

	/* Make copy of hash data for giFT. */
	if (!(data = malloc (AS_HASH_SIZE)))
	{
		as_hash_free (hash);
		return NULL;
	}

	memcpy (data, hash->data, AS_HASH_SIZE);
	as_hash_free (hash);

	if (len)
		*len = AS_HASH_SIZE;

	/* giFT will free this. */
	return data;
}

/* Called by giFT to encode a hash in human readable form. */
unsigned char *asp_giftcb_hash_encode (unsigned char *data)
{
#if 0 /* base64 */
	ASHash *hash;
	unsigned char *encoded;

	if (!(hash = as_hash_create (data, AS_HASH_SIZE)))
		return NULL;

	encoded = as_hash_encode (hash);
	as_hash_free (hash);
#else /* base32 */
	unsigned char *encoded = asp_base32_encode (data, AS_HASH_SIZE);
#endif

	return encoded;
}

ASHash *asp_hash_decode (const char *encoded)
{
	ASHash *hash;
	unsigned char *bin;
	size_t len;

	if ((hash = as_hash_decode (encoded)))
		return hash;

	if ((bin = asp_base32_decode (encoded, strlen (encoded), &len)))
	{
		if (len != AS_HASH_SIZE)
		{
			free (bin);
			return NULL;
		}
		
		return (ASHash *)bin;
	}
}

/*****************************************************************************/
