/*
 * $Id: rx_layer.h,v 1.3 2004/02/01 08:17:12 hipnod Exp $
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

#ifndef GIFT_GT_RX_LAYER_H_
#define GIFT_GT_RX_LAYER_H_

/*****************************************************************************/

#include "rx_stack.h"
#include "io_buf.h"

/*****************************************************************************/

struct gt_rx_stack;
struct rx_layer;

struct rx_layer_ops
{
	BOOL (*init)     (struct rx_layer *layer, void *udata);
	void (*destroy)  (struct rx_layer *layer);
	void (*enable)   (struct rx_layer *layer);
	void (*disable)  (struct rx_layer *layer);
	void (*recv)     (struct rx_layer *layer, struct io_buf *io_buf);
};

struct rx_layer
{
	const char            *name;
	void                  *udata;
	struct rx_layer_ops   *ops;
	BOOL                   enabled;

	struct rx_layer       *upper;
	struct rx_layer       *lower;

	struct gt_rx_stack    *stack;
};

/*****************************************************************************/

struct rx_layer *gt_rx_layer_new     (struct gt_rx_stack *stack,
                                      const char *name,
                                      struct rx_layer_ops *ops, void *udata);
void             gt_rx_layer_free    (struct rx_layer *layer);

void             gt_rx_layer_disable (struct rx_layer *layer);
void             gt_rx_layer_enable  (struct rx_layer *layer);

/* pass a message buffer up to preceding layers */
void             gt_rx_layer_recv    (struct rx_layer *layer,
                                      struct io_buf *io_buf);

/*****************************************************************************/

#endif /* GIFT_GT_RX_LAYER_H_ */
