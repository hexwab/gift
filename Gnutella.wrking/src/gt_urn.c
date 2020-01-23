/*
 * $Id: gt_urn.c,v 1.4 2004/03/05 17:47:29 hipnod Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

#include "gt_gnutella.h"
#include "gt_urn.h"
#include "encoding/base32.h"
#include "sha1.h"

/*****************************************************************************/

#define URN_PREFIX_LEN        (sizeof("urn:")-1)
#define SHA1_PREFIX_LEN       (sizeof("sha1:")-1)
#define BITPRINT_PREFIX_LEN   (sizeof("bitprint:")-1)

/*****************************************************************************/

enum urn_types
{
	GT_URN_SHA1     = 0,
	GT_URN_BITPRINT = 1,   /* for now, collapse bitprint to sha1 */
};

/*****************************************************************************/

static long get_urn_type (gt_urn_t *urn)
{
	long tmp;

	memcpy (&tmp, urn, sizeof(tmp));
	return tmp;
}

static void set_urn_type (gt_urn_t *urn, enum urn_types t)
{
	long tmp = t;

	memcpy (urn, &tmp, sizeof(tmp));
}

static unsigned char *get_urn_data (const gt_urn_t *urn)
{
	return (unsigned char *)urn + sizeof(long);
}

static void set_urn_data (gt_urn_t *urn, const unsigned char *data, size_t len)
{
	memcpy (get_urn_data (urn), data, len);
}

/*****************************************************************************/

static size_t bin_length (enum urn_types t)
{
	switch (t)
	{
	 case GT_URN_BITPRINT:
	 case GT_URN_SHA1: return SHA1_BINSIZE;
	 default:          return 0;
	}
}

static gt_urn_t *sha1_urn_new (const unsigned char *data)
{
	gt_urn_t *new_urn;

	if (!(new_urn = malloc (SHA1_BINSIZE + sizeof(long))))
		return NULL;

	/* put the identifier at the beginning */
	set_urn_type (new_urn, GT_URN_SHA1);

	/* copy the data */
	set_urn_data (new_urn, data, SHA1_BINSIZE);

	return new_urn;
}

gt_urn_t *gt_urn_new (const char *urn_type, const unsigned char *data)
{
	if (!strcasecmp (urn_type, "urn:sha1"))
		return sha1_urn_new (data);

	return NULL;
}

unsigned char *gt_urn_data (const gt_urn_t *urn)
{
	if (!urn)
		return NULL;

	return get_urn_data (urn);
}

char *gt_urn_string (const gt_urn_t *urn)
{
	unsigned char  *data;
	char           *urn_str;
	const size_t    prefix_len = URN_PREFIX_LEN + SHA1_PREFIX_LEN;

	/*
	 * This is the same for bitprint and sha1 urns, because we convert
	 * to sha1 anyway.
	 */

	if (!(data = gt_urn_data (urn)))
		return NULL;

	if (!(urn_str = malloc (prefix_len + SHA1_STRLEN + 1)))
		return NULL;

	memcpy (urn_str, "urn:sha1:", prefix_len);
	gt_base32_encode (data, SHA1_BINSIZE, urn_str + prefix_len, SHA1_STRLEN);

	urn_str[prefix_len + SHA1_STRLEN] = 0;

	return urn_str;
}

/*****************************************************************************/

static gt_urn_t *sha1_urn_parse (const char *base32)
{
	gt_urn_t *bin;

	/* make sure the hash is the right length */
	if (!gt_base32_valid (base32, SHA1_STRLEN))
		return NULL;

	if (!(bin = malloc (SHA1_BINSIZE + sizeof(long))))
		return NULL;

	gt_base32_decode (base32, SHA1_STRLEN, bin + sizeof(long), SHA1_BINSIZE);
	set_urn_type (bin, GT_URN_SHA1);

	return bin;
}

/*
 * Bitprint urns are the format:
 *
 *      urn:bitprint:[32-character SHA1].[39-character TigerTree]
 *
 * We use the sha1 parsing and truncate the tigertree for now.
 */
static gt_urn_t *bitprint_urn_parse (const char *base32)
{
	return sha1_urn_parse (base32);
}

gt_urn_t *gt_urn_parse (const char *str)
{
	if (strncasecmp ("urn:", str, URN_PREFIX_LEN) != 0)
		return NULL;

	str += URN_PREFIX_LEN;

	if (!strncasecmp (str, "sha1:", SHA1_PREFIX_LEN))
		return sha1_urn_parse (str + SHA1_PREFIX_LEN);

	if (!strncasecmp (str, "bitprint:", BITPRINT_PREFIX_LEN))
		return bitprint_urn_parse (str + BITPRINT_PREFIX_LEN);

	return NULL;
}

/*****************************************************************************/

int gt_urn_cmp (gt_urn_t *a, gt_urn_t *b)
{
	int ret;

	if (!a || !b)
		return -1;

	if ((ret = memcmp (a, b, 4)))
		return ret;

	return memcmp (a + sizeof(long), b + sizeof(long),
	               bin_length (get_urn_type (a)));
}
