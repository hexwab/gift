/*
 * $Id: upload.c,v 1.86 2003/06/26 19:50:47 jasta Exp $
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

#include "giftd.h"

#include "lib/event.h"
#include "lib/file.h"

#include "plugin/protocol.h"           /* UPLOAD_AUTH_ */

#include "share_cache.h"

#include "upload.h"

#include "if_transfer.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

typedef struct
{
	char     *user;                    /* username from queue_hit */
	size_t    pos;                     /* position in queue */
	timer_id  timer;                   /* timer keeping this object alive */
} queue_t;

static input_id  upload_timer  = 0;
static List     *uploads       = NULL;
static Array    *uploads_queue = NULL; /* see queue_hit */

/*****************************************************************************/

/*
 * max_uploads:
 *
 *  0    no upload slots available
 * -1    unlimited
 */
static int max_uploads = -1;
static int disabled    = FALSE;

#define MAX_PERUSER_UPLOADS \
	    config_get_int (gift_conf, "sharing/max_peruser_uploads=1")

/*****************************************************************************/

#ifdef THROTTLE_ENABLE

static unsigned int max_upstream = 0;

#define MAX_UPLOAD_BW max_upstream

static int            throttle_resume_timer        = 0;
static unsigned long  upload_credits               = 0;

#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

static int upload_report_progress (Transfer *transfer, void *udata)
{
	if (!UPLOAD(transfer)->display)
	{
		GIFT_TRACE (("son of a bitch"));
		return TRUE;
	}

	if_transfer_change (transfer->event, FALSE);

#ifdef THROTTLE_ENABLE
	transfer_calc_bw (transfer);
#endif

	return TRUE;
}

static int upload_report (void *udata)
{
	list_foreach (uploads, (ListForeachFunc) upload_report_progress, NULL);
	return TRUE;
}

size_t upload_throttle (Chunk *chunk, size_t len)
{
#ifdef THROTTLE_ENABLE
	return chunk_throttle (chunk, MAX_UPLOAD_BW, len, upload_credits);
#else
	return len;
#endif /* THROTTLE_ENABLE */
}

/*****************************************************************************/

List *upload_list ()
{
	return uploads;
}

#ifdef THROTTLE_ENABLE
static void throttle_resume (void *arg)
{
	transfers_resume (uploads, MAX_UPLOAD_BW, &upload_credits);
}
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

static char *get_hash_hack (char *htype, char *hash)
{
	static char hash_hack[128];

	snprintf (hash_hack, sizeof (hash_hack) - 1, "%s:%s", htype, hash);

	return hash_hack;
}

/* display is a boolean used to register this data structure but NOT report
 * it to the user interface yet.  this is useful for firewalled push
 * (http_push_file) */
Transfer *upload_new (Protocol *p, char *user, char *htype, char *hash,
                      char *filename, char *path, off_t start, off_t stop,
                      int display, int shared)
{
	Transfer *transfer;
	Chunk    *chunk;
	Source   *source;

	/* create the transfer object */
	transfer = transfer_new (TRANSFER_UPLOAD, filename,
	                         get_hash_hack (htype, hash), stop);

	if (!transfer)
		return NULL;

	transfer->transmit = start;
	transfer->transmit_change = start;

	transfer->path             = STRDUP (path);
	UPLOAD(transfer)->display  = display;
	UPLOAD(transfer)->shared   = shared;

	/* add source */
	source = source_new (user, hash,
	                     stringf ("%s://%s%s", p->name, user, path));
	transfer->sources = list_prepend (transfer->sources, source);

	/* add chunk
	 * NOTE: chunk_new sets up the circular reference */
	chunk = chunk_new (transfer, source, start, stop);

#ifdef THROTTLE_ENABLE
	max_upstream = config_get_int (gift_conf, "bandwidth/upstream=0");

	if (throttle_resume_timer == 0 && MAX_UPLOAD_BW > 0)
	{
		throttle_resume_timer =
		    timer_add (THROTTLE_TIME, (TimerCallback) throttle_resume, NULL);

		/* set the initial upload credits */
		upload_credits = max_upstream * THROTTLE_TIME / SECONDS;
	}
#endif /* THROTTLE_ENABLE */

	/* register the transfer object */
	uploads = list_prepend (uploads, transfer);

	/* ensure that we have some kind of status reporting */
	if (!upload_timer)
	{
		upload_timer = timer_add (1 * SECONDS, (TimerCallback) upload_report,
		                          NULL);
	}

	if (display)
	{
		source->status = SOURCE_WAITING;
		source->p->upload_avail (source->p, upload_availability ());
	}

	/* add the if_event */
	if (!(transfer->event = if_transfer_new (NULL, 0, transfer)))
	{
		transfer_free (transfer);
		return NULL;
	}

	return transfer;
}

