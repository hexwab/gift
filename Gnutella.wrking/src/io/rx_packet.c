/*
 * $Id: rx_packet.c,v 1.7 2004/02/15 04:53:38 hipnod Exp $
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

#include "rx_stack.h"
#include "rx_layer.h"

#include "gt_packet.h"
#include "rx_packet.h"

/*****************************************************************************/

struct rx_packet
{
	struct io_buf       *partial;
	rx_packet_handler_t  handler;
	void                *udata;
};

/*****************************************************************************/

#define RX_PACKET(rx) \
	((struct rx_packet *) ((rx)->udata))

/*****************************************************************************/

static void null_packet_handler (void *udata, GtPacket *packet)
{
	gt_packet_free (packet);
}

static BOOL rx_packet_init (struct rx_layer *rx, void *udata)
{
	struct rx_packet *rx_packet;

	if (!(rx_packet = NEW (struct rx_packet)))
		return FALSE;

	rx->udata = rx_packet;

	/* discard the packets received */
	rx_packet->handler = null_packet_handler;
	return TRUE;
}

static void rx_packet_destroy (struct rx_layer *rx)
{
	struct rx_packet *rx_packet = RX_PACKET(rx);

	io_buf_free (rx_packet->partial);
	FREE (rx_packet);
}

static void rx_packet_disable (struct rx_layer *rx)
{
	/* NOP: lower layer will stop sending us data -- it'd better
	 * stop now */
}

static void rx_packet_enable (struct rx_layer *rx)
{
	/* NOP: the lower layer will start sending us data again */
}

/*****************************************************************************/

static GtPacket *make_packet (struct rx_layer *rx, struct rx_packet *rx_packet,
                              size_t packet_size)
{
	GtPacket      *packet;
	struct io_buf *pbuf = rx_packet->partial;

	assert (io_buf_len (pbuf) == packet_size);
	assert (packet_size < GT_PACKET_MAX);

	/* construct a complete packet from the data in the binary buf */
	packet = gt_packet_unserialize (pbuf->data, packet_size);

	/*
	 * TODO: gt_packet_unserialize() currently copies the data, but should
	 * really take ownership of the data in place.  That would also allow the
	 * null terminator in the io_buf to terminate the last string in the
	 * packet, if any.
	 */
	io_buf_free (pbuf);
	rx_packet->partial = NULL;

	if (!packet)
	{
		gt_rx_stack_abort (rx->stack);
		return NULL;
	}

	return packet;
}

static BOOL fill_up_to (struct rx_layer *rx, struct io_buf *dst,
                        struct io_buf *src, size_t fill_size)
{
	size_t     new_len;
	size_t     old_len;
	size_t     len_to_read;

	old_len = io_buf_len (dst);
	new_len = io_buf_len (src);

	/* skip if we already have enough */
	if (old_len >= fill_size)
		return TRUE;

	len_to_read = MIN (fill_size - old_len, new_len);

	/* ensure the packet has enough room */
	if (!io_buf_resize (dst, old_len + len_to_read))
	{
		gt_rx_stack_abort (rx->stack);
		return FALSE;
	}

	io_buf_copy (dst, src, len_to_read);

	if (io_buf_len (dst) >= fill_size)
		return TRUE;

	return FALSE;
}

static BOOL fill_header (struct rx_layer *rx, struct rx_packet *rx_packet,
                         struct io_buf *io_buf)
{
	struct io_buf *dst = rx_packet->partial;
	struct io_buf *src = io_buf;

	if (!fill_up_to (rx, dst, src, GNUTELLA_HDR_LEN))
	{
		/* this would fail if there was an alloc failure */
		assert (io_buf_read_avail (io_buf) == 0);
		return FALSE;
	}

	return TRUE;
}

/*
 * We must read all the data the lower layer has sent and buffer
 * partial packets, because otherwise we would poll the CPU if the
 * packet were bigger than the buffer size of one of the layer
 * layers under this one.
 */
static BOOL read_packet (struct rx_layer *rx, struct rx_packet *rx_packet,
                         struct io_buf *io_buf, GtPacket **ret)
{
	uint32_t       payload_len;
	size_t         partial_len;
	uint32_t       packet_size;
	struct io_buf *partial     = rx_packet->partial;
	GtPacket      *pkt;

	*ret = NULL;

	partial_len = io_buf_len (partial);
	assert (partial_len >= GNUTELLA_HDR_LEN);

	/*
	 * The partial packet is now at least 23 bytes.  Look in the header for
	 * the payload length so we know how much we need to read before the
	 * packet is complete.
	 */
	payload_len = get_payload_len (partial->data);
	packet_size = payload_len + GNUTELLA_HDR_LEN;

	/*
	 * Check for wraparound, and reset the packet size to its payload len.
	 * Its likely we've experienced a protocol de-sync here.  Set the size so
	 * the connection will be closed.
	 */
	if (packet_size < GNUTELLA_HDR_LEN)
		packet_size = GT_PACKET_MAX;

	if (packet_size >= GT_PACKET_MAX)
	{
		if (IO_DEBUG)
			GT->dbg (GT, "received too large packet(%d)", packet_size);

		/* TODO: should send a BYE message here */
		gt_rx_stack_abort (rx->stack);
		return FALSE;
	}

	if (!fill_up_to (rx, partial, io_buf, packet_size))
	{
		/* this would fail if there was an alloc failure */
		assert (io_buf_read_avail (io_buf) == 0);
		return FALSE;
	}

	/* yay, read a packet */
	pkt = make_packet (rx, rx_packet, packet_size);
	*ret = pkt;

	return (pkt == NULL ? FALSE : TRUE);
}

/*
 * Receive a message buffer from the lower layer, parse it into as many
 * packets as it contains, and pass those to our handler function.
 *
 * This is meant to be the top layer of the rx stack only.
 *
 * TODO: A handler could be implemented as another layer, should think
 * about this approach instead.
 */
static void rx_packet_recv (struct rx_layer *rx, struct io_buf *io_buf)
{
	GtPacket          *packet = NULL;
	struct rx_packet  *rx_packet;

	rx_packet = RX_PACKET(rx);

	while (rx->enabled && io_buf_read_avail (io_buf) > 0)
	{
		/* allocate a new partial buffer, if one is not present yet */
		if (!rx_packet->partial &&
		    !(rx_packet->partial = io_buf_new (GNUTELLA_HDR_LEN)))
		{
			gt_rx_stack_abort (rx->stack);
			break;
		}

		/* try to read the first 23 bytes */
		if (!fill_header (rx, rx_packet, io_buf))
			break;

		/*
		 * Read the payload. If there arent enough bytes to complete the
		 * packet, we finish it later.
		 */
		if (!read_packet (rx, rx_packet, io_buf, &packet))
		{
			assert (packet == NULL);
			break;
		}

		assert (packet != NULL);
		(*rx_packet->handler) (rx_packet->udata, packet);

		/* freeing the packet here means the callback must make its own
		 * provisions for storing the packet's data */
		gt_packet_free (packet);
		packet = NULL;
	}

	io_buf_free (io_buf);
}

/*****************************************************************************/

struct rx_layer_ops gt_rx_packet_ops =
{
	rx_packet_init,
	rx_packet_destroy,
	rx_packet_enable,
	rx_packet_disable,
	rx_packet_recv,
};

void gt_rx_packet_set_handler (struct rx_layer *rx,
                               rx_packet_handler_t handler,
                               void *udata)
{
	struct rx_packet *rx_packet = RX_PACKET(rx);

	rx_packet->handler = handler;
	rx_packet->udata   = udata;
}
