/*
 * ft_stream.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __STREAM_H
#define __STREAM_H

/*****************************************************************************/

#ifdef USE_ZLIB
# include <zlib.h>
#endif

/*****************************************************************************/

#define FT_STREAM_HEADER 6             /* id(4) + flags(2) */

typedef enum
{
	FT_STREAM_RECV,
	FT_STREAM_SEND
} FTStreamDirection;

typedef enum
{
	FT_STREAM_NONE   = 0x00,
	FT_STREAM_FINISH = 0x01,           /* EOF */
	FT_STREAM_BLOCK  = 0x02,           /* multiple commands in one */
	FT_STREAM_ZLIB   = 0x04,           /* zlib compression */
} FTStreamFlags;

typedef struct
{
	Connection        *c;              /* OpenFT socket that owns this stream */
	ft_uint16          cmd;            /* OpenFT command contained in this
	                                    * stream */
	FTStreamDirection  dir;            /* stream data direction */
	FTStreamFlags      flags;          /* stream flags (compression info) */
	ft_uint32          id;             /* stream identifier */

	unsigned char      eof;            /* indicating that the last stream data
	                                    * has been read and processed, this
	                                    * stream should be cleaned up as soon
	                                    * as its no longer required */

	unsigned int       pkts;           /* total OpenFT packets recv/sent */
	unsigned int       spkts;          /* total stream packets recv/sent */

	/* holds temporary [un]compressed data */
	unsigned char      out_buf[FT_PACKET_MAX - FT_STREAM_HEADER];
	unsigned char      in_buf[FT_PACKET_MAX - FT_STREAM_HEADER];
	size_t             in_rem;

#ifdef USE_ZLIB
	z_stream           s;
#endif /* USE_ZlIB */
} FTStream;

/* see ft_stream_recv */
typedef void (*FTStreamRecv) (FTStream *stream, FTPacket *packet,
                              void *udata);

/*****************************************************************************/

FTStream *ft_stream_get    (Connection *c, FTStreamDirection dir,
                            FTPacket *packet);
void      ft_stream_finish (FTStream *stream);
void      ft_stream_free   (FTStream *stream);

/*****************************************************************************/

int       ft_stream_send (FTStream *stream, FTPacket *packet);
int       ft_stream_recv (FTStream *stream, FTPacket *packet,
                          FTStreamRecv func, void *udata);

/*****************************************************************************/

#endif /* __STREAM_H */
