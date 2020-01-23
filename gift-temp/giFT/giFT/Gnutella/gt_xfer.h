/*
 * $Id: gt_xfer.h,v 1.9 2003/04/30 03:49:20 hipnod Exp $
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

#ifndef __GT_XFER_H__
#define __GT_XFER_H__

/*****************************************************************************/

struct _gt_transfer;
struct _chunk;
struct _transfer;
struct _source;

struct _gt_share;

#define Source   struct _source
#define Chunk    struct _chunk
#define Transfer struct _transfer

typedef struct _gt_source
{
	uint32_t        user_ip;
	uint16_t        user_port;
	uint32_t        server_ip;
	uint16_t        server_port;
	int             firewalled;
	gt_guid        *guid;
	uint32_t        index;
	char           *filename;
} GtSource;

/*****************************************************************************/

void          gt_source_free       (GtSource *gt);
GtSource     *gt_source_new        (char *url);

char         *gt_source_url_new    (char *filename, uint32_t index,
                                    in_addr_t user_ip, uint16_t user_port,
                                    in_addr_t server_ip, uint16_t server_port,
                                    int firewalled, gt_guid *client_id);

/* temporary backward compat */
char            *gt_share_url_new    (struct _gt_share *share,
                                       in_addr_t user_ip, uint16_t user_port,
                                       in_addr_t server_ip,
                                       uint16_t server_port,
                                       int firewalled,
                                       gt_guid *client_id);

/*****************************************************************************/

#if 0
char       *gt_share_url_new (struct _gt_share *share, unsigned long ip,
                              unsigned short port, gt_guid *client_guid);
#endif
char *gt_localize_request (struct _gt_transfer *xfer, char *s_path,
                           int *authorized);

/*****************************************************************************/

int  gnutella_download_start  (struct _protocol *p, Transfer *transfer,
                               Chunk *chunk, Source *source);
void gnutella_download_stop   (struct _protocol *p, Transfer *transfer,
                               Chunk *chunk, Source *source, int complete);

int  gnutella_upload_start (struct _protocol *p, Transfer *transfer,
                            Chunk *chunk, Source *source, unsigned long avail);
void gnutella_upload_stop  (struct _protocol *p, Transfer *transfer,
                            Chunk *chunk, Source *source);
void gnutella_upload_avail (struct _protocol *p, unsigned long avail);

int gnutella_chunk_suspend (Protocol *p, Transfer *transfer, Chunk *chunk,
                            Source *source);
int gnutella_chunk_resume  (Protocol *p, Transfer *transfer, Chunk *chunk,
                            Source *source);

int gnutella_source_cmp    (Protocol *p, Source *a, Source *b);
int gnutella_source_remove (Protocol *p, Transfer *transfer, Source *source);

#undef Source
#undef Chunk
#undef Transfer

/*****************************************************************************/

#endif /* __GT_XFER_H__ */
