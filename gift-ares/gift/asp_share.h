/*
 * $Id: asp_share.h,v 1.1 2004/12/04 01:31:17 mkern Exp $
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

#ifndef __ASP_SHARE_H
#define __ASP_SHARE_H

/*****************************************************************************/

/* Called by giFT so we can add custom data to shares. */
void *asp_giftcb_share_new (Protocol *p, Share *share);

/* Called be giFT for us to free custom data. */
void asp_giftcb_share_free (Protocol *p, Share *share, void *data);

/* Called by giFT when share is added. */
BOOL asp_giftcb_share_add (Protocol *p, Share *share, void *data);

/* Called by giFT when share is removed. */
BOOL asp_giftcb_share_remove (Protocol *p, Share *share, void *data);

/* Called by giFT when it starts/ends syncing shares. */
void asp_giftcb_share_sync (Protocol *p, int begin);

/* Called by giFT when user hides shares. */
void asp_giftcb_share_hide (Protocol *p);

/* Called by giFT when user shows shares. */
void asp_giftcb_share_show (Protocol *p);

/*****************************************************************************/

#endif /* __ASP_SHARE_H */
