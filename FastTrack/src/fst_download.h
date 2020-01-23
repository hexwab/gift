/*
 * $Id: fst_download.h,v 1.10 2004/03/08 21:09:57 mkern Exp $
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

#ifndef __FST_DOWNLOAD_H
#define __FST_DOWNLOAD_H

#include "fst_fasttrack.h"
#include "fst_http_client.h"

/*****************************************************************************/

/* called by gift to start downloading of a chunk */
int fst_giftcb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
							   Source *source);

/* called by gift to stop download */
void fst_giftcb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
							   Source *source, int complete);

/* called by gift to add a source */
BOOL fst_giftcb_source_add (Protocol *p,Transfer *transfer, Source *source);

/* called by gift to remove source */
void fst_giftcb_source_remove (Protocol *p, Transfer *transfer,
							   Source *source);

/*****************************************************************************/

/* start download for source, optionally using existing tcpcon */
int fst_download_start (Source *source, TCPC *tcpcon);

/* Parses new format url.
 * Returns FSTHash which caller frees or NULL on failure.
 * params receives a dataset with additional params, caller frees, may be NULL
 */
FSTHash *fst_download_parse_url (char *url, in_addr_t *ip,
                                 in_port_t *port, Dataset **params);

/*****************************************************************************/

#endif /* __FST_DOWNLOAD_H */
