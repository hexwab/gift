/*
 * $Id: gt_message.h,v 1.3 2004/03/24 06:21:13 hipnod Exp $
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

#ifndef GIFT_GT_MESSAGE_H_
#define GIFT_GT_MESSAGE_H_

/*****************************************************************************/

void gnutella_start_connection (int fd, input_id id, TCPC *c);

/*****************************************************************************/

/* HACK: this isn't available in libgift, but giftd */
extern unsigned long upload_availability (void);

/*****************************************************************************/

#endif /* GIFT_GT_MESSAGE_H_ */
