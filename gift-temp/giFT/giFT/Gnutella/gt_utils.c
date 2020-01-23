/*
 * $Id: gt_utils.c,v 1.3 2003/03/20 05:01:10 rossta Exp $
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

#include "gt_gnutella.h"

#include "gt_utils.h"

#ifdef USE_ZLIB
#include <zlib.h>
#endif /* USE_ZLIB */

/*****************************************************************************/

int peer_addr (int fd, unsigned long *r_ip, unsigned short *r_port)
{
	struct sockaddr_in sin;
	int                len = sizeof (struct sockaddr_in);

	if (getpeername (fd, (struct sockaddr *) &sin, &len) < 0)
		return FALSE;

	/* maybe port should be kept in network byte-order */
	if (r_port)
		*r_port = ntohs (sin.sin_port);

	if (r_ip)
		*r_ip   = sin.sin_addr.s_addr;

	return TRUE;
}

char *make_str (char *array, int len)
{
	static int   data_len = 0;
	static char *data     = 0;

	if (len <= 0)
		return "";

	if (!data_len || data_len < len)
	{
		if (data)
			free (data);

		if (!(data = malloc (len + 1)))
			return "(No memory for string)";
	}

	memcpy (data, array, len);

	data[len] = 0;

	if (len > data_len)
		data_len = len;

	return data;
}

void print_hex (char *buf, int len)
{
	int i, j;
	unsigned char *line;
	unsigned char *end;

	end = buf + len;

	while ((line = buf) != end)
	{
		for (i = 0; i < 16; i++)
		{
			if (line + i == end)
				break;

			printf ("%02x ", line[i]);
		}

		for (j = i; j < 16; j++)
			printf ("   ");

		printf (" ");

		for (i = 0; i < 16; i++)
		{
			if (line + i == end)
				break;

			printf ("%c", isalnum (line[i]) ? line[i] : '.');
		}

		buf += i;
		printf ("\n");
	}
}

/*****************************************************************************/
/* HTTP HEADER STORAGE */

#if 0
static unsigned long hash_lowercase (Dataset *d, void *key, size_t key_len)
{
	char *str;
	int i;
	unsigned long hash;

	for (hash = 0, i = 0; i < key_len; i++)
		hash ^= tolower (str[i]);

	return hash;
}

static int cmp_caseless (Dataset *d, DatasetNode *node, void *key,
                          size_t key_len)
{
	return strncasecmp (node->key, key, MIN (node->key_len, key_len));
}

/* Like a Dataset, but stores case-insensitive strings for keys to
 * string fields. */
Headers *headers_new ()
{
	Dataset *dataset;

	if (!(dataset = dataset_new (DATASET_DEFAULT)))
		return NULL;

	dataset->hash_func = hash_lowercase;

	return hdrs;
}

char *header_lookup (Headers *hdrs, char *key)
{
	char *value;

	if (!hdrs || !key)
		return NULL;

	return dataset_lookupstr (dataset, key);
}

void header_insert (Headers **hdrs, char *key, char *value)
{
	if (!d || !key)
		return;

	if (!(*hdrs) && !(*hdrs = headers_new ()))
		return;

	dataset_insertstr (hdrs->dataset, key, value);
}

void header_remove (Headers *hdrs, char *key)
{
	if (!hdrs)
		return NULL;

	dataset_remove (hdrs->dataset, key, size);
}
#endif

/*****************************************************************************/
/* ZLIB WRAPPER ROUTINES */

static char *zlib_strerror (int error)
{
#ifndef USE_ZLIB
	return NULL;
#else /* USE_ZLIB */
	switch (error)
	{
	 case Z_OK:             return "OK";
	 case Z_STREAM_END:     return "End of stream";
	 case Z_NEED_DICT:      return "Decompressing dictionary needed";
	 case Z_STREAM_ERROR:   return "Stream error";
	 case Z_ERRNO:          return "Generic zlib error";
	 case Z_DATA_ERROR:     return "Data error";
	 case Z_MEM_ERROR:      return "Memory error";
	 case Z_BUF_ERROR:      return "Buffer error";
	 case Z_VERSION_ERROR:  return "Incompatible runtime zlib library";
	 default:               break;
	}

	return "Invalid zlib error code";
#endif /* USE_ZLIB */
}

