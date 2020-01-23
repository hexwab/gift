/*
 * $Id: io_buf.h,v 1.2 2003/09/17 17:44:11 hipnod Exp $
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

#ifndef GIFT_GT_IO_BUF_H_
#define GIFT_GT_IO_BUF_H_

/*****************************************************************************/

/*
 * This duplicates libgift's String somewhat...
 */
struct io_buf
{
	uint8_t   *data;
	size_t     size;
	size_t     r_offs;
	size_t     w_offs;
};

/*****************************************************************************/

/*
 * Macros for accessing particular things about an I/O buf...
 */

#define io_buf_read_avail(io_buf) \
	((io_buf)->w_offs - (io_buf)->r_offs)

#define io_buf_len(io_buf) \
	((io_buf)->w_offs)

#define io_buf_size(io_buf) \
	((io_buf)->size)

#define io_buf_write_avail(io_buf) \
	((io_buf)->size - (io_buf)->w_offs)

#define io_buf_write_ptr(io_buf) \
    (&(io_buf)->data[(io_buf)->w_offs])

#define io_buf_read_ptr(iobuf) \
	(&(iobuf)->data[(iobuf)->r_offs])

/*****************************************************************************/

struct io_buf  *io_buf_new        (size_t len);
void            io_buf_free       (struct io_buf *buf);
uint8_t        *io_buf_free_keep  (struct io_buf *buf);

BOOL            io_buf_resize     (struct io_buf *buf, size_t len);
void            io_buf_reset      (struct io_buf *buf);

void            io_buf_push       (struct io_buf *buf, size_t len);
void            io_buf_pop        (struct io_buf *buf, size_t len);

size_t          io_buf_copy       (struct io_buf *dst, struct io_buf *src,
                                   size_t len);

/*****************************************************************************/

#endif /* GIFT_GT_IO_BUF_H_ */
