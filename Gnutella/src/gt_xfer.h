/*
 * $Id: gt_xfer.h,v 1.27 2004/05/05 10:30:12 hipnod Exp $
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

#ifndef GIFT_GT_XFER_H_
#define GIFT_GT_XFER_H_

/*****************************************************************************/

struct transfer;
struct source;
struct chunk;

struct gt_transfer;

/*****************************************************************************/

char *gt_localize_request (struct gt_transfer *xfer, char *s_path,
                           BOOL *authorized);

/*****************************************************************************/
/* Push handling routines */

void  gt_push_source_add           (gt_guid_t *guid, in_addr_t ip,
                                    in_addr_t src_ip);
void  gt_push_source_remove        (gt_guid_t *guid, in_addr_t ip,
                                    in_addr_t src_ip);
BOOL  gt_push_source_add_xfer      (gt_guid_t *guid, in_addr_t ip,
                                    in_addr_t src_ip,
                                    struct gt_transfer *xfer);
BOOL  gt_push_source_add_conn      (gt_guid_t *guid, in_addr_t ip, TCPC *c);

void  gt_push_source_remove_xfer   (struct gt_transfer *xfer);
void  gt_push_source_remove_conn   (TCPC *c);

/*****************************************************************************/

int  gnutella_download_start  (struct protocol *p, struct transfer *transfer,
                               struct chunk *chunk, struct source *source);
void gnutella_download_stop   (struct protocol *p, struct transfer *transfer,
                               struct chunk *chunk, struct source *source,
                               BOOL complete);

/*****************************************************************************/

int  gnutella_upload_start (struct protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source,
                            unsigned long avail);
void gnutella_upload_stop  (struct protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source);
void gnutella_upload_avail (struct protocol *p, unsigned long avail);

/*****************************************************************************/

int gnutella_chunk_suspend (Protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source);
int gnutella_chunk_resume  (Protocol *p, struct transfer *transfer,
                            struct chunk *chunk, struct source *source);

/*****************************************************************************/

#endif /* GIFT_GT_XFER_H_ */
