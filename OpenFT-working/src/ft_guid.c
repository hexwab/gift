/*
 * $Id: gt_guid.c,v 1.2 2003/05/05 09:49:09 jasta Exp $
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

#include "ft_openft.h"

#include "ft_guid.h"

/*****************************************************************************/

static unsigned int seed = 0;

/*****************************************************************************/

ft_guid_t *ft_guid_new (void)
{
	ft_guid_t *buf;
	uint32_t  *buf32;
	int        i;

	if (!seed)
	{
		struct timeval tv;

		platform_gettimeofday (&tv, NULL);

		seed = tv.tv_usec ^ tv.tv_sec;
		srand (seed);
	}

	if (!(buf = malloc (FT_GUID_SIZE)))
		return NULL;

	buf32 = (uint32_t *)buf;

	for (i = 0; i < 4; i++)
		buf32[i] = rand ();

	return buf;
}

void ft_guid_free (ft_guid_t *guid)
{
	free (guid);
}

/*****************************************************************************/

ft_guid_t *ft_guid_dup (ft_guid_t *src)
{
	ft_guid_t *dst;

	if (!src)
		return NULL;

	if (!(dst = malloc (FT_GUID_SIZE)))
		return NULL;

	memcpy (dst, src, FT_GUID_SIZE);
	return dst;
}

/*****************************************************************************/

char *ft_guid_fmt (ft_guid_t *guid)
{
	static char buf[64];
	String     *s;
	int         i;

	if (!guid)
		return "(null)";

	if (!(s = string_new (buf, sizeof (buf), 0, FALSE)))
		return "(null)";

	for (i = 0; i < FT_GUID_SIZE; i++)
		string_appendf (s, "%02x", guid[i]);

	return string_free_keep (s);
}

/*****************************************************************************/

#if 0
int main (int argc, char **argv)
{
	Dataset   *guids;
	ft_guid_t *guid;
	int        i;

	guids = dataset_new (DATASET_HASH);
	assert (guids != NULL);

	for (i = 0 ;; i++)
	{
		guid = ft_guid_new ();
		assert (guid != NULL);

		if (dataset_lookup (guids, guid, FT_GUID_SIZE))
			break;

		dataset_insert (&guids, guid, FT_GUID_SIZE, "ft_guid_t", 0);

		if (i % 100000 == 0)
			printf ("i=%i...\n", i);
	}

	printf ("collision detected, i=%i\n", i);

	return 0;
}
#endif
