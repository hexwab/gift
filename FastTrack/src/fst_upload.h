/*
 * $Id: fst_upload.h,v 1.4 2004/03/08 21:09:57 mkern Exp $
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

#ifndef __FST_UPLOAD_H
#define __FST_UPLOAD_H

#include "fst_fasttrack.h"

/*****************************************************************************/

/* size of upload data buffer */
#define FST_UPLOAD_BUFFER_SIZE	4096

/*****************************************************************************/

typedef struct
{
	Transfer *transfer;		/* transfer / chunk this upload is connected to */
	Chunk *chunk;	
	Share *share;			/* giFT's share structure for this upload */

	FSTHttpHeader *request;	/* HTTP request the downloader sent */
	char *user;				/* username sent to giFT / front end */
	off_t start, stop;		/* requested range (exclusive stop) */

	TCPC *tcpcon;			/* connection to downloader */
	FILE *file;				/* handle to uploaded file */
	unsigned char *data;	/* buffer for upload data so we don't have it
	                         * on the stack
	                         */

} FSTUpload;

/*****************************************************************************/

/* called by http server for every received GET request */
int fst_upload_process_request (FSTHttpServer *server, TCPC *tcpcon,
                                FSTHttpHeader *request);

/*****************************************************************************/

/* called by gift to stop upload on user's request */
void fst_giftcb_upload_stop (Protocol *p, Transfer *transfer,
                             Chunk *chunk, Source *source);

/*****************************************************************************/

/* alloc and init upload */
FSTUpload *fst_upload_create (TCPC *tcpcon, FSTHttpHeader *request);

/* free push */
void fst_upload_free (FSTUpload *upload);

/*****************************************************************************/

#endif /* __FST_UPLOAD_H */
