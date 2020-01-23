/*
 * $Id: rx_packet.h,v 1.3 2004/01/31 13:33:17 hipnod Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_IO_RX_PACKET_H_
#define GIFT_GT_IO_RX_PACKET_H_

/*****************************************************************************/

typedef void (*rx_packet_handler_t) (void *udata, struct gt_packet *packet);

extern struct rx_layer_ops gt_rx_packet_ops;

void             gt_rx_packet_set_handler (struct rx_layer *rx,
                                           rx_packet_handler_t handler,
                                           void *udata);

/*****************************************************************************/

#endif /* GIFT_GT_IO_RX_PACKET_H_ */
