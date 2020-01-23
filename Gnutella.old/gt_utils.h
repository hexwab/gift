/*
 * $Id: gt_utils.h,v 1.5 2003/06/01 09:37:15 hipnod Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifndef __GT_UTILS_H__
#define __GT_UTILS_H__

/*****************************************************************************/

/* define to set data structures to a predefined value after freeing */
/* #define POISON_FREE	1 */

/*****************************************************************************/

typedef struct gt_zlib_stream
{
	void          *streamptr;

	enum
	{
		ZSTREAM_NONE    = 0x00,
		ZSTREAM_INFLATE = 0x01,
		ZSTREAM_DEFLATE = 0x02,
	} type;

	char *data;
	char *start, *end;
	char *pos;
} ZlibStream;

/*****************************************************************************/

char   *make_str   (char *pseudo_str, int len);

int     peer_addr  (int fd, in_addr_t *r_ip, in_port_t *r_port);

void    print_hex  (char *buf, int len);

/*****************************************************************************/

ZlibStream *zlib_stream_open    (size_t max);
void        zlib_stream_close   (ZlibStream *stream);
int         zlib_stream_inflate (ZlibStream *stream, char *data, size_t size);
int         zlib_stream_write   (ZlibStream *stream, char *data, size_t size);

/* returns the amount of data read */
int         zlib_stream_read    (ZlibStream *stream, char **r_data);

/*****************************************************************************/

#ifdef POISON_FREE
#define poisoned_free(ptr, chr) \
do { \
	memset ((ptr), (chr), sizeof (*(ptr))); \
	free ((ptr)); \
} while (0)
#else /* POISON_FREE */
#define poisoned_free(ptr, chr) \
	free ((ptr))
#endif /* POISON_FREE */

/*****************************************************************************/

#endif /* __GT_UTILS_H__ */
