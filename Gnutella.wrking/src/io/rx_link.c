/*
 * $Id: rx_link.c,v 1.7 2004/02/01 08:17:12 hipnod Exp $
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
#include "rx_link.h"

/*****************************************************************************/

struct rx_link
{
	/* TODO when the tx stack is implemented also
	 * right now cleanup is too hard */
#if 0
	struct io_source  *ios;
#endif
	TCPC              *c;
	input_id           id;
};

/*****************************************************************************/

/* Some macros for readability */

#define RX_LINK(rx) \
	((struct rx_link *)((rx)->udata))

#define IO_SOURCE(rx_link) \
	((rx_link)->ios)

/*****************************************************************************/

/* TODO: implement cached allocation */
#define RX_LINK_BUFSIZE      512

/*
 * This implementation is for bootstrapping the code onto
 * the present system.
 *
 * The low-level details of what we are actually reading
 * from should be abstracted away.
 */
static void read_data (int fd, input_id id, struct rx_layer *rx)
{
	struct rx_link   *rx_link = RX_LINK(rx);
	struct io_buf    *io_buf;
	ssize_t           n;

	/* we're processing data, we'd better be enabled */
	assert (rx->enabled);

	if (!(io_buf = io_buf_new (RX_LINK_BUFSIZE)))
	{
		gt_rx_stack_abort (rx->stack);
		return;
	}

	if ((n = tcp_recv (rx_link->c, io_buf_write_ptr (io_buf), 
	                   RX_LINK_BUFSIZE)) <= 0)
	{
		if (IO_DEBUG)
		{
			if (n < 0)
				GT->DBGSOCK (GT, rx_link->c, "recv error: %s", GIFT_NETERROR());
			else
				GT->DBGSOCK (GT, rx_link->c, "recv error: socket closed");
		}

		io_buf_free (io_buf);
		gt_rx_stack_abort (rx->stack);
		return;
	}

	/* set the data as having been read */
	io_buf_push (io_buf, n);

	/*
	 * Pass the data up the stack.
	 */
	gt_rx_layer_recv (rx, io_buf);
}

#if 0
/*
 * Receive data from the I/O source, and pass it up the stack
 */
static void recv_data (struct io_source *ios, struct io_buf *io_buf)
{
	struct rx_layer   *rx;
	struct rx_link    *rx_link;
	ssize_t            n;

    rx      = ios->recv_data;
	rx_link = RX_LINK(rx);

	/*
	 * Pass the data to an upper layer.
	 */
	gt_rx_layer_recv (rx, io_buf);

	/*
	 * ?? we have to free io_buf here ?? No.
	 * gtk-gnutella passes off responsibility to the upper layers,
	 * but why..
	 */
#if 0
	return n;
#endif
	/*
	 * Think i may understand why gtk-gnutella does it that way now:
	 * in the partial packet case there may be unread data on the packet,
	 * so we have to store that partial packet data in the intermediate
	 * layers.
	 *
	 * I still wonder if its possible to use a static buffer at each layer
	 * though...
	 */
}
#endif

static void init_input (struct rx_layer *rx, struct rx_link *rx_link)
{
	assert (rx_link->id == 0);
	rx_link->id = input_add (rx_link->c->fd, rx, INPUT_READ,
	                         (InputCallback)read_data, 0);
}

static void free_input (struct rx_layer *rx, struct rx_link *rx_link)
{
	/*
	 * This could be called multiple times on cleanup,
	 * so we don't assert the id is 0 here.
	 */
	if (rx_link->id)
	{
		input_remove (rx_link->id);
		rx_link->id = 0;
	}
}

static void rx_link_enable (struct rx_layer *rx)
{
	struct rx_link *rx_link = RX_LINK(rx);

#if 0
	/* set the callback for getting data */
	io_source_enable (IO_SOURCE(rx_link), IO_SOURCE_OP_RECV, recv_data, rx);
#endif

	init_input (rx, rx_link);
}

static void rx_link_disable (struct rx_layer *rx)
{
	struct rx_link *rx_link = RX_LINK(rx);

#if 0
	io_source_disable (IO_SOURCE(rx_link), IO_SOURCE_OP_RECV);
#endif
	free_input (rx, rx_link);
}

static BOOL rx_link_init (struct rx_layer *rx, void *udata)
{
	struct rx_link *rx_link;
	TCPC           *c = (TCPC *) udata;  /* ewwww */

	if (!(rx_link = NEW (struct rx_link)))
		return FALSE;

	/* store the connection which we get from the upper layer...gross */
	rx_link->c = c;

	/* store our data in the rx structure */
	rx->udata = rx_link;

	return TRUE;
}

static void rx_link_destroy (struct rx_layer *rx)
{
	struct rx_link *rx_link = RX_LINK(rx);

	/*
	 * rx_link_disable() should be called first
	 */
	assert (rx_link->id == 0);

	/*
	 * We would free the connection here, but its shared with
	 * a GtNode that frees it also at the moment.
	 */
#if 0
	tcp_close (rx->c);
#endif

#if 0
	io_source_free (rx_link->ios);
#endif

	FREE (rx_link);
}

/*****************************************************************************/

struct rx_layer_ops gt_rx_link_ops =
{
	rx_link_init,
	rx_link_destroy,
	rx_link_enable,
	rx_link_disable,
	NULL,           /* rx_link_recv */
};
