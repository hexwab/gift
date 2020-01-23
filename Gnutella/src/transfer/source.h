/*
 * $Id: source.h,v 1.4 2003/12/22 02:46:34 hipnod Exp $
 *
 * Copyright (C) 2002-2003 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_TRANSFER_SOURCE_H_
#define GIFT_GT_TRANSFER_SOURCE_H_

/*****************************************************************************/

struct transfer;
struct source;

typedef struct gt_source
{
	uint32_t        user_ip;
	uint16_t        user_port;
	uint32_t        server_ip;
	uint16_t        server_port;
	BOOL            firewalled;
	gt_guid_t      *guid;
	uint32_t        index;
	char           *filename;

	/*
	 * Parameters we don't understand, but were in the source URL.
	 * Parsed for forwards compatibility reasons, so newer versions
	 * can play with the same state files as older versions).
	 */
	Dataset        *extra;

	/*
	 * Fields that get set depending on what happens to this source.
	 * This should be in a shared per-server structure, actually.
	 */
	time_t          retry_time;       /* used for Retry-After; pollMin */
	char           *status_txt;       /* previous status text message */
	BOOL            uri_res_failed;   /* uri-res request failed */
	BOOL            connect_failed;   /* last connection attempt failed */
} GtSource;

/*****************************************************************************/

GtSource     *gt_source_new             (void);
void          gt_source_free            (GtSource *gt);

char         *gt_source_serialize       (GtSource *src);
GtSource     *gt_source_unserialize     (const char *url);

void          gt_source_set_ip          (GtSource *src, in_addr_t port);
void          gt_source_set_port        (GtSource *src, in_port_t port);
void          gt_source_set_index       (GtSource *src, uint32_t index);
void          gt_source_set_server_ip   (GtSource *src, in_addr_t server_ip);
void          gt_source_set_server_port (GtSource *src, in_port_t server_port);
void          gt_source_set_firewalled  (GtSource *src, BOOL fw);

BOOL          gt_source_set_filename    (GtSource *src, const char *filename);
BOOL          gt_source_set_guid        (GtSource *src, const gt_guid_t *guid);

/*****************************************************************************/

/* deprecated */
char         *gt_source_url_new    (const char *filename, uint32_t index,
                                    in_addr_t user_ip, uint16_t user_port,
                                    in_addr_t server_ip, uint16_t server_port,
                                    BOOL firewalled,
                                    const gt_guid_t *client_id);

/*****************************************************************************/

int  gnutella_source_cmp    (Protocol *p, struct source *a, struct source *b);
BOOL gnutella_source_add    (Protocol *p, struct transfer *transfer,
                             struct source *source);
void gnutella_source_remove (Protocol *p, struct transfer *transfer,
                             struct source *source);

/*****************************************************************************/

#endif /* GIFT_GT_TRANSFER_SOURCE_H_ */
