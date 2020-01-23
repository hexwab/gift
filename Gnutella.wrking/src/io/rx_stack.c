/*
 * $Id: rx_stack.c,v 1.8 2004/02/01 08:17:12 hipnod Exp $
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
#include "gt_node.h"

#include "rx_stack.h"
#include "rx_layer.h"
#include "rx_link.h"
#include "rx_inflate.h"
#include "rx_packet.h"

/*****************************************************************************/

struct gt_rx_stack
{
	TCPC                  *c;
	BOOL                   inflated;

	int                    depth;        /* how deep in the stack we have
	                                        currently called into the stack */
	BOOL                   aborted;      /* one of our layers bailed out */
	BOOL                   free_delayed; /* somebody called free during
	                                        message emission, and we delayed 
	                                        it for later */
	void                  *udata;
	struct rx_layer       *layers;

	GtRxStackHandler       handler;
	GtRxStackCleanup       cleanup;
};

static struct use_layer
{
	const char           *name;
	struct rx_layer_ops  *ops;
} layers[] =
{
	{ "rx_link",    &gt_rx_link_ops    },
	{ "rx_inflate", &gt_rx_inflate_ops },
	{ "rx_packet",  &gt_rx_packet_ops  },
};

/*****************************************************************************/

static void foreach_child (struct rx_layer *rx,
                           void (*exec)(struct rx_layer *rx, void *udata),
                           void *udata)
{
	struct rx_layer *next;

	while (rx != NULL)
	{
		/* grab the next element first so the callback can call free */
		next = rx->lower;

		exec (rx, udata);

		rx = next;
	}
}

static void disable_layer (struct rx_layer *rx, void *udata)
{
	gt_rx_layer_disable (rx);
}

static void destroy_foreach (struct rx_layer *rx, void *udata)
{
	gt_rx_layer_free (rx);
}

static void disable_all (GtRxStack *stack)
{
	struct rx_layer *layers = stack->layers;

	/* we must be at the top layer already */
	assert (layers->upper == NULL);

	foreach_child (layers, disable_layer, NULL);
}

/*****************************************************************************/

static struct rx_layer *push_layer (struct rx_layer *below,
                                    struct rx_layer *above)
{
	if (above)
		above->lower = below;

	if (below)
		below->upper = above;

	return above;
}

static void free_all_layers (GtRxStack *stack)
{
	struct rx_layer *layers;

	if (!stack)
		return;

	layers = stack->layers;

	if (!layers)
		return;

	/* make sure we've stopped all processing on this layer */
	disable_all (stack);

	/* call each layer's destroy method */
	foreach_child (layers, destroy_foreach, NULL);
}

static struct rx_layer *alloc_layers (GtRxStack *stack, TCPC *c,
                                      BOOL rx_inflated)
{
	struct rx_layer  *layer     = NULL;
	struct rx_layer  *new_layer = NULL;
	void             *udata     = NULL;
	int               i;

	for (i = 0; i < sizeof(layers) / sizeof(layers[0]); i++)
	{
		/* XXX */
		if (!strcmp (layers[i].name, "rx_link"))
			udata = c;

		if (!strcmp (layers[i].name, "rx_inflate") && !rx_inflated)
			continue;

		if (!(new_layer = gt_rx_layer_new (stack, layers[i].name,
		                                layers[i].ops, udata)))
		{
			foreach_child (layer, destroy_foreach, NULL);
			return NULL;
		}

		layer = push_layer (layer, new_layer);
		udata = NULL;
	}

	return layer;
}

/*****************************************************************************/

static void enable_layer (struct rx_layer *rx, void *udata)
{
	gt_rx_layer_enable (rx);
}

