/*
 * $Id: tx_layer.h,v 1.5 2004/01/31 13:33:17 hipnod Exp $
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

#ifndef GIFT_GT_TX_LAYER_H_
#define GIFT_GT_TX_LAYER_H_

/*****************************************************************************/

struct tx_layer_ops;
struct tx_layer;

struct io_buf;
struct gt_tx_stack;

/*
 * Return codes from the TX stack functions that lets the caller know
 * what processing occured.
 */
typedef enum tx_status
{
	TX_OK,
	TX_FULL,             /* lower layer became saturated */
	TX_EMPTY,            /* no waiting data */
	TX_PARTIAL,          /* buffer partially read */
	TX_ERROR,            /* general error */
} tx_status_t;

struct tx_layer_ops
{
	BOOL        (*init)       (struct tx_layer *tx);
	void        (*destroy)    (struct tx_layer *tx);

	/*
	 * If the layer is capable of consuming data (for example
	 * by sending it out on a connection), begin or stop flushing by obeying
	 * the 'stop' argument.  Only the bottommost layer in a stack
	 * should implement this.
	 */
	void        (*toggle)     (struct tx_layer *tx, BOOL stop);

	/* upper layer has sent us a buffer */
	tx_status_t (*queue)      (struct tx_layer *tx, struct io_buf *io_buf);

	/* lower layer wants us to send a buffer */
	tx_status_t (*ready)      (struct tx_layer *tx);   /* lower layer wants data */

	/* enable/disable this layer completely */
	void        (*enable)     (struct tx_layer *tx);
	void        (*disable)    (struct tx_layer *tx);
};

struct tx_layer
{
	void                *udata;
	struct tx_layer_ops *ops;

	struct tx_layer     *upper;
	struct tx_layer     *lower;

	/* leftovers from previous queue operations */
	struct io_buf       *partial_buf;

	struct gt_tx_stack  *stack;
	const char          *name;
};

/*****************************************************************************/

struct tx_layer *gt_tx_layer_new      (struct gt_tx_stack *stack,
                                       const char *name,
                                       struct tx_layer_ops *ops);
void             gt_tx_layer_free     (struct tx_layer *layer);

void             gt_tx_layer_enable   (struct tx_layer *layer);
void             gt_tx_layer_disable  (struct tx_layer *layer);

tx_status_t      gt_tx_layer_queue    (struct tx_layer *layer,
                                       struct io_buf *buf);
tx_status_t      gt_tx_layer_ready    (struct tx_layer *layer);

/*****************************************************************************/

#endif /* GIFT_GT_TX_LAYER_H_ */
