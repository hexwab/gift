/*
 * $Id: as_source.h,v 1.7 2004/12/24 12:06:26 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SOURCE_H
#define __AS_SOURCE_H

/*****************************************************************************/

typedef struct
{
	/* source host and port */
	in_addr_t host;
	in_port_t port; 

	/* the source's local ip */
	in_addr_t inside_ip;
	
	/* source's supernode host and port */
	in_addr_t shost;
	in_port_t sport;

	/* source's user and network name*/
	unsigned char *username;
	unsigned char *netname;

	/* supernode we got this source from */
	in_addr_t parent_host;
	in_port_t parent_port;

} ASSource;

/*****************************************************************************/

/* create new source */
ASSource *as_source_create ();

/* create copy of source */
ASSource *as_source_copy (ASSource *source);

/* free source */
void as_source_free (ASSource *source);

/*****************************************************************************/

/* returns TRUE if the sources are the same */
as_bool as_source_equal (ASSource *a, ASSource *b);

/* returns TRUE if the source is firewalled */
as_bool as_source_firewalled (ASSource *source);

/* returns TRUE if the source has enough info to send a push */
as_bool as_source_has_push_info (ASSource *source);

/*****************************************************************************/

#ifdef GIFT_PLUGIN

/* create source from gift url */
ASSource *as_source_unserialize (const char *str);

/* create url for gift. Caller frees returned string. */
char *as_source_serialize (ASSource *source);

#endif

/* Return static debug string with source data. Do not use for anything
 * critical because threading may corrupt buffer.
 */
char *as_source_str (ASSource *source);

/*****************************************************************************/

#endif /* __AS_SOURCE_H */

