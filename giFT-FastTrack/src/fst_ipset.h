/*
 * $Id: fst_ipset.h,v 1.2 2003/09/17 11:25:04 mkern Exp $
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

#ifndef __FST_IPSET_H__
#define __FST_IPSET_H__

#include "fst_fasttrack.h"

/**************************************************************************/

/* note: for lookup efficiency ips are stored in little endian internally */

typedef struct
{
	unsigned long first;
	unsigned long last;
} FSTIpSetItem;

typedef struct
{
	FSTIpSetItem *items;		/* array of items */

	unsigned int item_count;	/* number of used items */
	unsigned int allocated;		/* number of allocated items */
} FSTIpSet;

/*****************************************************************************/

/* alloc and init set */
FSTIpSet *fst_ipset_create ();

/* free set */
void fst_ipset_free (FSTIpSet *ipset);

/*****************************************************************************/

/* add ip range to set */
void fst_ipset_add (FSTIpSet *ipset, in_addr_t first, in_addr_t last);

/* returns TRUE if set contains ip */
int fst_ipset_contains (FSTIpSet *ipset, in_addr_t ip);

/*****************************************************************************/

/* load set from file, returns number of loaded ranges or -1 on failure */
int fst_ipset_load (FSTIpSet *ipset, const char *filename);

/*****************************************************************************/

#endif /* __FST_IPSET_H__ */
