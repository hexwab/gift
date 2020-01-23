/*
 * $Id: rx_layer.c,v 1.4 2004/02/01 08:17:12 hipnod Exp $
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

#include "rx_layer.h"

/*****************************************************************************/

void gt_rx_layer_disable (struct rx_layer *rx)
{
	if (!rx)
		return;

	rx->enabled = FALSE;
	rx->ops->disable (rx);
}

void gt_rx_layer_enable (struct rx_layer *rx)
{
	if (!rx)
		return;

	rx->enabled = TRUE;
	rx->ops->enable (rx);
}

void gt_rx_layer_recv (struct rx_layer *rx, struct io_buf *io_buf)
{
	struct rx_layer *upper;

	/* let the stack know we've started to receive some data */
	gt_rx_stack_recv_start (rx->stack);

	upper = rx->upper;
	assert (rx->upper != NULL);

	upper->ops->recv (upper, io_buf);

	/* 
	 * Let the stack know we're done. Currently, this will free the stack if
	 * rx_stack_abort() was called while we were receiving data.
	 *
	 * (Also note, the stack itself doesn't actually call free, but calls
	 * to the higher level callback's cleanup function, that calls 
	 * gt_rx_stack_free().
	 */
	gt_rx_stack_recv_end (rx->stack);
}

/*****************************************************************************/

struct rx_layer *gt_rx_layer_new (GtRxStack *stack, const char *name,
                               struct rx_layer_ops *ops, void *udata)
{
	struct rx_layer *rx;

	if (!(rx = NEW (struct rx_layer)))
		return NULL;

	rx->name  = name;
	rx->ops   = ops;    /* should we memdup this? */
	rx->udata = udata;
	rx->stack = stack;

	/*
	 * Call the child initialization function.
	 */
	if (!ops->init (rx, udata))
	{
		free (rx);
		return NULL;
	}

	return rx;
}

void gt_rx_layer_free (struct rx_layer *rx)
{
	if (!rx)
		return;

	/* tell the layer to free its data */
	rx->ops->destroy (rx);

	/* ops structues are global */
#if 0
	free (rx->ops);
#endif

	/* we free the layer itself here, at the top */
	FREE (rx);
}