static void zstream_close (ZlibStream *stream)
{
#ifdef USE_ZLIB
	switch (stream->type)
	{
	 case ZSTREAM_INFLATE: inflateEnd (stream->streamptr);    break;
	 case ZSTREAM_DEFLATE: deflateEnd (stream->streamptr);    break;
	 default:                                                 break;
	}

	if (stream->streamptr)
		free (stream->streamptr);

	stream->type      = ZSTREAM_NONE;
	stream->streamptr = NULL;
#endif /* USE_ZLIB */
}

ZlibStream *zlib_stream_open (size_t max_size)
{
	ZlibStream *stream;
	char       *data;

	if (!(stream = malloc (sizeof (ZlibStream))))
		return NULL;

	if (!(data = malloc (max_size)))
	{
		free (stream);
		return NULL;
	}

	memset (stream, 0, sizeof (ZlibStream));
	memset (data, 0, max_size);

	stream->start = data;
	stream->end   = data + max_size;
	stream->data  = data;
	stream->pos   = data;
	stream->type  = ZSTREAM_NONE;

	return stream;
}

void zlib_stream_close (ZlibStream *stream)
{
	if (!stream)
		return;

	if (stream->type != ZSTREAM_NONE)
		zstream_close (stream);

	if (stream->data)
		free (stream->data);

	free (stream);
}

int zlib_stream_write (ZlibStream *stream, char *data, size_t size)
{
	if (!stream)
		return 0;

	/* check for overflow */
	if (stream->pos + (size-1) > stream->end)
		return 0;

	memcpy (stream->pos, data, size);

	stream->pos += size;

	return size;
}

int zlib_stream_read (ZlibStream *stream, char **r_data)
{
	size_t size;

	if (stream->start == stream->pos)
		return 0;

	*r_data = stream->start;

	size = stream->pos - stream->start;

	stream->start = stream->pos;

	return size;
}

int zlib_stream_inflate (ZlibStream *stream, char *zdata, size_t size)
{
#ifndef USE_ZLIB
	return FALSE;
#else /* USE_ZLIB */
	z_streamp inz;
	int ret;
	size_t free_size;

	if (!stream)
		return FALSE;

	if (!stream->streamptr)
	{
		assert (stream->type == ZSTREAM_NONE);

		if (!(inz = malloc (sizeof (*inz))))
			return FALSE;

		inz->zalloc = NULL;
		inz->zfree  = NULL;
		inz->opaque = NULL;

		if ((ret = inflateInit (inz)) != Z_OK)
		{
			TRACE (("inflateInit error %s", zlib_strerror (ret)));
			free (inz);
			return FALSE;
		}

		stream->type      = ZSTREAM_INFLATE;
		stream->streamptr = inz;
	}

	inz = stream->streamptr;

	/* Argh, I think this is right, but I'm not sure about the +1 */
	free_size = stream->end - stream->pos + 1;

	inz->next_in   = zdata;
	inz->avail_in  = size;
	inz->next_out  = stream->pos;
	inz->avail_out = free_size;

	TRACE (("next_out: %p avail_out: %u", inz->next_out, inz->avail_out));

	if ((ret = inflate (inz, Z_NO_FLUSH)) != Z_OK)
	{
		TRACE (("decompression error: %s", zlib_strerror (ret)));
		return FALSE;
	}

	TRACE (("inz->avail_in = %u, inz->avail_out = %u", inz->avail_in,
	        inz->avail_out));

	stream->pos += free_size - inz->avail_out;

	if (ret == Z_STREAM_END)
		zstream_close (stream);

	return TRUE;
#endif /* USE_ZLIB */
}
