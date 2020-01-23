/*
 * ft_stream.c
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
static ft_uint32 id_cnt = 0;

/* create a unique session id for this connection
 * NOTE: this isn't a unique id, but it should be */
static ft_uint32 stream_id (Connection *c)
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

static int stream_init (FTStream *stream, FTStreamDirection dir)
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

static FTStream *stream_new (Connection *c, FTStreamDirection dir,
                             ft_uint32 id, FTStreamFlags flags,
                             ft_uint16 cmd)
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

static Dataset **get_direction (Connection *c, FTStreamDirection dir)
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

static FTStream *lookup_stream (Connection *c, FTStreamDirection dir,
                                 ft_uint32 id)
{
	Dataset **d;

	if (!c || id == 0)
		return NULL;

	if (!(d = get_direction (c, dir)))
		return NULL;

	return dataset_lookup (*d, &id, sizeof (id));
}

static int insert_stream (Connection *c, FTStreamDirection dir, ft_uint32 id,
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

FTStream *ft_stream_get (Connection *c, FTStreamDirection dir, FTPacket *packet)
{
	FTStream     *stream;
	FTStreamFlags flags = FT_STREAM_NONE;
	ft_uint32     id;

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
			TRACE_SOCK (("unable to negotiate zlib compression"));
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

	TRACE (("%s: %lu(%s): initialized",
	        net_ip_str (FT_NODE(c)->ip), (unsigned long)id,
	        get_direction_str (stream)));

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

	TRACE (("%s: %lu(%s): %u/%u: in=%u, out=%u",
	        net_ip_str (FT_NODE(stream->c)->ip), (unsigned long)stream->id,
			get_direction_str (stream),
			stream->pkts, stream->spkts,
	        stream->s.total_in, stream->s.total_out));
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
		                  (ft_uint16)(stream->flags | FT_STREAM_FINISH));
		return;
	}

	ft_packet_sendva (stream->c, stream->cmd, FT_PACKET_STREAM,
	                  "lhS", stream->id, (ft_uint16)stream->flags,
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
 *
 * TODO:
 * Handle errors
 */
int ft_stream_send (FTStream *stream, FTPacket *packet)
{
	unsigned char *pdata;
	size_t         plen = 0;

	if (!stream || !packet)
		return FALSE;

	assert (stream->dir == FT_STREAM_SEND);

	if (stream->cmd == 0)
		stream->cmd = ft_packet_command (packet);

	/* for now we don't support this */
	assert (ft_packet_command (packet) == stream->cmd);

	if ((pdata = ft_packet_serialize (packet, &plen)))
	{
		stream_deflate (stream, pdata, plen);
		stream->spkts++;
	}

	ft_packet_free (packet);

	return TRUE;
}

/*****************************************************************************/

#ifdef USE_ZLIB
static int zlib_recv (z_stream *s, unsigned char *buf, size_t len)
{
	int err;

	if (!buf || len == 0)
		return -1;

	if (s->avail_in <= 0)
		return -1;

	s->next_out  = buf;
	s->avail_out = len;

	while (s->avail_out != 0)
	{
		if (s->avail_in == 0)
			break;

		if ((err = inflate (s, Z_NO_FLUSH)) != Z_OK &&
			err != Z_STREAM_END)
		{
			GIFT_ERROR (("zlib: %i", err));
			return -1;
		}

		if (err == Z_STREAM_END)
			break;
	}

	return (int)(len - s->avail_out);
}
#endif /* USE_ZLIB */

int ft_stream_recv (FTStream *stream, FTPacket *stream_pkt,
                    FTStreamRecv func, void *udata)
{
	FTPacket      *packet;
	unsigned char *data;
	unsigned int   data_len;
	int len = 0;
	int ret = 0;

	if (!stream || !stream_pkt || !func)
		return -1;

	assert (stream->dir == FT_STREAM_RECV);

	if (stream->flags & FT_STREAM_FINISH)
	{
		stream->eof = TRUE;
		return 0;
	}

	assert (ft_packet_flags (stream_pkt) & FT_PACKET_STREAM);
	assert (stream_pkt->offset > 0);

	data = stream_pkt->data + FT_PACKET_HEADER + stream_pkt->offset;
	data_len = ft_packet_length (stream_pkt) - stream_pkt->offset;

#ifdef USE_ZLIB
	if (stream->flags & FT_STREAM_ZLIB)
	{
		stream->s.next_in = (Bytef *) data;
		stream->s.avail_in = data_len;
	}
#else /* !UZE_ZLIB */
	if (stream->flags & FT_STREAM_ZLIB)
	{
		TRACE (("some asshole sent an invalid stream"));
		stream->eof = TRUE;
		return -1;
	}
#endif /* USE_ZLIB */

	stream->pkts++;

	/* main loop: attempt to fill a large buffer of several uncompressed
	 * packets together */
	for (;;)
	{
		size_t offs = 0;

#ifdef USE_ZLIB
		if (stream->flags & FT_STREAM_ZLIB)
		{
			/* attempt to read as much uncompressed data as we can at a time,
			 * process once its decompressed */
			len = zlib_recv (&stream->s, stream->in_buf + stream->in_rem,
							 sizeof (stream->in_buf) - stream->in_rem);

			if (len < 0)
				break;
		}
		else
#endif /* USE_ZLIB */
		{

			len = MIN (data_len, sizeof (stream->in_buf)) - stream->in_rem;

			if (len <= 0)
				break;

			memcpy (stream->in_buf + stream->in_rem, data, len);
			data_len -= len;
		}

		if (len == 0)
			continue;

		/* pretend we read more than we actually did to emulate a joined
		 * buffer (old leftover data + new incoming data) without the lacking
		 * efficiency */
		len += stream->in_rem;

		/* inner loop: attempt to process and satisfy as many complete
		 * OpenFT packets as are available */
		for (;;)
		{
			packet = ft_packet_unserialize (stream->in_buf + offs,
			                                MAX (0, len - offs));

			if (!packet)
				break;

			/* increment offset */
			offs += FT_PACKET_HEADER + ft_packet_length (packet);
			stream->spkts++;
			ret++;

			/* raise callback */
			(*func) (stream, packet, udata);
			ft_packet_free (packet);
		}

		/* shift the buffer back to hold onto the remainder of the unprocessed
		 * data for the next call */
		if ((stream->in_rem = len - offs) > 0 && offs)
			memmove (stream->in_buf, stream->in_buf + offs, stream->in_rem);
	}

	return ret;
}

/*****************************************************************************/

static int clear_stream (Dataset *d, DatasetNode *node, int *cnt)
{
	ft_stream_free (node->value);
	(*cnt)++;

	return TRUE;
}

int ft_stream_clear (Connection *c, FTStreamDirection dir)
{
	Dataset **d;
	int       cnt = 0;

	if (!(d = get_direction (c, dir)))
		return 0;

	dataset_foreach (*d, DATASET_FOREACH(clear_stream), &cnt);
	dataset_clear (*d);
	*d = NULL;

	return cnt;
}

int ft_stream_clear_all (Connection *c)
{
	int cnt = 0;

	cnt += ft_stream_clear (c, FT_STREAM_RECV);
	cnt += ft_stream_clear (c, FT_STREAM_SEND);

	return cnt;
}
