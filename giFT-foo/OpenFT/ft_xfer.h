/*
 * $Id: ft_xfer.h,v 1.15 2003/06/26 09:24:21 jasta Exp $
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

#ifndef __FT_XFER_H
#define __FT_XFER_H

/*****************************************************************************/

typedef struct
{
	TCPC          *c;                  /* see ft_transfer_ref */
	Chunk         *chunk;              /* ...                 */

	Dataset       *header;             /* HTTP headers */
	int            code;               /* HTTP status code last seen */

	in_addr_t      ip;                 /* address of the user who is either
	                                    * leeching our node or is being leeched
	                                    * by it */
	in_port_t      port;               /* used only by the client routines */

	char          *command;            /* request operator (GET, PUSH, ...) */
	char          *request;            /* exact request operand, url encoded */
	char          *version;            /* HTTP version resembling HTTP/1.1 */
	char          *request_path;       /* url decoded copy of request */

	off_t          start;              /* range begin */
	off_t          stop;               /* range stop.  0 is an exception which
	                                    * will be translated to the total file
	                                    * size as soon as known */

	/* used by the server routines for uploading */
	FILE          *f;                  /* used only by the server routines */
	Share         *share_authd;        /* hack for the new sharing
	                                    * interface...ugh */
	BOOL           share_authd_alloc;  /* do we need to share_free? */

	/* more hacks... once again, i promise this is being rewritten, sigh */
	BOOL           dying;
} FTTransfer;

/*****************************************************************************/

#include "ft_http_client.h"
#include "ft_http_server.h"

/*****************************************************************************/

FTTransfer  *ft_transfer_new    (in_addr_t ip, in_port_t port,
                                 off_t start, off_t stop);
void         ft_transfer_close  (FTTransfer *xfer, int force_close);
void         ft_transfer_status (FTTransfer *xfer, SourceStatus status,
                                 char *text);

/*****************************************************************************/

void  http_connection_close (TCPC *c, int force_close);
TCPC *http_connection_open  (in_addr_t ip, in_port_t port);

/*****************************************************************************/

int   ft_transfer_set_request  (FTTransfer *xfer, char *request);
FILE *ft_transfer_open_request (FTTransfer *xfer, int *code);

/*****************************************************************************/

void ft_transfer_ref   (TCPC *c, Chunk *chunk, FTTransfer *xfer);
void ft_transfer_unref (TCPC **c, Chunk **chunk, FTTransfer **xfer);

/*****************************************************************************/

unsigned long ft_upload_avail (void);

/*****************************************************************************/

int openft_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source);
void openft_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                           Source *source, int complete);
void openft_upload_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source);
void openft_upload_avail (Protocol *p, unsigned long avail);
int openft_chunk_suspend (Protocol *p, Transfer *transfer, Chunk *chunk,
                          Source *source);
int openft_chunk_resume (Protocol *p, Transfer *transfer, Chunk *chunk,
                         Source *source);
int openft_source_remove (Protocol *p, Transfer *transfer, Source *source);
int openft_source_cmp (Protocol *p, Source *a, Source *b);
int openft_user_cmp (Protocol *p, char *a, char *b);

/*****************************************************************************/

#endif /* __FT_XFER_H */
