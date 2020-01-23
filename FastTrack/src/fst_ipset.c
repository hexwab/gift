/*
 * $Id: fst_ipset.c,v 1.4 2004/11/11 17:24:38 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_ipset.h"

/*****************************************************************************/

/* number of items allocated at once */
#define ALLOC_INCREMENT 32

/*****************************************************************************/

/* alloc and init set */
FSTIpSet *fst_ipset_create ()
{
	FSTIpSet *ipset;

	if (! (ipset = malloc (sizeof (FSTIpSet))))
		return NULL;

	ipset->allocated = ALLOC_INCREMENT;
	ipset->item_count = 0;
	ipset->items = malloc (sizeof (FSTIpSetItem) * ipset->allocated);

	if (!ipset->items)
	{
		free (ipset);
		return NULL;
	}

	return ipset;
}

/* free set */
void fst_ipset_free (FSTIpSet *ipset)
{
	if (!ipset)
		return;

	free (ipset->items);
	free (ipset);
}

/*****************************************************************************/

/* add ip range to set */
void fst_ipset_add (FSTIpSet *ipset, in_addr_t first, in_addr_t last)
{
	unsigned long hfirst = ntohl (first);
	unsigned long hlast = ntohl (last);

	if (!ipset)
		return;

	if (ipset->item_count >= ipset->allocated)
	{
		/* alloc more memory */
		FSTIpSetItem *new_arr;
		unsigned int new_size;
		
		new_size = (ipset->allocated + ALLOC_INCREMENT) * sizeof (FSTIpSetItem);
		if (! (new_arr = realloc (ipset->items, new_size)))
			return; /* not much we could do */

		ipset->items = new_arr;
		ipset->allocated += ALLOC_INCREMENT;
	}

	if (hfirst > hlast)
	{
		ipset->items[ipset->item_count].first = hlast;
		ipset->items[ipset->item_count].last = hfirst;
	}
	else
	{
		ipset->items[ipset->item_count].first = hfirst;
		ipset->items[ipset->item_count].last = hlast;
	}

	ipset->item_count++;
}

/* returns TRUE if set contains ip */
int fst_ipset_contains (FSTIpSet *ipset, in_addr_t ip)
{
	unsigned int i;
	unsigned long hip = ntohl (ip);

	if (!ipset)
		return FALSE;

	for (i=0; i<ipset->item_count; i++)
	{
		if (hip >= ipset->items[i].first && hip <= ipset->items[i].last)
		{
#if 0
			FST_HEAVY_DBG_3 ("ip 0x%08X matching range 0x%08X-0x%08X",
							 hip, ipset->items[i].first, ipset->items[i].last);
#endif

			return TRUE;
		}
	}

	return FALSE;
}

/*****************************************************************************/

/* load set from file, returns number of loaded ranges or -1 on failure */
int fst_ipset_load (FSTIpSet *ipset, const char *filename)
{
	FILE *f;
	char *buf = NULL;

	if (! (f = fopen (filename, "r")))
		return -1;

	while (file_read_line (f, &buf))
	{
		in_addr_t first, last;
		char *ptr = buf;

		string_trim (ptr);

		/* ingore comments */
		if (*ptr == '#')
			continue;

		/* format: <description>:<first_ip>-<last_ip> */

		string_sep (&ptr, ":");
		first =	net_ip (string_sep (&ptr, "-"));
		last = net_ip (ptr);

		if (first == INADDR_NONE || first == 0 ||
			last == INADDR_NONE || last == 0) 
			continue;

		fst_ipset_add (ipset, first, last);
	}

	fclose (f);

	/* sort and return number of loaded nodes */
	return ipset->item_count;
}

/*****************************************************************************/
