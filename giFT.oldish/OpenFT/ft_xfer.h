/*
 * ft_xfer.h
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

#ifndef __XFER_H
#define __XFER_H

/*****************************************************************************/

typedef void (*FTTransferCB) (Chunk *chunk, unsigned char *data, size_t len);

typedef struct
{
	Connection    *c;                  /* see ft_transfer_ref */
	Chunk         *chunk;              /* ...                 */

	FTTransferCB   callback;           /* where to report progress
	                                    * see ft_download and ft_upload in
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
} FTTransfer;

/*****************************************************************************/

#include "ft_http_client.h"
#include "ft_http_server.h"

/*****************************************************************************/

FTTransfer  *ft_transfer_new    (FTTransferCB cb,
                                 in_addr_t ip, unsigned short port,
                                 off_t start, off_t stop);
void         ft_transfer_close  (FTTransfer *xfer, int force_close);
void         ft_transfer_status (FTTransfer *xfer, SourceStatus status,
                                 char *text);

/*****************************************************************************/

void         http_connection_close (Connection *c, int force_close);
Connection  *http_connection_open  (in_addr_t ip, unsigned short port);

/*****************************************************************************/

int   ft_transfer_set_request  (FTTransfer *xfer, char *request);
FILE *ft_transfer_open_request (FTTransfer *xfer, int *code);

/*****************************************************************************/

void ft_transfer_ref   (Connection *c, Chunk *chunk, FTTransfer *xfer);
void ft_transfer_unref (Connection **c, Chunk **chunk, FTTransfer **xfer);

/*****************************************************************************/

void ft_download (Chunk *chunk, unsigned char *segment, size_t len);
void ft_upload   (Chunk *chunk, unsigned char *segment, size_t len);

/*****************************************************************************/

int openft_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source);
void openft_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source, int complete);
int openft_upload_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source, unsigned long avail);
void openft_upload_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source, unsigned long avail);
int openft_chunk_suspend (Protocol *p, Transfer *transfer, Chunk *chunk,
                          Source *source);
int openft_chunk_resume (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source);
int openft_source_remove (Protocol *p, Transfer *transfer, Source *source);
int openft_source_cmp (Protocol *p, Source *a, Source *b);
int openft_user_cmp (Protocol *p, char *a, char *b);

/*****************************************************************************/

#endif /* __XFER_H */
