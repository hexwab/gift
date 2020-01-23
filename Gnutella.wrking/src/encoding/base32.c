/*
 * $Id: base32.c,v 1.2 2004/03/27 00:34:25 mkern Exp $
 *
 * Copyright (C) 2003-2004 giFT project (gift.sourceforge.net)
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
#include "encoding/base32.h"

/*****************************************************************************/

/* base32 alphabet */
static const char *ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static uint8_t base32_bits[256] = { 0 };

/*****************************************************************************/

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

BOOL gt_base32_valid (const char *base32, size_t len)
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

void gt_base32_encode (const uint8_t *in, size_t in_len,
                       char *out, size_t out_len)
{
	assert (in_len == 20);
	assert (out_len == 32);

	bin_to_base32 (in,      out);
	bin_to_base32 (in + 5,  out + 8);
	bin_to_base32 (in + 10, out + 16);
	bin_to_base32 (in + 15, out + 24);
}

void gt_base32_decode (const char *in, size_t in_len,
                       uint8_t *out, size_t out_len)
{
	/* initialize lookup table */
	if (base32_bits['b'] == 0)
		init_base32_bits ();

	assert (in_len == 32);
	assert (out_len == 20);

	base32_to_bin (in     , out     );
	base32_to_bin (in +  8, out +  5);
	base32_to_bin (in + 16, out + 10);
	base32_to_bin (in + 24, out + 15);
}

/*****************************************************************************/

#if 0
static void test_str (const char *test)
{
	unsigned char bin[20];
	char          hash[33];

	hash[32] = 0;

	gt_base32_decode (test, strlen (test), bin, sizeof(bin));
	gt_base32_encode (bin, sizeof(bin), hash, sizeof(hash) - 1);

	if (!gt_base32_valid (hash, 32))
		abort ();

	if (strcmp (hash, test) != 0)
	{
		printf ("\ntest=%s(%d)\nhash=%s(%d)\n", test,
		         strlen (test), hash, strlen (hash));
		fflush (stdout);
	}
}

int main (int argc, char **argv)
{
	int   i, j;
	char  str[33];
	int   len   = strlen (ALPHA);

	str[sizeof (str) - 1] = 0;

	test_str (ALPHA);

	for (i = 1; i <= 100000; i++)
	{
		for (j = 0; j < 32; j++)
			str[j] = ALPHA[rand () % len];

		if (!gt_base32_valid (str, 32))
			abort ();

		printf ("%i\r", i);
		fflush (stdout);
		test_str (str);
		usleep (1);
	}

	printf ("\n");
	return 0;
}
#endif
