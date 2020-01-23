/*
 * $Id: asp_search.h,v 1.1 2004/12/04 01:31:17 mkern Exp $
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

#ifndef __ASP_SEARCH_H
#define __ASP_SEARCH_H

/*****************************************************************************/

/* Called by giFT to initiate search. */
BOOL asp_giftcb_search (Protocol *p, IFEvent *event, char *query,
                        char *exclude, char *realm, Dataset *meta);

/* Called by giFT to initiate browse. */
BOOL asp_giftcb_browse (Protocol *p, IFEvent *event, char *user, char *node);

/* Called by giFT to locate file. */
BOOL asp_giftcb_locate (Protocol *p, IFEvent *event, char *htype, char *hstr);

/* Called by giFT to cancel search/locate/browse. */
void asp_giftcb_search_cancel (Protocol *p, IFEvent *event);

/*****************************************************************************/

#endif /* __ASP_SEARCH_H */
