/*
 * $Id: gt_bind.h,v 1.2 2004/03/24 06:19:46 hipnod Exp $
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_BIND_H_
#define GIFT_GT_BIND_H_

/*****************************************************************************/

struct gt_node;

/* should be renamed to gt_bind_uptime() */
time_t gt_uptime                    (void);

/*****************************************************************************/

/* received an incoming connection on this binding */
void   gt_bind_clear_firewalled     (void);

/* a connection on this binding completed: used for sending ConnectBack
 * messages to the other side */
void   gt_bind_completed_connection (struct gt_node *remote);

/* whether this node is firewalled */
BOOL   gt_bind_is_firewalled        (void);

/*****************************************************************************/

void   gt_bind_init                 (void);
void   gt_bind_cleanup              (void);

/*****************************************************************************/

#endif /* GIFT_GT_BIND_H_ */
