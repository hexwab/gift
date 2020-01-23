/*
 * $Id: tx_stack.c,v 1.12 2004/04/17 06:07:33 hipnod Exp $
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
#include "gt_packet.h"               /* gt_packet_log XXX */

#include "io/tx_stack.h"
#include "io/tx_layer.h"
#include "io/io_buf.h"

/*****************************************************************************/

extern struct tx_layer_ops gt_tx_packet_ops;
extern struct tx_layer_ops gt_tx_deflate_ops;
extern struct tx_layer_ops gt_tx_link_ops;

/*****************************************************************************/

static struct use_tx_layer
{
	const char          *name;
	struct tx_layer_ops *ops;
} tx_layers[] =
{
	{ "tx_link",    &gt_tx_link_ops,   },
	{ "tx_deflate", &gt_tx_deflate_ops },
	{ "tx_packet",  &gt_tx_packet_ops  },
};

/*****************************************************************************/

static void foreach_tx_child (struct tx_layer *tx, 
                              void (*exec) (struct tx_layer *tx))
{
	struct tx_layer *next;

	while (tx != NULL)
	{
		/* grab the next element first so the callback can call free */
		next = tx->lower;

		exec (tx);

		tx = next;
	}
}

static struct tx_layer *tx_push_layer (struct tx_layer *below, 
                                       struct tx_layer *above)
{
	if (above)
		above->lower = below;

	if (below)
		below->upper = above;

	return above;
}

static void destroy_tx (struct tx_layer *tx)
{
	gt_tx_layer_free (tx);
}

static void disable_tx (struct tx_layer *tx)
{
	gt_tx_layer_disable (tx);
}

static void disable_all_tx_layers (struct tx_layer *layers)
{
	if (!layers)
		return;

	assert (layers->upper == NULL);

	foreach_tx_child (layers, disable_tx);
}

static void free_all_tx_layers (struct tx_layer *layers)
{
	if (!layers)
		return;

	disable_all_tx_layers (layers);
	foreach_tx_child (layers, destroy_tx);
}

static struct tx_layer *alloc_tx_layers (GtTxStack *stack, BOOL tx_deflated)
{
	struct tx_layer *new_layer;
	struct tx_layer *layer      = NULL;
	int i;

	for (i = 0; i < sizeof(tx_layers) / sizeof(tx_layers[0]); i++)
	{
		if (!strcmp (tx_layers[i].name, "tx_deflate") && !tx_deflated)
			continue;

		if (!(new_layer = gt_tx_layer_new (stack, tx_layers[i].name, 
		                                   tx_layers[i].ops)))
		{
			foreach_tx_child (layer, destroy_tx);
			return NULL;
		}

		layer = tx_push_layer (layer, new_layer);
	}

	return layer;
}

GtTxStack *gt_tx_stack_new (TCPC *c, BOOL tx_deflated)
{
	struct gt_tx_stack *stack;
	int                 size;

	if (!(stack = NEW (struct gt_tx_stack)))
		return NULL;

	if (!(stack->layers = alloc_tx_layers (stack, tx_deflated)))
	{
		free (stack);
		return NULL;
	}

	/* set the send buffer to a not too high value */
	size = 256;

	if (setsockopt (c->fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof (size)) != 0)
		GT->DBGSOCK (GT, c, "Error setting sndbuf size: %s", GIFT_NETERROR());

	stack->c             = c;
	stack->start_time    = time (NULL);

	return stack;
}

/*****************************************************************************/

void gt_tx_stack_free (GtTxStack *stack)
{
	if (!stack)
		return;

	free_all_tx_layers (stack->layers);
	FREE (stack);
}

void gt_tx_stack_abort (GtTxStack *stack)
{
	stack->cleanup (stack, stack->udata);
}

/*****************************************************************************/

static void activate_tx (struct tx_layer *tx)
{
	tx->ops->toggle (tx, FALSE);
}

/*
 * Start sending data down the stack.  Called asynchronously by the topmost
 * layer.
 */
void gt_tx_stack_activate (GtTxStack *stack)
{
	foreach_tx_child (stack->layers, activate_tx);
}

static void deactivate_tx (struct tx_layer *tx)
{
	tx->ops->toggle (tx, TRUE);
}

void gt_tx_stack_deactivate (GtTxStack *stack)
{
	foreach_tx_child (stack->layers, deactivate_tx);
}

/*****************************************************************************/

BOOL gt_tx_stack_queue (GtTxStack *stack, const uint8_t *data, size_t len)
{
	struct io_buf   *io_buf;
	struct tx_layer *tx;
	uint8_t         *ptr;
	tx_status_t      ret;
	GtPacket         pkt;

	if (!(io_buf = io_buf_new (len)))
		return FALSE;

	ptr = io_buf_write_ptr (io_buf);

	memcpy (ptr, data, len);
	io_buf_push (io_buf, len);

	tx = stack->layers;

	/* send the data on its way down the stack */
	if ((ret = tx->ops->queue (tx, io_buf)) != TX_OK)
	{
		GT->DBGSOCK (GT, stack->c, "bad txstatus: %d", ret);
		gt_tx_stack_abort (stack);
		return FALSE;
	}

	pkt.data = (unsigned char *)data;
	pkt.len  = len;

	gt_packet_log (&pkt, stack->c, TRUE);

	/*
	 * Activate the stack if not active already. NOTE: this actually
	 * sucks bad when using compression because we end up enabling, then
	 * disabling right away until some data is compressed by nagle timer.
	 */
	gt_tx_stack_activate (stack);

	return TRUE;
}

int gt_tx_stack_send (GtTxStack *stack, const uint8_t *data, size_t len)
{
	int   ret;

	/* check if the file descriptor has an error */
	if (net_sock_error (stack->c->fd))
		return -1;

	ret = tcp_send (stack->c, (unsigned char *)data, len);

	return ret;
}

void gt_tx_stack_set_handler (GtTxStack *stack, GtTxStackCleanup cleanup, 
                              void *udata)
{
	stack->cleanup = cleanup;
	stack->udata   = udata;
}
