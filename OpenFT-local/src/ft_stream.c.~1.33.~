/*
 * $Id: ft_stream.c,v 1.33 2004/04/25 23:36:02 jasta Exp $
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

#include "ft_openft.h"

#include "ft_packet.h"
#include "ft_stream.h"

#ifdef USE_ZLIB
#include <zlib.h>
#endif

/*****************************************************************************/

static void stream_write (FTStream *stream, unsigned char *buf, size_t len);

/*****************************************************************************/

/* sigh, i do this far too often */
static uint32_t id_cnt = 0;

/* create a unique session id for this connection
 * NOTE: this isn't a unique id, but it should be */
static uint32_t stream_id (TCPC *c)
{
	FTSession *s;

	if (id_cnt == 0)
		id_cnt++;

	if (!(s = FT_SESSION(c)))
		return 0;

	while (dataset_lookup (s->streams_recv, &id_cnt, sizeof (id_cnt)) ||
	       dataset_lookup (s->streams_send, &id_cnt, sizeof (id_cnt)))
	{
		id_cnt++;
	}

	return id_cnt;
}

/*****************************************************************************/

static int stream_init (FTStream *stream, ft_stream_dir_t dir)
{
#ifdef USE_ZLIB
	memset (&stream->s, 0, sizeof (stream->s));

	switch (dir)
	{
	 case FT_STREAM_SEND:
		{
			if (deflateInit (&stream->s, Z_DEFAULT_COMPRESSION) != Z_OK)
				return FALSE;

			memset (stream->out_buf, 0, sizeof (stream->out_buf));
			stream->s.next_out  = stream->out_buf;
			stream->s.avail_out = sizeof (stream->out_buf);
		}
		break;
	 case FT_STREAM_RECV:
		{
			if (inflateInit (&stream->s) != Z_OK)
				return FALSE;

			memset (stream->in_buf, 0, sizeof (stream->in_buf));
			stream->s.next_in  = NULL;
			stream->s.avail_in = 0;
		}
		break;
	}
#endif /* USE_ZLIB */

	return TRUE;
}

static FTStream *stream_new (TCPC *c, ft_stream_dir_t dir,
                             uint32_t id, ft_stream_flags_t flags,
                             uint16_t cmd)
{
	FTStream *stream;

	if (id == 0)
		return NULL;

	if (!(stream = MALLOC (sizeof (FTStream))))
		return NULL;

	stream->c     = c;
	stream->cmd   = cmd;
	stream->dir   = dir;
	stream->id    = id;
	stream->flags = flags;

	if (!stream_init (stream, dir))
	{
		free (stream);
		return NULL;
	}

	return stream;
}

static void stream_free (FTStream *stream)
{
#ifdef USE_ZLIB
	switch (stream->dir)
	{
	 case FT_STREAM_SEND:
		deflateEnd (&stream->s);
		break;
	 case FT_STREAM_RECV:
		inflateEnd (&stream->s);
		break;
	}
#endif /* USE_ZLIB */

	free (stream);
}

/*****************************************************************************/

static Dataset **get_direction (TCPC *c, ft_stream_dir_t dir)
{
	Dataset **d = NULL;

	switch (dir)
	{
	 case FT_STREAM_RECV:
		d = &(FT_SESSION(c)->streams_recv);
		break;
	 case FT_STREAM_SEND:
		d = &(FT_SESSION(c)->streams_send);
		break;
	}

	return d;
}

#if 0
static char *get_direction_str (FTStream *stream)
{
	char *str = NULL;

	switch (stream->dir)
	{
	 case FT_STREAM_RECV:
		str = "RECV";
		break;
	 case FT_STREAM_SEND:
		str = "SEND";
		break;
	}

	return str;
}
#endif

static FTStream *lookup_stream (TCPC *c, ft_stream_dir_t dir,
                                 uint32_t id)
{
	Dataset **d;

	if (!c || id == 0)
		return NULL;

	if (!(d = get_direction (c, dir)))
		return NULL;

	return dataset_lookup (*d, &id, sizeof (id));
}

static int insert_stream (TCPC *c, ft_stream_dir_t dir, uint32_t id,
                          FTStream *stream)
{
	Dataset **d;

	if (!c || !stream || id == 0)
		return FALSE;

	if (!(d = get_direction (c, dir)))
		return FALSE;

	dataset_insert (d, &id, sizeof (id), stream, 0);
	return TRUE;
}

