/*
 * $Id: rx_inflate.c,v 1.10 2004/04/05 07:56:54 hipnod Exp $
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

#include <zlib.h>

/*****************************************************************************/

/* this is small for testing purposes */
#define    RX_INFLATE_BUFSIZE   256

#define RX_INFLATE(rx) \
	((struct rx_inflate *) (((rx)->udata)))

struct rx_inflate
{
	z_stream       z;
	BOOL           init_done;
};

/*****************************************************************************/

static BOOL rx_inflate_init (struct rx_layer *rx, void *udata)
{
	struct rx_inflate *rx_inflate;

	if (!(rx_inflate = NEW (struct rx_inflate)))
		return FALSE;

	/* inflateInit() may touch these variables */
	rx_inflate->z.zalloc   = Z_NULL;
	rx_inflate->z.zfree    = Z_NULL;
	rx_inflate->z.opaque   = Z_NULL;
	rx_inflate->z.next_in  = Z_NULL;
	rx_inflate->z.avail_in = 0;

	if (inflateInit (&rx_inflate->z) != Z_OK)
	{
		gt_rx_stack_abort (rx->stack);
		return FALSE;
	}

	rx->udata = rx_inflate;

	rx_inflate->init_done = TRUE;

	return TRUE;
}

static void rx_inflate_destroy (struct rx_layer *rx)
{
	struct rx_inflate *rx_inflate = RX_INFLATE(rx);

	/*
	 * We don't check the error here, there could very well be leftover data
	 * and it would be annoying to print a message every time.
	 */
	inflateEnd (&rx_inflate->z);
	rx_inflate->init_done = FALSE;

	FREE (rx_inflate);
}

static void rx_inflate_enable (struct rx_layer *rx)
{
	/* nothing -- we only process data when it comes towards us */
}

static void rx_inflate_disable (struct rx_layer *rx)
{
	/* nothing */
}

/*
 * Handle the data from the lower layer, decompress it, and pass
 * it to the upper layer.
 */
static struct io_buf *read_buf (struct rx_layer *rx, struct io_buf *io_buf)
{
	struct rx_inflate *rx_inflate   = RX_INFLATE(rx);
	struct io_buf     *out_msg;
	z_streamp          inz;
	int                ret;
	size_t             uncompressed_size;
	size_t             compressed_read;
	size_t             out_size     = RX_INFLATE_BUFSIZE;
	size_t             avail;
	static size_t      running_cnt  = 0;
	static int         msg_count    = 0;

	avail = io_buf_read_avail (io_buf);

	if (avail == 0)
		return NULL;

	if (!(out_msg = io_buf_new (out_size)))
	{
		GT->dbg (GT, "couldn't allocate memory for recv buf");
		gt_rx_stack_abort (rx->stack);
		return NULL;
	}

	assert (rx_inflate->init_done);
	inz = &rx_inflate->z;

	inz->next_in   = io_buf_read_ptr (io_buf);
	inz->avail_in  = avail;
	inz->next_out  = io_buf_write_ptr (out_msg);
	inz->avail_out = out_size;

	ret = inflate (inz, Z_SYNC_FLUSH);

	if (ret != Z_OK)
	{
		if (IO_DEBUG)
			GT->dbg (GT, "zlib recv error: %d", ret);

		gt_rx_stack_abort (rx->stack);
		io_buf_free (out_msg);
		return NULL;
	}

	uncompressed_size  = out_size - inz->avail_out;
	compressed_read    = avail - inz->avail_in;

	running_cnt += uncompressed_size;
	if (IO_DEBUG && ++msg_count % 50 == 0)
	{
		GT->dbg (GT, "uncompressed %u bytes", running_cnt);
		running_cnt = 0;
	}

	/* add the bytes we read to the new messge */
	io_buf_push (out_msg, uncompressed_size);

	/* pop the old bytes we read off the incoming message */
	io_buf_pop (io_buf, compressed_read);

	return out_msg;
}

/*
 * Parse the data into buffers, and send it to the upper layers.  packets to
 */
static void rx_inflate_recv (struct rx_layer *rx, struct io_buf *io_buf)
{
	struct io_buf   *msg;

	while (rx->enabled && (msg = read_buf (rx, io_buf)))
	{
		assert (msg != NULL);
		gt_rx_layer_recv (rx, msg);

		/* 
		 * NOTE: gt_rx_layer_recv() may abort the stack here...  there's not much
		 * we can do about that, but in practice that doesn't happen and we
		 * won't access freed memory because the stack doesn't get freed until
		 * the lowest rx layer is notified of the abort...pretty hacky.
		 */
	}

	/* we have to free the buffer */
	io_buf_free (io_buf);
}

/*****************************************************************************/

struct rx_layer_ops gt_rx_inflate_ops =
{
	rx_inflate_init,
	rx_inflate_destroy,
	rx_inflate_enable,
	rx_inflate_disable,
	rx_inflate_recv,
};
