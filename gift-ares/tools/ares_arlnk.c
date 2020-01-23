/*
 * $Id: ares_arlnk.c,v 1.2 2005/12/17 23:09:58 mkern Exp $
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
#include <zlib.h>

/* Compile with msvc: 
 *   cl /MTd /I..\zlib ares_arlnk.c ..\zlib\zlibd.lib
 */

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

/* caller frees returned string */
unsigned char *as_base64_decode (const char *data, int *dst_len)
{
	static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char *dst, *out;
	char *p;
	int i;
	unsigned char in[4];

	if(!data)
		return NULL;

	if((out = dst = malloc (strlen (data))) == NULL)
		return NULL;

	for (i=0, *dst_len=0; *data; data++)
	{
		if((p = strchr(base64, *data)) == NULL)
			continue;

		in[i++] = (unsigned char)(p - base64);

		if (i == 4)
		{
			dst[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
			dst[1] = ((in[1] & 0x0F) << 4) | ((in[2] & 0x3C) >> 2);
			dst[2] = ((in[2] & 0x03) << 6) | (in[3] & 0x3F);
			dst += 3;
			*dst_len += 3;
			i = 0;
		}
	}

	if (i >= 2)
	{
		dst[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
		(*dst_len)++;
	}

	if (i == 3)
	{
		dst[1] = ((in[1] & 0x0F) << 4) | ((in[2] & 0x3C) >> 2);
		(*dst_len)++;
	}

	return out;
}

/*****************************************************************************/

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

/* encrypt/decrypt arlnk */
void as_decrypt_arlnk (as_uint8 *data, int len)
{
	unmunge (data, len, 0x6F13, 0x5AB3, 0x8D1E);
}

/*****************************************************************************/

int main (int argc, char* argv[])
{
	char *encoded;
	unsigned char *data;
	int len;
	unsigned char uncompressed[4096];
	unsigned long size = sizeof(uncompressed);

	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s <arlnk>\n", argv[0]);
		exit (1);
	}

	if ((encoded = strstr (argv[1], "arlnk://")))
		encoded += 8;
	else
		encoded = argv[1];

	fprintf (stderr, "arlnk: %s\nlen: %d\n", encoded, strlen (encoded));

	/* decode */
	if (!(data = as_base64_decode (encoded, &len)))
		FATAL_ERROR ("base64 decode failed");

	fprintf (stderr, "\nbase64 decoded data (%d bytes):\n\n", len);
	print_bin_data (data, len);

	/* decrypt */
	as_decrypt_arlnk (data, len);
	fprintf (stderr, "\ndecrypted data (%d bytes):\n\n", len);
	print_bin_data (data, len);

	/* inflate */
	if ((uncompress (uncompressed, &size, data, len)) != Z_OK)
		FATAL_ERROR ("zlib inflate failed");

	free (data);
	data = uncompressed;
	len = (int) size;

	fprintf (stderr, "\ninflated data (%d bytes):\n\n", len);
	print_bin_data (data, len);

}

/*****************************************************************************/