static int remove_stream (FTStream *stream)
{
	Dataset **d;

	if (!stream)
		return FALSE;

	if (!(d = get_direction (stream->c, stream->dir)))
		return FALSE;

	dataset_remove (*d, &(stream->id), sizeof (stream->id));
	return TRUE;
}

FTStream *ft_stream_get (TCPC *c, ft_stream_dir_t dir, FTPacket *packet)
{
	FTStream         *stream;
	ft_stream_flags_t flags = FT_STREAM_NONE;
	uint32_t          id;

	if (!packet)
	{
		id = stream_id (c);

#ifdef USE_ZLIB
		if (dataset_lookup (FT_SESSION(c)->cap, "ZLIB", 5))
		{
			flags |= FT_STREAM_BLOCK;
			flags |= FT_STREAM_ZLIB;
		}
#endif /* USE_ZLIB */
	}
	else
	{
		id    = ft_packet_get_uint32 (packet, TRUE);
		flags = ft_packet_get_uint16 (packet, TRUE);

#ifndef USE_ZLIB
		if (flags & FT_STREAM_ZLIB)
		{
			FT->DBGSOCK (FT, c, "unable to negotiate zlib compression");
			return NULL;
		}
#endif /* !USE_ZLIB */

		/* we already have a stream by this id, return it */
		if ((stream = lookup_stream (c, dir, id)))
		{
			stream->flags |= flags;
			return stream;
		}
	}

	if (!(stream = stream_new (c, dir, id, flags, ft_packet_command (packet))))
		return NULL;

#if 0
	FT->DBGFN (FT, "%s: %lu(%s): initialized",
			   net_ip_str (FT_NODE(c)->ip), (unsigned long)id,
			   get_direction_str (stream));
#endif

	if (!insert_stream (c, dir, id, stream))
	{
		free (stream);
		return NULL;
	}

	return stream;
}

#ifdef USE_ZLIB
static void output_flush (FTStream *stream)
{
	int          err;
	unsigned int len;
	int          done = FALSE;

	/* do not flush output if we had absolutely nothing to say */
	if (stream->spkts == 0)
	{
		assert (stream->s.total_in == 0);
		return;
	}

	assert (stream->s.avail_in == 0);

	for (;;)
	{
		if (stream->s.avail_out)
		{
			err = deflate (&stream->s, Z_FINISH);

			if ((sizeof (stream->out_buf) - stream->s.avail_out) == 0 &&
			    err == Z_BUF_ERROR)
			{
				err = Z_OK;
			}

			done = stream->s.avail_out != 0 || err == Z_STREAM_END;

			if (err != Z_OK && err != Z_STREAM_END)
				break;
		}

		if ((len = sizeof (stream->out_buf) - stream->s.avail_out) != 0)
		{
			stream_write (stream, stream->out_buf, len);
			stream->s.next_out  = stream->out_buf;
			stream->s.avail_out = sizeof (stream->out_buf);
		}

		if (done)
			break;
	}

	/* write the eof packet */
	stream_write (stream, NULL, 0);
}
#endif /* USE_ZLIB */

void ft_stream_finish (FTStream *stream)
{
	if (!stream)
		return;

#ifdef USE_ZLIB
	if (stream->dir == FT_STREAM_SEND)
		output_flush (stream);

#if 0
	/* it's really not very useful to print streams that were never actually
	 * written to or read from */
	if (stream->s.total_in || stream->s.total_out)
	{
		FT->DBGFN (FT, "%s: %lu(%s): %u/%u: in=%u, out=%u",
		           net_ip_str (FT_NODE(stream->c)->ip),
		           (unsigned long)stream->id,
		           get_direction_str (stream),
		           stream->pkts, stream->spkts,
		           stream->s.total_in, stream->s.total_out);
	}
#endif
#endif /* USE_ZLIB */

	remove_stream (stream);
	stream_free (stream);
}

void ft_stream_free (FTStream *stream)
{
	stream_free (stream);
}

/*****************************************************************************/

