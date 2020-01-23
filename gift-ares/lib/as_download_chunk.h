/*
 * $Id: as_download_chunk.h,v 1.3 2004/09/13 01:01:18 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_CHUNK_H
#define __AS_DOWNLOAD_CHUNK_H

/*****************************************************************************/

typedef struct
{
	size_t start;    /* byte offset of chunk in file */
	size_t size;     /* size of entire chunk in bytes */
	size_t received; /* bytes already received */

	void *udata;     /* arbitrary user data */

} ASDownChunk;

/*****************************************************************************/

/* alloc chunk */
ASDownChunk *as_downchunk_create (size_t start, size_t size);

/* free chunk */
void as_downchunk_free (ASDownChunk *chunk);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_CHUNK_H */

