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

/**/extern Config *gift_conf;

/*****************************************************************************/

static unsigned long upload_timer = 0;   /* if_event_reply */
static List *uploads = NULL;

#ifdef THROTTLE_ENABLE

static unsigned int max_upstream = 0;

#define MAX_UPLOAD_BW max_upstream

static int            throttle_update_timer        = 0;
static int            throttle_tick_timer          = 0;
static unsigned short upload_tick_count            = 0;

#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

static int upload_report (void *udata)
{
	List *ptr;

	/* loop all uploads and report them */
	for (ptr = uploads; ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;

#ifdef THROTTLE_ENABLE
		transfer_calc_bw (transfer);
#endif

		if (!transfer->display)
			continue;

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

	if (!transfer->display)
		return;

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
		                "hash=s",   transfer->hash,
		                "href=s",   chunk->source->url,
		                NULL);
	}
}

#ifdef THROTTLE_ENABLE
static void throttle_tick (void *arg)
{
	transfers_throttle_tick (uploads, &upload_tick_count);
}

static void throttle_update(void *arg)
{
	transfers_throttle (uploads, MAX_UPLOAD_BW);
}
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

void upload_display (Transfer *transfer)
{
	Source *source;

	if (transfer->display)
	{
		TRACE (("display = %i", transfer->display));
		return;
	}

	if (!transfer->chunks || !transfer->chunks->data)
		return;

	/* mm. pretty. */
	if (!(source = ((Chunk *)transfer->chunks->data)->source))
		return;

	/* display the <transfer .../> initiation tag */
	if_event_reply (transfer->id, "transfer",
	                "id=i",     transfer->id,
	                "action=s", "upload",
	                "size=i",   transfer->total,
	                "user=s",   source->user,
	                "hash=s",   source->hash,
	                "href=s",   source->url,
	                NULL);

	transfer->display = TRUE;
}

/* display is a boolean used to register this data structure but NOT report
 * it to the user interface yet.  this is useful for firewalled push
 * (http_push_file) */
Transfer *upload_new (Protocol *p, char *host, char *hash, char *filename,
                      char *path, size_t start, size_t stop, int display)
{
	IFEventID id;
	Transfer *transfer;
	char     *href;

	/* create the transfer object */
	transfer = transfer_new (TRANSFER_UPLOAD, filename, hash, stop);
	transfer->transmit = start;
	transfer->path     = STRDUP (path);
	transfer->display  = display;

	/* add the if_event */
	id = if_event_new (NULL, IFEVENT_TRANSFER, (IFEventFunc) upload_close,
	                   transfer);
	transfer->id = id;

	/* construct the protocol string */
	href = malloc (strlen (host) + strlen (path) + 256);
	sprintf (href, "%s://%s%s", p->name, host, path);

	if (display)
	{
		/* display the <transfer .../> initiation tag */
		if_event_reply (transfer->id, "transfer",
		                "id=i",     transfer->id,
		                "action=s", "upload",
		                "size=i",   stop,
		                "user=s",   host,
		                "hash=s",   hash,
		                "href=s",   href,
		                NULL);
	}

	/* so that we know where this upload is going */
	chunk_new (transfer, source_new (host, hash, href), start, stop);

	free (href);

#ifdef THROTTLE_ENABLE
	max_upstream = config_get_int (gift_conf, "bandwidth/upstream=0");

	if(!uploads && MAX_UPLOAD_BW > 0)
	{
		throttle_tick_timer =
		    timer_add (TICK_INTERVAL, (TimerCallback) throttle_tick, NULL);

		throttle_update_timer =
		    timer_add (THROTTLE_TIME, (TimerCallback) throttle_update, NULL);
	}
#endif /* THROTTLE_ENABLE */

	/* register the transfer object */
	uploads = list_append (uploads, transfer);

	/* ensure that we have some kind of status reporting */
	if (!upload_timer)
	{
		upload_timer = timer_add (1 * SECONDS, (TimerCallback) upload_report,
		                          NULL);
	}

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

#ifdef THROTTLE_ENABLE
		timer_remove (throttle_tick_timer);
		timer_remove (throttle_update_timer);
		throttle_tick_timer = throttle_update_timer = 0;
#endif /* THROTTLE_ENABLE */
	}
}

/*****************************************************************************/

void upload_stop (Transfer *upload, int cancel)
{
	Chunk  *chunk;
	Source *source;

	assert (upload);

	chunk  = upload->chunks->data;
	source = chunk->source;

	chunk->source = NULL;

	/* give the protocol a heads up */
	if (source->p->upload)
		(*source->p->upload) (chunk, PROTOCOL_TRANSFER_CANCEL, NULL);

	upload_free (upload);
}

/*****************************************************************************/

void upload_write (Chunk *chunk, char *segment, size_t len)
{
	unsigned long remainder;

	assert (chunk);
	assert (chunk->transfer);

	if (!segment || len == 0)
	{
		upload_free (chunk->transfer);
		return;
	}

	remainder = chunk->transfer->total - chunk->transfer->transmit;

	if (len > remainder)
		len = remainder;

	chunk->transmit           += len;
	chunk->transfer->transmit += len;
}

/*****************************************************************************/

int upload_length (char *user)
{
	List *ptr;
	int   length = 0;

	for (ptr = uploads; ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;
		Chunk    *chunk    = transfer->chunks->data;

		/* if this transfer is in limbo, dont count it */
		if (!transfer->display)
			continue;

		if (user && strcmp (chunk->source->user, user))
			continue;

		length++;
	}

	return length;
}
