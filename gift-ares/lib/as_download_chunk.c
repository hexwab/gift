/*
 * $Id: as_download_chunk.c,v 1.2 2004/09/09 16:55:46 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* alloc chunk */
ASDownChunk *as_downchunk_create (size_t start, size_t size)
{
	ASDownChunk *chunk;

	if (!(chunk = malloc (sizeof (ASDownChunk))))
		return NULL;

	chunk->start = start;
	chunk->size = size;
	chunk->received = 0;
	chunk->udata = NULL;

	return chunk;
}

/* free chunk */
void as_downchunk_free (ASDownChunk *chunk)
{
	if (!chunk)
		return;

	free (chunk);
}

/*****************************************************************************/
