/*
 * $Id: asp_download.h,v 1.1 2004/12/04 01:31:17 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#ifndef __ASP_DOWNLOAD_H
#define __ASP_DOWNLOAD_H

/*****************************************************************************/

/* Called by gift to start downloading of a chunk. */
BOOL asp_giftcb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                                Source *source);

/* Called by gift to stop download. */
void asp_giftcb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                               Source *source, int complete);

/* Called by gift when a source is added to a download. */
BOOL asp_giftcb_source_add (Protocol *p,Transfer *transfer, Source *source);

/* Called by gift when a source is removed from a download. */
void asp_giftcb_source_remove (Protocol *p, Transfer *transfer,
                               Source *source);

/*****************************************************************************/

#endif /* __ASP_DOWNLOAD_H */
