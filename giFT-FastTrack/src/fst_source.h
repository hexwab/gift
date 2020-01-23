/*
 * $Id: fst_source.h,v 1.1 2004/03/10 02:07:01 mkern Exp $
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

#ifndef __FST_SOURCE_H
#define __FST_SOURCE_H

#include "fst_fasttrack.h"

/*****************************************************************************/

typedef struct
{
	in_addr_t ip;           /* ip of source */
	in_port_t port;         /* port of source */

	in_addr_t snode_ip;     /* ip of source's supernode */
	in_port_t snode_port;   /* port of source's supernode */

	in_addr_t parent_ip;    /* The ip of the supernode _we_ were connected to 
	                         * when receiving this result. Important for not
	                         * sending pushes to the wrong supernode */

	char *username;         /* User name of source. */
	char *netname;          /* Network the source is on */
	
	unsigned int bandwidth; /* bandwidth in kbit/sec */

} FSTSource;

/*****************************************************************************/

/* create new source object */
FSTSource *fst_source_create ();

/* create new source object with the data from org_source */
FSTSource *fst_source_create_copy (FSTSource *org_source);

/* create new source object from url */
FSTSource *fst_source_create_url (const char *url);

/* free source object */
void fst_source_free (FSTSource *source);

/*****************************************************************************/

/* parse an url */
BOOL fst_source_decode (FSTSource *source, const char *url);

/* create an url, caller frees result */
char *fst_source_encode (FSTSource *source);

/*****************************************************************************/

/* returns TRUE if the sources are the same */
BOOL fst_source_equal (FSTSource *a, FSTSource *b);

/* returns TRUE if the source is firewalled */
BOOL fst_source_firewalled (FSTSource *source);

/* returns TRUE if the source has enough info to send a push */
BOOL fst_source_has_push_info (FSTSource *source);

/*****************************************************************************/

#endif /* __FST_SOURCE_H */
