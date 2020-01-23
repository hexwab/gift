/*
 * $Id: tx_layer.c,v 1.7 2004/03/24 06:37:30 hipnod Exp $
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

#include "gt_gnutella.h"

#include "io/tx_stack.h"
#include "io/tx_layer.h"
#include "io/io_buf.h"

/*****************************************************************************/

struct tx_layer *gt_tx_layer_new (GtTxStack *stack, const char *name,
                                  struct tx_layer_ops *ops)
{
	struct tx_layer *tx;

	if (!(tx = NEW (struct tx_layer)))
		return NULL;

	tx->ops         = ops;
	tx->name        = name;
	tx->stack       = stack;
	tx->partial_buf = NULL;

	if (!ops->init (tx))
	{
		free (tx);
		return NULL;
	}

	return tx;
}

void gt_tx_layer_free (struct tx_layer *layer)
{
	if (!layer)
		return;

	io_buf_free (layer->partial_buf);

	layer->ops->destroy (layer);

	FREE (layer);
}

/*****************************************************************************/

static tx_status_t queue_data (struct tx_layer *tx, struct io_buf *io_buf)
{
	tx_status_t ret;

	ret = tx->ops->queue (tx, io_buf);

	/*
	 * If the message didn't get completely written, buffer the partial
	 * message buffer until it's finished.
	 */
	if (ret == TX_PARTIAL)
	{
		assert (io_buf_read_avail (io_buf) > 0);

		tx->partial_buf = io_buf;
		return TX_OK;
	}

	return ret;
}

/* send a message to the layer underneath this one */
tx_status_t gt_tx_layer_queue (struct tx_layer *tx, struct io_buf *io_buf)
{
	struct tx_layer *lower = tx->lower;

	if (lower->partial_buf)
		return TX_FULL;

	return queue_data (lower, io_buf);
}

/* let upper layer know we're writable and get data from it */
tx_status_t gt_tx_layer_ready (struct tx_layer *tx)
{
	struct tx_layer *upper;
	tx_status_t      ret;

	upper = tx->upper;

	/*
	 * If there is a partially written buffer on the layer, try to finish it
	 * off before asking for more data by pretending the upper layer tried to
	 * write it.
	 *
	 * Doing this avoids a special case in each layer where a partial buffer
	 * is kept set aside and checked before calling gt_tx_layer_ready(), leading
	 * to less code.
	 */
	if (tx->partial_buf)
	{
		struct io_buf *io_buf = tx->partial_buf;

		tx->partial_buf = NULL;

		/* this ends up calling this layer's queue func */
		ret = queue_data (tx, io_buf);

		/*
		 * Can't happen because layer wouldn't have invoked us.
		 */
		assert (ret != TX_FULL);
		assert (ret != TX_EMPTY);

		/*
		 * Upper layer can't be safely invoked again, even if the partial
		 * buffer was completed, because we don't know if the lower layer has
		 * any more room.  So, the lower layer must reinvoke tx_layer_ready()
		 * to write more data.
		 */
		return ret;
	}

	ret = upper->ops->ready (upper);
	assert (ret != TX_FULL);

	return ret;
}

/*****************************************************************************/

void gt_tx_layer_enable (struct tx_layer *layer)
{
	layer->ops->enable (layer);
}

void gt_tx_layer_disable (struct tx_layer *layer)
{
	layer->ops->disable (layer);
}
