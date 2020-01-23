/*
 * $Id: rx_stack.h,v 1.3 2003/09/19 00:10:30 hipnod Exp $
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

#ifndef GIFT_GT_RX_STACK_H_
#define GIFT_GT_RX_STACK_H_

/*****************************************************************************/

#include "gt_gnutella.h"

/*****************************************************************************/

#define IO_DEBUG             gt_config_get_int("io/debug=0")

/*****************************************************************************/

struct gt_rx_stack;
struct gt_node;
struct gt_packet;

typedef struct gt_rx_stack GtRxStack;

typedef void (*GtRxStackHandler) (void *udata, struct gt_packet *packet);
typedef void (*GtRxStackCleanup) (void *udata);

/*****************************************************************************/

GtRxStack     *gt_rx_stack_new          (struct gt_node *node, TCPC *c,
                                         BOOL rx_inflated);
void           gt_rx_stack_free         (GtRxStack *stack);

void           gt_rx_stack_set_handler  (GtRxStack *stack,
                                         GtRxStackHandler handler,
                                         GtRxStackCleanup cleanup,
                                         void *udata);

void           gt_rx_stack_recv_start   (GtRxStack *stack);
void           gt_rx_stack_recv_end     (GtRxStack *stack);

void           gt_rx_stack_abort        (GtRxStack *stack);

/*****************************************************************************/

#endif /* GIFT_GT_RX_STACK_H_ */
