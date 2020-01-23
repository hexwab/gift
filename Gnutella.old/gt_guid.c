/*
 * $Id: gt_guid.c,v 1.9 2003/06/29 23:15:15 hipnod Exp $
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

#include "gt_gnutella.h"

/*****************************************************************************/

/* seed for generating unique numbers */
static unsigned long seed = 0;

/* map binary numbers to hexadecimal */
static char bin_to_hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
                             'a', 'b', 'c', 'd', 'e', 'f' };

/*****************************************************************************/

gt_guid_t *gt_guid_new (void)
{
	uint32_t  *buf;
	gt_guid_t *guid;
	int        i;

	/* TODO: a better RNG */
	if (!seed)
	{
		struct timeval tv;

		platform_gettimeofday (&tv, NULL);

		seed = tv.tv_usec ^ tv.tv_sec;

		srand (seed);
	}

	buf = malloc (GUID_LEN);

	for (i = 0; i < 4; i++)
		buf[i] = rand ();

	guid = (gt_guid_t *) buf;

	/* mark this GUID as coming from a "new" client */
	guid[8]  = 0xff;
	guid[15] = 0x00;

	return guid;
}

int gt_guid_cmp (gt_guid_t *a, gt_guid_t *b)
{
	return memcmp (a, b, GUID_LEN);
}

char *gt_guid_str (gt_guid_t *guid)
{
	static char    buf[128];
	unsigned char  c;
	int            pos;
	int            len;

	if (!guid)
		return NULL;

	pos = 0;
	len = GUID_LEN;

	while (len-- > 0)
	{
		c = *guid++;

		buf[pos++] = bin_to_hex[(c & 0xf0) >> 4];
		buf[pos++] = bin_to_hex[(c & 0x0f)];
	}

	buf[pos] = 0;

	return buf;
}

gt_guid_t *gt_guid_dup (gt_guid_t *guid)
{
	gt_guid_t *new_guid;

	if (!(new_guid = malloc (GUID_LEN)))
		return NULL;

	memcpy (new_guid, guid, GUID_LEN);

	return new_guid;
}

static unsigned char hex_char_to_bin (char x)
{
	if (x >= '0' && x <= '9')
		return (x - '0');

	x = toupper (x);

	return ((x - 'A') + 10);
}

static int hex_to_bin (char *hex, unsigned char *bin, int len)
{
	unsigned char value;

	while (isxdigit (hex[0]) && isxdigit (hex[1]) && len-- > 0)
	{
		value  = (hex_char_to_bin (*hex++) << 4) & 0xf0;
		value |= (hex_char_to_bin (*hex++)       & 0x0f);
		*bin++ = value;
	}

   return (len <= 0) ? TRUE : FALSE;
}

gt_guid_t *gt_guid_bin (char *guid_ascii)
{
	gt_guid_t *guid;

	if (!guid_ascii)
		return NULL;

	if (!(guid = malloc (GUID_LEN)))
		return NULL;

	if (!hex_to_bin (guid_ascii, guid, GUID_LEN))
	{
		free (guid);
		return NULL;
	}

	return guid;
}

BOOL gt_guid_is_empty (gt_guid_t *guid)
{
	return memcmp (guid, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0;
}
