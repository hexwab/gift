/*
 * $Id: ares_dl_encrypt.c,v 1.1 2006/02/27 14:39:39 mkern Exp $
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

/* Compile with msvc: 
 *   cl ares_dl_encrypt.c
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

int main (int argc, char* argv[])
{
	as_uint8 c;
	as_uint16 key, key1, key2, mul, add;
	int i, v;
	FILE *src, *dst;

	if (argc != 3 && argc != 4 && argc != 5)
	{
		fprintf (stderr, "Usage: %s <level> <src file> <dst file> [key]\n", argv[0]);
		fprintf (stderr, "    reads from src file, writes to dst file\n");
		fprintf (stderr, "    level is one of:\n");
		fprintf (stderr, "      1 - download request, outer wrap\n");
		fprintf (stderr, "      2 - download request, inner wrap\n");
		fprintf (stderr, "      3 - download reply (requires key)\n");
		fprintf (stderr, "      4 - download request TLV 0x0A encrypt\n");
		fprintf (stderr, "      5 - get_lowered_token (only source file is used)\n");
		exit (1);
	}

	if (!strcmp (argv[1], "1"))
	{
		if (!(src = fopen (argv[2], "rb")))
			FATAL_ERROR ("Failed to open src file");

		if (!(dst = fopen (argv[3], "wb")))
			FATAL_ERROR ("Failed to open dst file");

		key = 0x5D1C;
		mul = 0x5CA0;
		add = 0x15EC;

		while ((v = fgetc (src)) != EOF)
		{
			c = ((as_uint8)v) ^ (key >> 8);
			key = (key + ((as_uint8)c)) * mul + add;
			fputc (c, dst);
		}

		fclose (src);
		fclose (dst);
	}
	else if (!strcmp (argv[1], "2"))
	{
		if (!(src = fopen (argv[2], "rb")))
			FATAL_ERROR ("Failed to open src file");

		if (!(dst = fopen (argv[3], "wb")))
			FATAL_ERROR ("Failed to open dst file");

		key = 0x3FAA;
		mul = 0xD7FB;
		add = 0x3EFD;

		while ((v = fgetc (src)) != EOF)
		{
			c = ((as_uint8)v) ^ (key >> 8);
			key = (key + ((as_uint8)c)) * mul + add;
			fputc (c, dst);
		}

		fclose (src);
		fclose (dst);
	}
	else if (!strcmp (argv[1], "3"))
	{
		if (argc != 5)
			FATAL_ERROR ("Need decryption key from request");

		if (!(src = fopen (argv[2], "rb")))
			FATAL_ERROR ("Failed to open src file");

		if (!(dst = fopen (argv[3], "wb")))
			FATAL_ERROR ("Failed to open dst file");

		if (sscanf (argv[4], "0x%x", &i) == 0)
			sscanf (argv[4], "%u", &i);

		key = i;
		mul = 0xCB6F;
		add = 0x41BA;

		while ((v = fgetc (src)) != EOF)
		{
			c = ((as_uint8)v) ^ (key >> 8);
			key = (key + ((as_uint8)c)) * mul + add;
			fputc (c, dst);
		}

		fclose (src);
		fclose (dst);
	}
	else if (!strcmp (argv[1], "4"))
	{
		if (!(src = fopen (argv[2], "rb")))
			FATAL_ERROR ("Failed to open src file");

		if (!(dst = fopen (argv[3], "wb")))
			FATAL_ERROR ("Failed to open dst file");

		key = 0x5F40;
		key1 = 0x15D9;
		key2 = 0xB334;
		i = 0;

		while ((v = fgetc (src)) != EOF)
		{
			c = v;

			if (i >= 9)
			{
				c = ((as_uint8)c) ^ (key2 >> 8);
				key2 = (key2 + ((as_uint8)c)) * 0xCE6D + 0x58BF;
			}

			c = ((as_uint8)c) ^ (key1 >> 8);
			key1 = (key1 + ((as_uint8)c)) * 0x5AB3 + 0x8D1E;

			c = ((as_uint8)c) ^ (key >> 8);
			key = (key + ((as_uint8)c)) * 0x310F + 0x3A4E;

			fputc (c, dst);
			i++;
		}

		fclose (src);
		fclose (dst);
	}
	else if (!strcmp (argv[1], "5"))
	{
		as_uint32 acc = 0;
		as_uint8 c;
		int b = 0;

		if (!(src = fopen (argv[2], "rb")))
			FATAL_ERROR ("Failed to open src file");

		while ((v = fgetc (src)) != EOF)
		{
			c = tolower(v);
			acc ^= c << (b * 8);
			b = (b + 1) & 3;
		}

		acc = (acc * 0x4f1bbcdc) >> 16;

		printf ("token: 0x%04X\n", acc);

		fclose (src);
	}
	else
	{
		FATAL_ERROR ("Invalid level");
	}
}

/*****************************************************************************/
