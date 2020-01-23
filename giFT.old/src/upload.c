/*
 * upload.c
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

#include "event.h"

#include "upload.h"

/*****************************************************************************/

static unsigned long upload_timer = 0;   /* if_event_reply */
static List *uploads      = NULL;

/*****************************************************************************/

static int upload_report (void *udata)
{
	List *ptr;

	/* loop all uploads and report them */
	for (ptr = uploads; ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;

		if_event_reply (transfer->id, "transfer",
		                "id=i",       transfer->id,
		                "transmit=i", transfer->transmit,
		                "total=i",    transfer->total,
		                NULL);
	}

	return TRUE;
}

/*****************************************************************************/

static void upload_close (IFEvent *event)
{
	Transfer *transfer;

	transfer = event->data;

	if_event_reply (event->id, "transfer",
	                "id=i",       event->id,
	                "transmit=i", transfer->transmit,
	                "total=i",    transfer->total,
	                NULL);

	if_event_reply (event->id, "transfer", "id=i", event->id, NULL);
}

/*****************************************************************************/

/* newly attached connections need to be given this information */
void upload_report_attached (Connection *c)
{
	List *ptr;

	for (ptr = uploads; ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;
		Chunk    *chunk    = transfer->chunks->data;

		if (!chunk->source)
			continue;

		/* display the <transfer .../> initiation tag */
		interface_send (c, "transfer",
		                "id=i",     transfer->id,
		                "action=s", "upload",
		                "size=i",   transfer->total,
		                "user=s",   chunk->source->user,
						"hash=s",   chunk->source->hash,
		                "href=s",   chunk->source->url,
		                NULL);
	}
}

/*****************************************************************************/

Transfer *upload_new (Protocol *p, char *host, char *hash, char *filename,
                      char *path, size_t start, size_t stop)
{
	IFEventID id;
	Transfer *transfer;
	char     *href;

	/* create the transfer object */
	transfer = transfer_new (TRANSFER_UPLOAD, filename, stop);
	transfer->transmit = start;
	transfer->path     = STRDUP (path);

	/* add the if_event */
	id = if_event_new (NULL, IFEVENT_TRANSFER, (IFEventFunc) upload_close,
	                   transfer);
	transfer->id = id;

	/* construct the protocol string */
	href = malloc (strlen (host) + strlen (path) + 256);
	sprintf (href, "%s://%s%s", p->name, host, path);

	/* display the <transfer .../> initiation tag */
	if_event_reply (transfer->id, "transfer",
	                "id=i",     transfer->id,
	                "action=s", "upload",
	                "size=i",   stop,
					"user=s",   host,
					"hash=s",   hash,
	                "href=s",   href,
	                NULL);

	/* so that we know where this upload is going */
	chunk_new (transfer, source_new (host, hash, href), start, stop);

	free (href);

	/* register the transfer object */
	uploads = list_append (uploads, transfer);

	/* ensure that we have some kind of status reporting */
	if (!upload_timer)
		upload_timer = timer_add (1, (TimerCallback) upload_report, NULL);

	return transfer;
}

void upload_free (Transfer *upload)
{
	uploads = list_remove (uploads, upload);
	transfer_free (upload);

	/* unload the timer, no reason to call it anymore */
	if (!uploads)
	{
		timer_remove (upload_timer);
		upload_timer = 0;
	}
}

/*****************************************************************************/

void upload_stop (Transfer *upload, int cancel)
{
	Chunk *chunk;

	assert (upload);

	chunk = upload->chunks->data;
	chunk->active = FALSE;

	/* give the protocol a heads up */
	if (chunk->source->p->upload)
		(*chunk->source->p->upload) (chunk);

	upload_free (upload);
}

/*****************************************************************************/

void upload_write (Chunk *chunk, char *segment, size_t len)
{
	assert (chunk);
	assert (chunk->transfer);

	if (!segment || len == 0)
	{
		upload_free (chunk->transfer);
		return;
	}

	chunk->transfer->transmit += len;
}
