/*
 * $Id: fst_share.h,v 1.1 2003/11/28 14:50:15 mkern Exp $
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

#ifndef __FST_SHARE_H
#define __FST_SHARE_H

#include "fst_fasttrack.h"

/*****************************************************************************/

/* called by giFT so we can add custom data to shares */
void *fst_giftcb_share_new (Protocol *p, Share *share);

/* called be giFT for us to free custom data */
void fst_giftcb_share_free (Protocol *p, Share *share, void *data);

/* called by giFT when share is added */
BOOL fst_giftcb_share_add (Protocol *p, Share *share, void *data);

/* called by giFT when share is removed */
BOOL fst_giftcb_share_remove (Protocol *p, Share *share, void *data);

/* called by giFT when it starts/ends syncing shares */
void fst_giftcb_share_sync (Protocol *p, int begin);

/* called by giFT when user hides shares */
void fst_giftcb_share_hide (Protocol *p);

/* called by giFT when user shows shares */
void fst_giftcb_share_show (Protocol *p);

/*****************************************************************************/

/* return TRUE if it makes sense to share at call time */
int fst_share_do_share ();

/* send all shares to supernode */
int fst_share_register_all ();

/* remove all shares from supernode */
int fst_share_unregister_all ();

/*****************************************************************************/

#endif /* __FST_SHARE_H */
