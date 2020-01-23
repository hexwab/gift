/*
 * $Id: ft_stream.h,v 1.9 2003/05/29 09:37:42 jasta Exp $
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

#ifndef __FT_STREAM_H
#define __FT_STREAM_H

/*****************************************************************************/

/**
 * @file ft_stream.h
 *
 * @brief Grouped packet streams within a previously established OpenFT
 *        session.
 *
 * OpenFT streams effectively run as channels over the original OpenFT
 * specification that allows for seamless compression, encryption, and more
 * logical packet management of a particular action.  Currently, the are only
 * utilized for transparent ZLib compression only, but extensions can be made
 * very easily.
 */

/*****************************************************************************/

#ifdef USE_ZLIB
# include <zlib.h>
#endif

/*****************************************************************************/

/**
 * Stream packets use a special message body and command flag for negotiated
 * the data within.  This represents the size of that message body.  It
 * consists of a stream id (4 bytes) and the streams flags (2 bytes).
 */
#define FT_STREAM_HEADER 6

/**
 * Describes which direction the stream affects.  Streams cannot be
 * bidirectional due to implementation and design flaws.  This is not likely
 * to change because it's not at all useful to fix.
 */
typedef enum
{
	FT_STREAM_RECV,
	FT_STREAM_SEND
} FTStreamDirection;

/**
 * Special flags within the stream packet that describes how to manage the
 * data being delivered.
 */
typedef enum
{
	FT_STREAM_NONE   = 0x00,           /**< Nothing special */
	FT_STREAM_FINISH = 0x01,           /**< End-Of-Stream */
	FT_STREAM_BLOCK  = 0x02,           /**< Multiple commands in one.  Not
	                                    *   currently used. */
	FT_STREAM_ZLIB   = 0x04,           /**< ZLib compression.  Implies
	                                    *   FT_STREAM_BLOCK. */
} FTStreamFlags;

/**
 * Bidirectional stream structure used for both input and output to an
 * established session.
 */
typedef struct
{
	/**
	 * @name protected
	 */
	TCPC              *c;              /**< OpenFT socket that owns this
	                                    *   stream */
	uint16_t           cmd;            /**< OpenFT command contained in this
	                                    *   stream.  Currently, only one
	                                    *   type of command can be sent per
	                                    *   stream instance. */
	FTStreamDirection  dir;            /**< Stream data direction */
	FTStreamFlags      flags;          /**< Stream flags (compression info) */
	uint32_t           id;             /**< Stream id (channel number) */

	unsigned char      eof;            /**< Indicating that the last of the
	                                    *   stream data has been read and
	                                    *   processed.  This stream should be
	                                    *   cleaned up as soon as its no
	                                    *   longer required. */

	unsigned int       pkts;           /**< Total OpenFT packets recv/sent */
	unsigned int       spkts;          /**< Total Stream packets recv/sent */

	/**
	 * @name private
	 */

	/**
	 * Holds temporary compressed (or uncompressed) data as it's being
	 * processed.
	 */
	unsigned char      out_buf[FT_PACKET_MAX - FT_STREAM_HEADER];
	unsigned char      in_buf[FT_PACKET_MAX - FT_STREAM_HEADER];
	size_t             in_rem;

#ifdef USE_ZLIB
	z_stream           s;              /**< Actually compression element */
#endif /* USE_ZlIB */
} FTStream;

/**
 * See ::ft_stream_recv.
 */
typedef void (*FTStreamRecv) (FTStream *stream, FTPacket *packet,
                              void *udata);

/*****************************************************************************/

/**
 * Construct a new stream to operate on the supplied parameters.
 *
 * @param c       Connection to associate with this stream.
 * @param dir     Data flow direction this stream should worry about.
 * @param packet  Optional packet to construct the stream from.  This should
 *                be a valid stream packet (with the necessary OpenFT command
 *                flags).  If you do not specify this, you will always
 *                construct a brand new stream.
 *
 * @return Dynamically allocated stream object or NULL on failure.
 */
FTStream *ft_stream_get (TCPC *c, FTStreamDirection dir,
                         FTPacket *packet);

/**
 * Partial wrapper around ::ft_stream_free that also handles flushing any
 * remaining output to the socket.  You should generall use this even when
 * not writing.
 */
void ft_stream_finish (FTStream *stream);

/**
 * Destroy a constructed stream element.  Lower level alternative to
 * ::ft_stream_finish.
 */
void ft_stream_free (FTStream *stream);

/*****************************************************************************/

/**
 * Send a packet encapsulated in a stream.
 *
 * @param stream
 * @param packet
 *
 * @return Boolean success or failure.  Number of bytes written is not
 *         returned due to the nature of this function.  That is, data is
 *         never really sent, it's always buffered and/or queued at some
 *         other layer.
 */
BOOL ft_stream_send (FTStream *stream, FTPacket *packet);

/**
 * Extract all complete packets contained within the stream-oriented packet
 * described by the calling argument.
 *
 * @param stream
 * @param packet
 * @param func    Callback to use when a complete packet has been read.
 * @param udata   Arbitrary udata to pass to \em func.
 *
 * @return Number of OpenFT packets extracted.
 */
int ft_stream_recv (FTStream *stream, FTPacket *packet,
                    FTStreamRecv func, void *udata);

/*****************************************************************************/

/**
 * Clear all streams associated with a particular direction.  This is useful
 * when attempting to shutdown an active OpenFT session/connection.
 *
 * @param c
 * @param dir  Direction that should be cleared.  You must specify one of
 *             FT_STREAM_RECV or FT_STREAM_SEND.
 *
 * @return Number of streams destroyed.
 */
int ft_stream_clear (TCPC *c, FTStreamDirection dir);

/**
 * Simple wrapper around ::ft_stream_clear that simply calls it on all
 * directions.
 *
 * @return Total number of nodes cleared, that is, the sum of both directions.
 */
int ft_stream_clear_all (TCPC *c);

/*****************************************************************************/

#endif /* __FT_STREAM_H */
