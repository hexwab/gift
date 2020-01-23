/*
 * $Id: ft_xfer.h,v 1.6 2003/04/26 20:07:18 hipnod Exp $
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

#ifndef __XFER_H
#define __XFER_H

/*****************************************************************************/

#include "http.h"

typedef void (*GtTransferCB) (Chunk *chunk, unsigned char *data, size_t len);

typedef struct _gt_transfer
{
	Connection    *c;                  /* see gt_transfer_ref */
	Chunk         *chunk;              /* ...                 */
	HTTP_Protocol *http;               /* which HTTP protocol owns this xfer */

	GtTransferCB  callback;            /* where to report progress
	                                    * see gt_download and gt_upload in
	                                    * xfer.c */

	Dataset       *header;             /* HTTP headers */
	int            code;               /* HTTP status code last seen */

	in_addr_t      ip;                 /* address of the user who is either
	                                    * leeching our node or is being leeched
	                                    * by it */
	unsigned short port;               /* used only by the client routines */

	char          *command;            /* request operator (GET, PUSH, ...) */
	char          *request;            /* exact request operand, url encoded */
	char          *version;            /* HTTP version resembling HTTP/1.1 */
	char          *request_path;       /* url decoded copy of request */

	char          *content_type;       /* Content-Type: send from server or
	                                    * to client */
	char          *content_urns;       /* X-Gnutella-Content-URN: if requested
	                                    * by urn */

	off_t          start;              /* range begin */
	off_t          stop;               /* range stop.  0 is an exception which
	                                    * will be translated to the total file
	                                    * size as soon as known */

	/* used by the server routines for uploading */
	FILE          *f;                  /* used only by the server routines */
	char          *open_path;          /* path opened by the server */
	off_t          open_path_size;     /* size of the file on disk described
	                                    * by open_path */
	char          *hash;               /* openft's hash to deliever to the
	                                    * interface protocol when we register
	                                    * this upload */
	unsigned char  shared : 1;         /* see interface proto docs */
} GtTransfer;

/*****************************************************************************/

#include "ft_http_client.h"
#include "ft_http_server.h"

/*****************************************************************************/

GtTransfer  *gt_transfer_new    (HTTP_Protocol *http, GtTransferCB cb,
                                 in_addr_t ip, unsigned short port,
                                 off_t start, off_t stop);
void         gt_transfer_close  (GtTransfer *xfer, int force_close);
void         gt_transfer_status (GtTransfer *xfer, SourceStatus status,
                                 char *text);

/*****************************************************************************/

void         gt_http_connection_close  (HTTP_Protocol *http, Connection *c, 
                                        int force_close);
Connection  *gt_http_connection_open   (HTTP_Protocol *http, in_addr_t ip, 
                                        unsigned short port);
Connection  *gt_http_connection_lookup (HTTP_Protocol *http, in_addr_t ip, 
                                        unsigned short port);

/*****************************************************************************/

int   gt_transfer_set_request  (GtTransfer *xfer, char *request);
FILE *gt_transfer_open_request (GtTransfer *xfer, int *code);

/*****************************************************************************/

void gt_transfer_ref   (Connection *c, Chunk *chunk, GtTransfer *xfer);
void gt_transfer_unref (Connection **c, Chunk **chunk, GtTransfer **xfer);

/*****************************************************************************/

void gt_download (Chunk *chunk, unsigned char *segment, size_t len);
void gt_upload   (Chunk *chunk, unsigned char *segment, size_t len);

/*****************************************************************************/

void gt_download_cancel (Chunk *chunk, void *data);
void gt_upload_cancel   (Chunk *chunk, void *data);

/*****************************************************************************/

int gt_chunk_suspend (Chunk *chunk, Transfer *transfer, void *data);
int gt_chunk_resume  (Chunk *chunk, Transfer *transfer, void *data);

/*****************************************************************************/

#endif /* __XFER_H */
