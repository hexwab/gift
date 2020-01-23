/*
 * $Id: gt_xfer.h,v 1.16 2003/06/01 09:34:48 hipnod Exp $
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

struct gt_transfer;
struct chunk;
struct transfer;
struct source;

struct gt_share;

typedef struct gt_source
{
	uint32_t        user_ip;
	uint16_t        user_port;
	uint32_t        server_ip;
	uint16_t        server_port;
	int             firewalled;
	gt_guid_t      *guid;
	uint32_t        index;
	char           *filename;
} GtSource;

/*****************************************************************************/

void          gt_source_free       (GtSource *gt);
GtSource     *gt_source_new        (char *url);

char         *gt_source_url_new    (char *filename, uint32_t index,
                                    in_addr_t user_ip, uint16_t user_port,
                                    in_addr_t server_ip, uint16_t server_port,
                                    int firewalled, gt_guid_t *client_id);

/* temporary backward compat */
char            *gt_share_url_new    (struct gt_share *share,
                                      in_addr_t user_ip, uint16_t user_port,
                                      in_addr_t server_ip,
                                      uint16_t server_port,
                                      int firewalled,
                                      gt_guid_t *client_id);

/*****************************************************************************/

#if 0
char       *gt_share_url_new (struct gt_share *share, in_addr_t ip,
                              in_port_t port, gt_guid_t *client_guid);
#endif
char *gt_localize_request (struct gt_transfer *xfer, char *s_path,
                           int *authorized);

/*****************************************************************************/
/* Push handling routines */

void  gt_push_source_add           (gt_guid_t *guid, in_addr_t ip,
                                    in_addr_t src_ip);
void  gt_push_source_remove        (gt_guid_t *guid, in_addr_t ip,
                                    in_addr_t src_ip);
int   gt_push_source_add_xfer      (gt_guid_t *guid, in_addr_t ip,
                                    in_addr_t src_ip,
                                    struct gt_transfer *xfer);
int   gt_push_source_add_conn      (gt_guid_t *guid, in_addr_t ip, TCPC *c);

void  gt_push_source_remove_xfer   (struct gt_transfer *xfer);
void  gt_push_source_remove_conn   (TCPC *c);

/*****************************************************************************/

int  gnutella_download_start  (struct protocol *p, struct transfer *transfer,
                               struct chunk *chunk, struct source *source);
void gnutella_download_stop   (struct protocol *p, struct transfer *transfer,
                               struct chunk *chunk, struct source *source,
                               int complete);

int  gnutella_upload_start (struct protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source,
                            unsigned long avail);
void gnutella_upload_stop  (struct protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source);
void gnutella_upload_avail (struct protocol *p, unsigned long avail);

int gnutella_chunk_suspend (Protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source);
int gnutella_chunk_resume  (Protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source);

int gnutella_source_cmp    (Protocol *p, struct source *a, struct source *b);
int gnutella_source_remove (Protocol *p, struct transfer *transfer, Source *source);

#undef Source
#undef Chunk
#undef Transfer

/*****************************************************************************/

#endif /* __GT_XFER_H__ */