static void stream_write (FTStream *stream, unsigned char *buf, size_t len)
{
	stream->pkts++;

	/* send eof */
	if (!buf)
	{
		ft_packet_sendva (stream->c, stream->cmd, FT_PACKET_STREAM,
		                  "lh", stream->id,
		                  (uint16_t)(stream->flags | FT_STREAM_FINISH));
		return;
	}

	ft_packet_sendva (stream->c, stream->cmd, FT_PACKET_STREAM,
	                  "lhS", stream->id, (uint16_t)stream->flags,
	                  buf, len);
}

#ifdef USE_ZLIB
static void zlib_deflate (FTStream *stream, unsigned char *p, size_t len)
{
	z_stream *s = &stream->s;

	s->next_in  = (Bytef *) p;
	s->avail_in = (unsigned int) len;

	while (s->avail_in != 0)
	{
		if (s->avail_out == 0)
		{
			stream_write (stream, stream->out_buf, sizeof (stream->out_buf));
			s->next_out  = (Bytef *) stream->out_buf;
			s->avail_out = (unsigned int) sizeof (stream->out_buf);
		}

		assert (deflate (s, Z_NO_FLUSH) == Z_OK);
	}
}
#endif /* USE_ZLIB */

static void stream_deflate (FTStream *stream, unsigned char *p, size_t len)
{
#ifdef USE_ZLIB
	if (stream->flags & FT_STREAM_ZLIB)
		zlib_deflate (stream, p, len);
	else
#endif /* USE_ZLIB */
		stream_write (stream, p, len);
}

/*
 * Append the given packet to the stream, this may or may not flush.  The
 * supplied packet will be cleaned up as if ft_packet_send was used.
 */
int ft_stream_send (FTStream *stream, FTPacket *packet)
{
	unsigned char *pdata;
	size_t         pdatalen = 0;
	uint16_t       plen;

	if (!stream || !packet)
		return -1;

	assert (stream->dir == FT_STREAM_SEND);

	if (stream->cmd == 0)
		stream->cmd = ft_packet_command (packet);

	/* for now we don't support this */
	assert (ft_packet_command (packet) == stream->cmd);

	if ((pdata = ft_packet_serialize (packet, &pdatalen)))
	{
		stream_deflate (stream, pdata, pdatalen);
		stream->spkts++;
	}

	plen = ft_packet_length (packet);
	ft_packet_free (packet);

	return plen;
}

/*****************************************************************************/

#ifdef USE_ZLIB
static int do_work_inflate (z_stream *s)
{
	int err;

	/*
	 * Consume input until we exhaust all input, or run out of room in
	 * the output buffer, whichever comes first.
	 */
	while ((err = inflate (s, Z_NO_FLUSH)) != Z_STREAM_END)
	{
		if (err != Z_OK)
			break;

		if (s->avail_in == 0 || s->avail_out == 0)
			break;
	}

	return err;
}

static size_t consume_packets (FTStream *stream,
                               FTStreamRecv recvfn, void *udata)
{
	FTPacket      *pkt;
	unsigned char *ptr;
	size_t         npkts = 0;
	size_t         consumed;

	ptr = stream->in_buf;

	/*
	 * Attempt to parse as many packets from the uncompressed stream as
	 * possible.
	 */
	while (1)
	{
		if (!(pkt = ft_packet_unserialize (ptr, stream->s.next_out - ptr)))
			break;

		/* allow the caller to handle this individual packet */
		recvfn (stream, pkt, udata);

		npkts++;
		stream->spkts++;
		ptr += FT_PACKET_HEADER;
		ptr += ft_packet_length (pkt);

		ft_packet_free (pkt);
	}

	stream->in_rem = stream->s.next_out - ptr;
	consumed = ptr - stream->in_buf;

	/*
	 * ...Then reposition the uncompressed buffer such that all fully
	 * processed packets are consumed and no longer occupy space.
	 */
	if (consumed > 0)
	{
		if (stream->in_rem > 0)
			memmove (stream->in_buf, ptr, stream->in_rem);

		stream->s.avail_out += consumed;
		stream->s.next_out -= consumed;
	}

	return npkts;
}

