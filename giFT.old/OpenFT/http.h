/*
 * http.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __HTTP_H
#define __HTTP_H

/*****************************************************************************/

typedef void (*OpenFT_TransferCB) (Chunk *chunk, char *data, size_t len);

typedef struct
{
	/* help give the stats back to the daemon */
	OpenFT_TransferCB callback;

	/* connection downloading the content */
	Connection    *c;

	/* this should really be handled by the chunk parent */
	FILE          *f;

	/* HTTP header information */
	Dataset       *header;

	unsigned long  ip;
	unsigned short port;

	char          *filename;
	char          *encoded;

	char          *hash;

	/* Range: request */
	unsigned long  range_start;
	unsigned long  range_stop;

	/* when requesting an indirect download, the first request must be flagged
	 * as indirect so that it's not written to the file */
	int            indirect;
} OpenFT_Transfer;

#define OPENFT_TRANSFER(chunk) ((OpenFT_Transfer *)chunk->data)

/*****************************************************************************/

OpenFT_Transfer *http_transfer_new (OpenFT_TransferCB cb,
                                    unsigned long ip, unsigned short port,
                                    char *filename, size_t start, size_t stop);

char *http_handle_cgiquery (Connection *c, char *request);
void  http_handle_incoming (Protocol *p, Connection *c);
void  http_cancel          (Chunk *chunk);

int   http_push_file       (Chunk *chunk);
int   http_pull_file       (Chunk *chunk, char *source, int indirect);

/*****************************************************************************/

#endif /* __HTTP_H */
