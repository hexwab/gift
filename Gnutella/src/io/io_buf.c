/*
 * $Id: io_buf.c,v 1.2 2003/09/17 17:44:11 hipnod Exp $
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

#include "io_buf.h"

/*****************************************************************************/

struct io_buf *io_buf_new (size_t size)
{
	struct io_buf *buf;
	uint8_t       *data;

	if (!(data = gift_malloc (size + 1)))
		return NULL;

	if (!(buf = gift_malloc (sizeof (struct io_buf))))
	{
		free (data);
		return NULL;
	}

	buf->data   = data;
	buf->size   = size;
	buf->r_offs = 0;
	buf->w_offs = 0;

	/* null terminate the buffer */
	buf->data[size] = 0;

	return buf;
}

void io_buf_free (struct io_buf *buf)
{
	uint8_t *data;

	if (!buf)
		return;

	data = io_buf_free_keep (buf);
	free (data);
}

uint8_t *io_buf_free_keep (struct io_buf *buf)
{
	uint8_t *data;

	data = buf->data;
	free (buf);

	return data;
}

/*
 * Resize the underlying buffer. This includes +1 for the null terminator.
 */
BOOL io_buf_resize (struct io_buf *buf, size_t len)
{
	uint8_t *resized;

	if (buf->size >= len)
		return TRUE;

	if (!(resized = gift_realloc (buf->data, len + 1)))
		return FALSE;

	buf->data = resized;
	buf->size = len;
	
	/* ensure null-termination */
	buf->data[len] = 0;

	return TRUE;
}

void io_buf_reset (struct io_buf *buf)
{
	buf->w_offs = 0;
	buf->r_offs = 0;
}

void io_buf_push (struct io_buf *buf, size_t len)
{
	assert (len + buf->w_offs <= buf->size);
	buf->w_offs += len;
}

void io_buf_pop (struct io_buf *buf, size_t len)
{
	assert (len + buf->r_offs <= buf->w_offs);
	buf->r_offs += len;
}

size_t io_buf_copy (struct io_buf *dst, struct io_buf *src, size_t len)
{
	size_t src_avail = io_buf_read_avail (src);
	size_t dst_avail = io_buf_write_avail (dst);

	if (len > src_avail)
		len = src_avail;

	if (len > dst_avail)
		len = dst_avail;

	memcpy (dst->data + dst->w_offs, src->data + src->r_offs, len);

	dst->w_offs += len;
	src->r_offs += len;

	return len;
}
