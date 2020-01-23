/*
 * $Id: asp_upload.h,v 1.1 2004/12/04 01:31:17 mkern Exp $
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

#ifndef __ASP_UPLOAD_H
#define __ASP_UPLOAD_H

/*****************************************************************************/

/* Register internal upload callbacks with ares library. */
void asp_upload_register_callbacks ();

/* Called by giFT to stop upload on user's request. */
void asp_giftcb_upload_stop (Protocol *p, Transfer *transfer,
                             Chunk *chunk, Source *source);

/*****************************************************************************/

#endif /* __ASP_UPLOAD_H */
