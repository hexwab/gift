/*
 * $Id: gt_guid.c,v 1.5 2003/04/25 06:01:28 jasta Exp $
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

/* seed for generating unique numbers */
static unsigned long seed = 0;

gt_guid *guid_new ()
{
	uint32_t *buf;
	gt_guid  *guid;
	int       i;

	/* TODO: a better RNG */
	if (!seed)
	{
		struct timeval tv;

		platform_gettimeofday (&tv, NULL);

		seed = tv.tv_usec ^ tv.tv_sec;

		srand (seed);
	}

	buf = malloc (16);

	for (i = 0; i < 4; i++)
		buf[i] = rand ();

	guid = (gt_guid *) buf;

	/* mark this GUID as coming from a "new" client */
	guid[8]  = 0xff;
	guid[15] = 0x00;

	return guid;
}

int guid_cmp (gt_guid *a, gt_guid *b)
{
	return memcmp (a, b, 16);
}

char *guid_str (gt_guid *guid)
{
	static char buf[64];
	int i, len;

	if (!guid)
		return NULL;

	for (len = i = 0; i < 16 && len < sizeof (buf); i++)
		len += sprintf (buf + len, "%02x", guid[i]);

	buf[len] = '\0';

	return buf;
}

gt_guid *guid_dup (gt_guid *guid)
{
	gt_guid *new_guid;

	if (!(new_guid = malloc (16)))
		return NULL;

	memcpy (new_guid, guid, 16);

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

gt_guid *guid_bin (char *guid_ascii)
{
	gt_guid *guid;

	if (!guid_str)
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
