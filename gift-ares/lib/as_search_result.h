/*
 * $Id: as_search_result.h,v 1.2 2004/09/24 22:27:20 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SEARCH_RESULT_H
#define __AS_SEARCH_RESULT_H

/*****************************************************************************/

typedef struct as_result_t
{
	as_uint16 search_id; /* search id used on network or zero if there is
	                      * none. e.g. for hash search results */
	
	ASSource *source;    /* info about the user this came from */
	ASMeta   *meta;      /* meta tags */

	ASRealm        realm;

	ASHash        *hash;
	size_t         filesize;
	unsigned char *filename;
	unsigned char *fileext;

	/* always 0x61? (could be bandwidth) */
	as_uint8  unknown;
	as_uint8  unk[5];

} ASResult;

/*****************************************************************************/

/* create empty search result */
ASResult *as_result_create ();

/* create search result from packet */
ASResult *as_result_parse (ASPacket *packet);

/* free search result */
void as_result_free (ASResult *result);

/*****************************************************************************/

#endif /* __AS_SEARCH_RESULT_H */

