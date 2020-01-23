/*
 * xfer.h
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

typedef void (*FT_TransferCB) (Chunk *chunk, char *data, size_t len);

typedef struct
{
	Connection    *c;                  /* see ft_transfer_ref */
	Chunk         *chunk;              /* ...                 */

	FT_TransferCB  callback;           /* where to report progress
	                                    * see ft_download and ft_upload in
	                                    * xfer.c */

	Dataset       *header;             /* HTTP headers */
	int            code;               /* HTTP status code last seen */

	unsigned long  ip;                 /* address of the user who is either
	                                    * leeching our node or is being leeched
	                                    * by it */
	unsigned short port;               /* used only by the client routines */

	char          *command;            /* request operator (GET, PUSH, ...) */
	char          *request;            /* exact request operand, url encoded */
	char          *request_path;       /* decoded copy of request */

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
} FT_Transfer;

/*****************************************************************************/

#include "http_client.h"
#include "http_server.h"

/*****************************************************************************/

FT_Transfer *ft_transfer_new   (FT_TransferCB cb,
                                unsigned long ip, unsigned short port,
                                off_t start, off_t stop);
void         ft_transfer_close (FT_Transfer *xfer, int force_close);

/*****************************************************************************/

void         http_connection_close (Connection *c, int force_close);
Connection  *http_connection_open  (unsigned long ip, unsigned short port);

/*****************************************************************************/

int   ft_transfer_set_request  (FT_Transfer *xfer, char *request);
FILE *ft_transfer_open_request (FT_Transfer *xfer, int *code);

/*****************************************************************************/

void ft_transfer_ref   (Connection *c, Chunk *chunk, FT_Transfer *xfer);
void ft_transfer_unref (Connection **c, Chunk **chunk, FT_Transfer **xfer);

/*****************************************************************************/

void ft_download (Chunk *chunk, char *segment, size_t len);
void ft_upload   (Chunk *chunk, char *segment, size_t len);

void ft_daemon_download   (Chunk *chunk, ProtocolCommand cmd, void *data);
void ft_daemon_upload     (Chunk *chunk, ProtocolCommand cmd, void *data);
int  ft_daemon_source_cmp (Source *a, Source *b);

/*****************************************************************************/

#endif /* __XFER_H */
