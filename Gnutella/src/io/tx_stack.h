/*
 * $Id: tx_stack.h,v 1.6 2004/04/17 06:07:33 hipnod Exp $
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

#ifndef GIFT_GT_TX_STACK_H_
#define GIFT_GT_TX_STACK_H_

/*****************************************************************************/

typedef struct gt_tx_stack GtTxStack;

typedef void (*GtTxStackCleanup) (GtTxStack *stack, void *udata);

struct gt_tx_stack
{
	struct tx_layer  *layers;
	GtTxStackCleanup  cleanup;

	TCPC             *c;
	void             *udata;

	time_t            start_time;
};

/*****************************************************************************/

GtTxStack     *gt_tx_stack_new          (TCPC *c, BOOL tx_deflated);
void           gt_tx_stack_free         (GtTxStack *stack);

void           gt_tx_stack_abort        (GtTxStack *stack);

void           gt_tx_stack_set_handler  (GtTxStack *stack,
                                         GtTxStackCleanup cleanup, 
                                         void *udata);

BOOL           gt_tx_stack_queue        (GtTxStack *stack, const uint8_t *data,
                                         size_t len);

/* used by tx_link to send data on the connection...argh, i can't
 * come up with a good interface for this */
int            gt_tx_stack_send         (GtTxStack *stack, const uint8_t *data,
                                         size_t len);

/*****************************************************************************/

#endif /* GIFT_GT_TX_STACK_H_ */