GtRxStack *gt_rx_stack_new (GtNode *node, TCPC *c, BOOL rx_inflated)
{
	GtRxStack *stack;
	int        size;

	if (!(stack = NEW (GtRxStack)))
		return NULL;

	stack->c        = c;
	stack->inflated = rx_inflated;

	if (!(stack->layers = alloc_layers (stack, c, rx_inflated)))
	{
		free (stack);
		return NULL;
	}

	/* set the receive buf to a not large value */
	size = 4096;

	if (setsockopt (c->fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size)) != 0)
		GT->DBGSOCK (GT, c, "Error setting rcvbuf size: %s", GIFT_NETERROR());

	/* enable each layer */
	foreach_child (stack->layers, enable_layer, NULL);

	return stack;
}

/*****************************************************************************/

/* 
 * Cleanup handling. This is a bit tricky, because both the owner of the
 * stack and the stack itself may want to free the stack, and they both have
 * to coordinate so only one of them does the free(). 
 * 
 * Also, the stack has to worry about the free happening whilst a message
 * emission is taking place, and so has to unwind the stack to the top level
 * to make sure it doesn't reference freed memory.
 */

static void free_stack (GtRxStack *stack)
{
	/*
	 * ->cleanup gets called from the lower layers, and only
	 * if something bad happened (socket close, etc).
	 */
	free_all_layers (stack);
	FREE (stack);
}

void gt_rx_stack_free (GtRxStack *stack)
{
	if (!stack)
		return;

	/*
	 * If we in the middle of a reception when someone calls this function
	 * [say by calling gt_node_disconenct()], we can't free right now
	 * because the data structures are still in use in the reception stack.
	 *
	 * So we queue the removal in that case, by setting free_delayed and
	 * freeing when the stack unwinds, just like when waiting to notify the
	 * stack's listener about gt_rx_stack_abort().
	 */
	if (stack->depth > 0)
	{
		/* we'll defer the real free until later */
		stack->free_delayed = TRUE;

		/* we must stop processing */
		gt_rx_stack_abort (stack);

		return;
	}

	free_stack (stack);
}

/* notify the user of the stack it's time to clean up this stack */
static void cleanup_notify (GtRxStack *stack)
{
	/*
	 * First, we check if our owner tried to call ->free on us already.
	 * If so, we don't notify them because they already are well aware we are
	 * on our way to oblivion.
	 */
	if (stack->free_delayed)
	{
		free_stack (stack);
		return;
	}

	if (stack->aborted)
		stack->cleanup (stack->udata);
}

void gt_rx_stack_recv_start (GtRxStack *stack)
{
	assert (stack->depth >= 0);
	stack->depth++;
}

void gt_rx_stack_recv_end (GtRxStack *stack)
{
	assert (stack->depth > 0);

	if (--stack->depth == 0)
		cleanup_notify (stack);
}

/*
 * RX layers call this function when something bad happens and they need
 * to abort the stack processing.
 *
 * In other words, this is the "oh shit" function.
 */
void gt_rx_stack_abort (GtRxStack *stack)
{
	disable_all (stack);

	/* set the flag indicated this stack has been aborted while processing */
	stack->aborted = TRUE;

	/*
	 * If we are in the middle of receiving some data, set stack->aborted
	 * so when the reception unwinds, we'll notify the owner of the
	 * stack's cleanup function.
	 */
	if (stack->depth > 0)
		return;

	/* 
	 * This can happen from the bottom layer, if it hasn't passed any data to
	 * upper layers yet.  TODO: if the bottom layer was driven by the RX stack
	 * instead of rx_link, this wouldn't need to be here, i think..
	 */
	cleanup_notify (stack);
}

void gt_rx_stack_set_handler (GtRxStack *stack, GtRxStackHandler handler,
                              GtRxStackCleanup cleanup, void *udata)
{
	stack->udata   = udata;
	stack->handler = handler;
	stack->cleanup = cleanup;

	/*
	 * The topmost layer is rx_packet, so we can simply set the data.
	 */
	gt_rx_packet_set_handler (stack->layers, handler, udata);
}