static void upload_free (Transfer *upload)
{
	Chunk  *chunk;
	Source *source;

	assert (upload != NULL);

	/* unregister this event.  this should also dump the last known status
	 * to the ui for reporting */
	if_transfer_finish (upload->event);
	upload->event = NULL;

	uploads = list_remove (uploads, upload);

	/* let the protocol know this happened */
	if (upload->chunks)
	{
		chunk  = upload->chunks->data;
		source = chunk->source;

		source->p->upload_avail (source->p, upload_availability ());
		source->p->upload_stop (source->p, upload, chunk, source);
	}

	transfer_free (upload);

	/* unload the timer, no reason to call it anymore */
	if (!uploads)
	{
		timer_remove (upload_timer);
		upload_timer = 0;

#ifdef THROTTLE_ENABLE
		timer_remove (throttle_resume_timer);
		throttle_resume_timer = 0;
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

	if (chunk->start + chunk->transmit >= chunk->stop)
		source->status = SOURCE_COMPLETE;

#if 0
	/* give the protocol a heads up */
	if (source->p->upload)
		(*source->p->upload) (chunk, PROTOCOL_TRANSFER_CANCEL, NULL);
#endif

#if 0
	chunk->source = NULL;
	source->chunk = NULL;
#endif

	upload_free (upload);
}

/*****************************************************************************/

void upload_write (Chunk *chunk, unsigned char *segment, size_t len)
{
	unsigned long remainder;

	assert (chunk);
	assert (chunk->transfer);

	if (!segment || len == 0)
	{
		upload_stop (chunk->transfer, TRUE);
		return;
	}

	remainder = chunk->transfer->total - chunk->transfer->transmit;

	if (len > remainder)
		len = remainder;

	chunk->transmit           += len;
	chunk->transfer->transmit += len;

	chunk->source->status = SOURCE_ACTIVE;

#ifdef THROTTLE_ENABLE
	if (MAX_UPLOAD_BW > 0)
		upload_credits -= len;
#endif /* THROTTLE_ENABLE */

	/* upload completed, tell the protocol to clean itself up */
	if (remainder - len <= 0)
		upload_stop (chunk->transfer, TRUE);
}

/*****************************************************************************/

int upload_length (char *user)
{
	return transfer_length (uploads, user, FALSE);
}

/*****************************************************************************/

static queue_t *queue_find (const char *user)
{
	size_t   i;
	size_t   len = array_count (&uploads_queue);
	queue_t *queue;

	for (i = 0; i < len; i++)
	{
		queue = array_index (&uploads_queue, i);
		assert (queue != NULL);

		if (strcmp (queue->user, user) == 0)
			return queue;
	}

	return NULL;
}

static queue_t *queue_new (const char *user)
{
	queue_t *queue;

	assert (user != NULL);

	if (!(queue = MALLOC (sizeof (queue_t))))
		return NULL;

	queue->user = STRDUP (user);
	queue->timer = 0;

	return queue;
}

static void queue_free (queue_t *queue)
{
	assert (queue != NULL);

	free (queue->user);
	free (queue);
}

static BOOL remove_queue_tick (queue_t *queue)
{
	assert (queue->pos >= 0);
	assert (queue->pos < array_count (&uploads_queue));

	/* remove the entry and nuke the queue object */
	array_splice (&uploads_queue, queue->pos, 1, NULL);
	queue_free (queue);

	/* do not renew the timer (formerly queue->timer) */
	return FALSE;
}

static queue_t *queue_add (const char *user)
{
	queue_t *queue;

	if (!(queue = queue_new (user)))
		return NULL;

	/* the position is meant to be the index, which would be the previous
	 * number of members */
	queue->pos = array_count (&uploads_queue);
	array_push (&uploads_queue, queue);

	/* run a timer to remove this entry after so much idle time (that is, time
	 * spent between hits) */
	queue->timer =
	    timer_add (2 * MINUTES, (TimerCallback)remove_queue_tick, queue);

	return queue;
}

static char *queue_next (void)
{
	queue_t *queue;

	/* TODO: implement array_push_peek? */
	if (!(queue = array_index (&uploads_queue, 0)))
		return NULL;

	return queue->user;
}

static void queue_shift (void)
{
	queue_t *queue;

	queue = array_shift (&uploads_queue);

	timer_remove (queue->timer);
	queue_free (queue);
}

/*
 * Each time a queued upload request is made we "hit" the users status to
 * keep them alive in the fair position-based upload queue.  This queue
 * method is used to guarantee that no one can alter their timers to get a
 * better "chance" at slipping into a queue slot.  It's inefficient as shit
 * but its implemented here so that other protocols may take advantage of the
 * feature (even if the protocol has no such support).
 *
 * The fair upload queue effectively tracks all failed requests and will wait
 * for the next person in line to request a file when the queue has an
 * available slot.  If any user does not "hit" their upload in a given amount
 * of time a timer will be raised which removes them from the queue and they
 * must start over again.  This is done because most common implementations
 * will use a basic 60 second timer for re-requesting and we can rely on a
 * lacking request in x amount of time means the transfer was cancelled on
 * their client side.
 */
static unsigned int queue_hit (const char *user)
{
	queue_t *queue;

	if ((queue = queue_find (user)))
		timer_reset (queue->timer);
	else
	{
		queue = queue_add (user);
		assert (queue != NULL);
	}

	return ((unsigned int)queue->pos);
}

/*****************************************************************************/

static int test_max_uploads (const char *user)
{
	int max_peruser         = MAX_PERUSER_UPLOADS;
	unsigned int active_uls = (unsigned int)(upload_length ((char *)user));
	unsigned int open_slots = (unsigned int)(upload_availability ());
	int ret;

	if (max_peruser > 0)
	{
		if (active_uls >= max_peruser)
			return UPLOAD_AUTH_MAX_PERUSER;
	}

	switch (open_slots)
	{
	 case 1:                           /* only one slot left */
		{
			char *queue_user = queue_next ();

			/* user requesting is the next user in line (or we there are no
			 * users in line) */
			if (!queue_user || strcmp (queue_user, user) == 0)
			{
				/* move this user off the queue list and authorize */
				queue_shift ();
				ret = UPLOAD_AUTH_ALLOW;
				break;
			}
		}
	 case 0:                           /* may fall through from case 1 */
		{
			unsigned int pos;

			pos = queue_hit (user);
			GIFT_TRACE (("hit %s: %u", user, pos));

			ret = UPLOAD_AUTH_MAX;
		}
		break;
	 default:
		ret = UPLOAD_AUTH_ALLOW;
		break;
	}

	return ret;
}

int upload_auth (const char *user, Share *share)
{
	int ret;

	if (!share)
		return UPLOAD_AUTH_NOTSHARED;

	if (share_update_entry (share))
		return UPLOAD_AUTH_STALE;

	if ((ret = test_max_uploads (user)) != UPLOAD_AUTH_ALLOW)
		return ret;

	return UPLOAD_AUTH_ALLOW;
}

/*****************************************************************************/

void upload_disable ()
{
	disabled = TRUE;
	max_uploads = upload_status ();
}

void upload_enable ()
{
	disabled = FALSE;
	max_uploads = upload_status ();
}

int upload_status ()
{
	if (disabled)
		return 0;

	/* sync with config */
	max_uploads = config_get_int (gift_conf, "sharing/max_uploads=-1");

	return max_uploads;
}

unsigned long upload_availability ()
{
	unsigned long av = 0;
	int           status;

	status = upload_status ();

	if (status == 0)
		return av;

	/* hardcode an availability of 9 (arbitrarily high number) in the event
	 * that there is no limit imposed */
	if (status < 0)
		av = 9;
	else
		av = MAX (0, status - upload_length (NULL));

	return av;
}
