/*
 * transfer.c
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

#include "gift.h"

#include "transfer.h"
#include "event.h"

#include "download.h"
#include "upload.h"

/*****************************************************************************/

/* laziness.  this should be typedef'd */
void download_write (Chunk *chunk, char *segment, size_t len);
void upload_write   (Chunk *chunk, char *segment, size_t len);

/*****************************************************************************/

Transfer *transfer_new (TransferType direction,
                        char *filename, unsigned long size)
{
	Transfer *transfer;

	if (!(transfer = malloc (sizeof (Transfer))))
		return NULL;

	memset (transfer, 0, sizeof (Transfer));

	transfer->type     = direction;

	transfer->filename = STRDUP (filename);
	transfer->transmit = 0;
	transfer->total    = size;
	transfer->max_seek = 0;
	transfer->timer    = 0;

	return transfer;
}

void transfer_free (Transfer *transfer)
{
	List *ptr;

	assert (transfer);

	for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
		chunk_free (ptr->data);

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
		source_free (ptr->data);

	list_free (transfer->chunks);

	free (transfer->filename);
	free (transfer->path);

	if (transfer->f)
		fclose (transfer->f);

	timer_remove (transfer->timer);
	if_event_remove (transfer->id);

	free (transfer);
}

/*****************************************************************************/

void transfer_stop (Transfer *transfer, int cancel)
{
	TRACE (("transfer->type = %i, cancel = %i", transfer->type, cancel));

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_stop (transfer, cancel);
		break;
	 case TRANSFER_UPLOAD:
		upload_stop (transfer, cancel);
		break;
	 default:
		break;
	}
}

/*****************************************************************************/

Source *source_new (char *user, char *hash, char *url)
{
	Protocol *p;
	char     *ptr;
	Source   *source;

	/* hash may be NULL */
	if (!user || !url)
		return NULL;

	/* locate the protocol from the url */
	if ((ptr = strchr (url, ':')))
	{
		char  *buf;
		size_t buf_len;

		/* allocate a buffer to hold the protocol name for lookup */
		buf_len = ptr - url;
		buf = malloc (buf_len + 1);
		strncpy (buf, url, buf_len);
		buf[buf_len] = 0;

		p = protocol_find (buf);

		free (buf);
	}

	if (!p)
	{
		TRACE (("url '%s' is incomplete", url));
		return NULL;
	}

	/* allocate the Source structure */
	if (!(source = malloc (sizeof (Source))))
		return NULL;

	memset (source, 0, sizeof (Source));

	source->user = STRDUP (user);
	source->hash = STRDUP (hash);
	source->url  = STRDUP (url);
	source->p    = p;

	return source;
}

void source_free (Source *source)
{
	if (!source)
		return;

	free (source->user);
	free (source->hash);
	free (source->url);

	free (source);
}

int source_equal (Source *a, Source *b)
{
	int ret = 0;

	if (!a)
		return -1;

	if (!b)
		return 1;

	ret |= STRCMP (a->user, b->user);
	ret |= STRCMP (a->hash, b->hash);
	ret |= STRCMP (a->url,  b->url);

	return (ret == 0);
}

/*****************************************************************************/

Chunk *chunk_new (Transfer *transfer, Source *source,
                  size_t start, size_t stop)
{
	Chunk *chunk;

	if (!source)
		return NULL;

	if (!(chunk = malloc (sizeof (Chunk))))
		return NULL;

	memset (chunk, 0, sizeof (Chunk));

	chunk->transfer = transfer;
	chunk->source   = source;
	chunk->data     = NULL;

	chunk->transmit = 0;
	chunk->start    = start;
	chunk->stop     = stop;

	if (source)
		source->active = TRUE;

	chunk->active = TRUE;

	transfer->chunks = list_append (transfer->chunks, chunk);

	return chunk;
}

void chunk_free (Chunk *chunk)
{
	assert (chunk);

	/* source_free (chunk->source); */
	free (chunk);
}

void chunk_write (Chunk *chunk, char *segment, size_t len)
{
	Transfer *transfer = chunk->transfer;

	assert (chunk);
	assert (transfer);

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_write (chunk, segment, len);
		break;
	 case TRANSFER_UPLOAD:
		upload_write (chunk, segment, len);
		break;
	 default:
		GIFT_ERROR (("unknown transfer type %i", transfer->type));
		break;
	}
}

void chunk_cancel (Chunk *chunk)
{
	Transfer *transfer = chunk->transfer;

	TRACE_FUNC ();

	if (!chunk->active)
		return;

	chunk->active         = FALSE;
	chunk->source->active = FALSE;

	if (transfer->type == TRANSFER_DOWNLOAD)
		chunk->source->p->download (chunk);

	chunk->source         = NULL;
	chunk->data           = NULL;
}
