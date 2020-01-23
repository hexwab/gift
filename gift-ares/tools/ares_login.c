/*
 * $Id: ares_login.c,v 1.2 2005/01/07 19:59:57 mkern Exp $
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

#include <stdio.h>
#include <stdlib.h>

typedef unsigned int as_uint32;
typedef unsigned short as_uint16;
typedef unsigned char as_uint8;

#ifdef _MSC_VER
# define strcasecmp(s1,s2) _stricmp(s1, s2)
#endif

#define FATAL_ERROR(x) { fprintf (stderr, "\nFATAL: %s\n", x); exit (1); }

/*****************************************************************************/

void print_bin_data(unsigned char * data, int len)
{
        int i;
        int i2;
        int i2_end;

//      printf("data len %d\n", data_len);

        for (i2 = 0; i2 < len; i2 = i2 + 16)
        {
                i2_end = (i2 + 16 > len) ? len: i2 + 16;
                for (i = i2; i < i2_end; i++)
                        if (isprint(data[i]))
                                fprintf(stderr, "%c", data[i]);
                        else
                        fprintf(stderr, ".");
                for ( i = i2_end ; i < i2 + 16; i++)
                        fprintf(stderr, " ");
                fprintf(stderr, " | ");
                for (i = i2; i < i2_end; i++)
                        fprintf(stderr, "%02x ", data[i]);
                fprintf(stderr, "\n");
        }
}

/*****************************************************************************/

static const char hex_string[] = "0123456789ABCDEFabcdef";

/* caller frees returned string */
char *as_hex_encode (const unsigned char *data, int src_len)
{
	char *out, *dst;
	int i;

	if (!data)
		return NULL;

	if (! (out = dst = malloc (src_len * 2 + 1)))
		return NULL;

	for(i=0; i<src_len; i++, dst += 2)
	{
		dst[0] = hex_string[data[i] >> 4];
		dst[1] = hex_string[data[i] & 0x0F];
	}

	dst[0] = 0;

	return out;
}

/* caller frees returned string */
unsigned char *as_hex_decode (const char *data, int *dst_len)
{
	char *dst, *h;
	int i, j;

	if (!data)
		return NULL;

	if (! (dst = malloc (strlen (data) / 2 + 1)))
		return NULL;

	for(i=0; *data && data[1]; i++, data += 2)
	{
		unsigned char byte = 0;

		for (j=0; j<2; j++)
		{
			if ((h = strchr (hex_string, data[j])) == NULL)
			{
				free (dst);
				return NULL;
			}

			byte <<= 4;
			byte |= (h - hex_string > 0x0F) ? (h - hex_string - 6) : h - hex_string;
		}

		dst[i] = byte;
	}

	if (dst_len)
		*dst_len = i;

	return dst;
}

/*****************************************************************************/

static void munge (as_uint8 *data, int len, as_uint16 key,
                   as_uint16 mul, as_uint16 add)
{
	int i;

	for (i = 0; i < len; i++)
	{
		data[i] = data[i] ^ (key >> 8);
		key = (key + data[i]) * mul + add;
	}
}

static void unmunge (as_uint8 *data, int len, as_uint16 key,
                     as_uint16 mul, as_uint16 add)
{
	as_uint8 c;
	int i;

	for (i = 0; i < len; i++)
	{
		c = data[i] ^ (key >> 8);
		key = (key + data[i]) * mul + add;
		data[i] = c;
	}
}

/*****************************************************************************/

int main (int argc, char* argv[])
{
	unsigned char *data, *hex_data;
	int len;
	unsigned int seed_16 = 0, seed_8 = 0;


	if (argc != 5)
	{
		fprintf (stderr, "Usage: %s <cmd> <seed_16> <seed_8> <data>\n", argv[0]);
		fprintf (stderr, "Where cmd is one of:\n"
		                 " - 'decode' to decode hex login string\n"
		                 " - 'encode' to encode plain login string\n");
		fprintf (stderr, "Plain login string is for example \"Ares 1.8.1.2951\"\n");
		exit (1);
	}

	if (!strcasecmp (argv[1], "decode"))
	{
		if (!(data = as_hex_decode (argv[4], &len)))
			FATAL_ERROR ("hex decode failed");

		if (sscanf (argv[2], "0x%x", &seed_16) == 0)
			sscanf (argv[2], "%u", &seed_16);
		if (sscanf (argv[3], "0x%x", &seed_8) == 0)
			sscanf (argv[3], "%u", &seed_8);

		fprintf (stderr, "seed_16: 0x%02x, seed_8: 0x%01x\n", seed_16, seed_8);
		fprintf (stderr, "\ndecoded data:\n");
		print_bin_data (data, len);

		/* the interesting part */
		unmunge (data, len, (seed_16 - seed_8) + 0x0B, 0x310F, 0x3A4E);
		unmunge (data, len, seed_16 + 0x0C, 0xCE6D, 0x58BF);

		fprintf (stderr, "\ndecrypted data:\n");
		print_bin_data (data, len);

		free (data);
	}
	else if (!strcasecmp (argv[1], "encode"))
	{
		data = strdup (argv[4]);
		len = strlen (data);

		if (sscanf (argv[2], "0x%x", &seed_16) == 0)
			sscanf (argv[2], "%u", &seed_16);
		if (sscanf (argv[3], "0x%x", &seed_8) == 0)
			sscanf (argv[3], "%u", &seed_8);

		fprintf (stderr, "seed_16: 0x%02x, seed_8: 0x%01x\n", seed_16, seed_8);

		/* the interesting part */
		munge (data, len, seed_16 + 0x0C, 0xCE6D, 0x58BF);
		munge (data, len, (seed_16 - seed_8) + 0x0B, 0x310F, 0x3A4E);

		fprintf (stderr, "\nencrypted data:\n");
		print_bin_data (data, len);

		if (!(hex_data = as_hex_encode (data, len)))
			FATAL_ERROR ("hex encode failed");

		fprintf (stderr, "\nencoded data:\n");
		print_bin_data (hex_data, strlen (hex_data));

		free (data);
		free (hex_data);
	}
	else
	{
		FATAL_ERROR ("Invalid command");
	}
}

/*****************************************************************************/