static int stream_decompress (FTStream *stream,
                              const unsigned char *buf, size_t len,
                              FTStreamRecv recvfn, void *udata)
{
	int    err;
	size_t npkts = 0;

	assert (stream->flags & FT_STREAM_ZLIB);

	stream->s.next_in = (Bytef *)buf;
	stream->s.avail_in = len;

	/* honor stream->in_rem so that we can make sure to leave any left-over
	 * uncompressed data from a previous stream_decompress() call */
	stream->s.next_out = stream->in_buf + stream->in_rem;
	stream->s.avail_out = sizeof (stream->in_buf) - stream->in_rem;

	while (1)
	{
		if ((err = do_work_inflate (&stream->s)) < 0)
		{
			FT->DBGFN (FT, "zlib err=%d", err);
			break;
		}

		npkts += consume_packets (stream, recvfn, udata);

		/*
		 * When err == Z_STREAM_END, stream->s.avail_in must be 0, but
		 * the inverse case is not always true.
		 */
		if (err == Z_STREAM_END || stream->s.avail_in == 0)
			break;
	}

	return ((int)npkts);
}
#endif /* USE_ZLIB */

static int stream_copy (FTStream *stream,
                        const unsigned char *buf, size_t len,
                        FTStreamRecv recvfn, void *udata)
{
	FTPacket *pkt;
	size_t    npkts = 0;
	size_t    pktsize;

	assert (!(stream->flags & FT_STREAM_ZLIB));

	/*
	 * Slightly altered version of the consume_packets() logic above.  Code
	 * duplication is necessary in lieu of some more generalized
	 * stream-processing routines.
	 */
	while (len > 0)
	{
		if (!(pkt = ft_packet_unserialize (buf, len)))
			break;

		recvfn (stream, pkt, udata);

		npkts++;
		stream->spkts++;

		pktsize  = FT_PACKET_HEADER;
		pktsize += ft_packet_length (pkt);

		ft_packet_free (pkt);

		buf += pktsize;
		len -= pktsize;
	}

	return npkts;
}

static int stream_recv (FTStream *stream, const unsigned char *buf, size_t len,
                        FTStreamRecv recvfn, void *udata)
{
	int ret;

#ifdef USE_ZLIB
	if (stream->flags & FT_STREAM_ZLIB)
		ret = stream_decompress (stream, buf, len, recvfn, udata);
	else
#endif /* USE_ZLIB */
		ret = stream_copy (stream, buf, len, recvfn, udata);

	return ret;
}

int ft_stream_recv (FTStream *stream, FTPacket *stream_pkt,
                    FTStreamRecv func, void *udata)
{
	unsigned char *data;
	size_t         len;

	if (!stream || !stream_pkt || !func)
		return -1;

	assert (stream->dir == FT_STREAM_RECV);

	if (stream->flags & FT_STREAM_FINISH)
	{
		stream->eof = TRUE;
		return 0;
	}

#ifndef USE_ZLIB
	if (stream->flags & FT_STREAM_ZLIB)
	{
		FT->DBGFN (FT, "unsupported stream flag: FT_STREAM_ZLIB");
		stream->eof = TRUE;

		return -1;
	}
#endif /* !USE_ZLIB */

	assert (ft_packet_flags (stream_pkt) & FT_PACKET_STREAM);
	assert (stream_pkt->offset > 0);

	data = stream_pkt->data + FT_PACKET_HEADER + stream_pkt->offset;
	len = ft_packet_length (stream_pkt) - stream_pkt->offset;

	/* statistics purposes only */
	stream->pkts++;

	return stream_recv (stream, data, len, func, udata);
}

/*****************************************************************************/

static int clear_stream (ds_data_t *key, ds_data_t *value, int *cnt)
{
	ft_stream_free (value->data);
	(*cnt)++;

	return DS_CONTINUE | DS_REMOVE;
}

int ft_stream_clear (TCPC *c, ft_stream_dir_t dir)
{
	Dataset **d;
	int       cnt = 0;

	if (!(d = get_direction (c, dir)))
		return 0;

	dataset_foreach_ex (*d, DS_FOREACH_EX(clear_stream), &cnt);
	dataset_clear (*d);
	*d = NULL;

	return cnt;
}

int ft_stream_clear_all (TCPC *c)
{
	int cnt = 0;

	cnt += ft_stream_clear (c, FT_STREAM_RECV);
	cnt += ft_stream_clear (c, FT_STREAM_SEND);

	return cnt;
}
