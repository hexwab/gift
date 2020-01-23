/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#include "sl_soulseek.h"
#include "sl_share.h"

/*****************************************************************************/

static BOOL shares_shown = TRUE;

/*****************************************************************************/

/* called by giFT so we can add custom data to shares */
void *sl_gift_cb_share_new(Protocol *p, Share *share)
{
	return NULL;
}

/* called by giFT for us to free custom data */
void sl_gift_cb_share_free(Protocol *p, Share *share, void *data)
{

}

/* called by giFT when share is added */
BOOL sl_gift_cb_share_add(Protocol *p, Share *share, void *data)
{
	return TRUE;
}

/* called by giFT when share is removed */
BOOL sl_gift_cb_share_remove(Protocol *p, Share *share, void *data)
{
	return TRUE;
}

/* called by giFT when it starts/ends syncing shares */
void sl_gift_cb_share_sync(Protocol *p, int begin)
{

}

/* called by giFT when user hides shares */
void sl_gift_cb_share_hide(Protocol *p)
{
	shares_shown = FALSE;
}

/* called by giFT when user shows shares */
void sl_gift_cb_share_show(Protocol *p)
{
	shares_shown = TRUE;
}

/*****************************************************************************/
