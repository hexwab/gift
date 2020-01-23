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
#include "hook.h"
#include "file.h"

#include "share_cache.h"

#include "upload.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

static unsigned long upload_timer                  = 0;
static List *uploads                               = NULL;

/*****************************************************************************/

/* 0  == no uploads
 *  * -1 == unlimited */
static int max_uploads = -1;
static int disabled    = FALSE;

#define MAX_PERUSER_UPLOADS \
	    config_get_int (gift_conf, "sharing/max_peruser_uploads=1")

/*****************************************************************************/

#ifdef THROTTLE_ENABLE

static unsigned int max_upstream = 0;

#define MAX_UPLOAD_BW max_upstream

static int            throttle_update_timer        = 0;
static int            throttle_tick_timer          = 0;
static unsigned short upload_tick_count            = 0;

#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

static int upload_report_progress (Transfer *transfer, void *udata)
{
	if (!transfer->display)
	{
		TRACE (("son of a bitch"));
		return TRUE;
	}

	if (!transfer_report_progress (transfer))
		return TRUE;

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

	if (source->p && source->p->upload)
		(*source->p->upload) (source->chunk, PROTOCOL_TRANSFER_REGISTER, NULL);
}

/* display is a boolean used to register this data structure but NOT report
 * it to the user interface yet.  this is useful for firewalled push
 * (http_push_file) */
Transfer *upload_new (Protocol *p, char *host, char *hash, char *filename,
                      char *path, unsigned long start, unsigned long stop,
                      int display)
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

	if (display)
	{
		Chunk  *chunk  = transfer->chunks->data;
		Source *source = chunk->source;

		source->status = SOURCE_WAITING;

		if (source->p && source->p->upload)
			(*source->p->upload) (chunk, PROTOCOL_TRANSFER_REGISTER, NULL);
	}

	return transfer;
}

static void upload_free (Transfer *upload)
{
	Chunk  *chunk;
	Source *source;

	uploads = list_remove (uploads, upload);

	/* let the protocol know this happened */
	if (upload->chunks)
	{
		chunk  = upload->chunks->data;
		source = chunk->source;

		if (source && source->p && source->p->upload)
			(*source->p->upload) (chunk, PROTOCOL_TRANSFER_UNREGISTER, NULL);
	}

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

	if (chunk->start + chunk->transmit >= chunk->stop)
		source->status = SOURCE_COMPLETE;

	/* give the protocol a heads up */
	if (source->p->upload)
		(*source->p->upload) (chunk, PROTOCOL_TRANSFER_CANCEL, NULL);

	chunk->source = NULL;

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
		upload_stop (chunk->transfer, TRUE);
		return;
	}

	remainder = chunk->transfer->total - chunk->transfer->transmit;

	if (len > remainder)
		len = remainder;

	chunk->transmit           += len;
	chunk->transfer->transmit += len;

	chunk->source->status = SOURCE_ACTIVE;

	/* upload completed, tell the protocol to clean itself up */
	if (remainder - len <= 0)
		upload_stop (chunk->transfer, TRUE);
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

/*****************************************************************************/

/*
 * each time a queued upload request is made we "hit" the users status to
 * keep them alive in the fair position-based upload queue.  this queue
 * method is used to guarantee that no one can alter their timers to get a
 * better "chance" at slipping into a queue slot.  its inefficient as shit
 * but its implemented here so that other protocols may take advantage of the
 * feature (even if the protocol has no such support)
 *
 * the fair upload queue effectively tracks all failed requests and will wait
 * for the next person in line to request a file when the queue has an
 * available slot.  if any user does not "hit" their upload in a given amount
 * of time a timer will be raised which removes them from the queue and they
 * must start over again.  this is done because most common implementations
 * will use a basic 60 second timer for re-requesting and we can rely on a
 * lacking request in x amount of time means the transfer was cancelled on
 * their client side.
 */
static void queue_hit (char *user)
{
	/* TODO */
}

/*****************************************************************************/

/* silly helper function.  hinders readability for sure ;) */
static char *auth_reason (AuthReason *mem, AuthReason value)
{
	if (mem)
		*mem = value;

	return NULL;
}

/* checks to see if the perl support has any "hint" as to which action we
 * should perform */
static int get_hook_hint (char *user, char *path)
{
	HookVar *hv;
	int      hint;

	hv = hook_event ("upload_auth", TRUE,
					 HOOK_VAR_STR, path,
					 HOOK_VAR_INT, upload_length (NULL),
					 HOOK_VAR_INT, max_uploads,
					 HOOK_VAR_NUL, NULL);

	if (!hv)
		return -1;

	if (hv->type == HOOK_VAR_INT)
	{
		TRACE (("hook event returned: %i",
				P_INT (hook_var_value (hv))));
	}

	hint = P_INT (hook_var_value (hv));
	hook_var_free (hv);

	return hint;
}

/*
 * this function will accept file paths in the form:
 *
 * /usr/local/share/giFT/OpenFT/nodepage.html
 *
 * -OR -
 *
 * /file.txt (root path exclusion)
 *
 * return:
 *   on success, the fully qualified local pathname is returned (you must free
 *   this)
 *
 *   if a storage location is specified for reason it will be set
 *   appropriately.
 */
char *upload_auth (char *user, char *path, AuthReason *reason)
{
	FileShare *file;
	char      *s_path;
	int        hv_hint;

	/* reconstructs the path w/o '.' or '..' elements
	 * TODO -- I think Windows accepts '...', but ironically that's a valid
	 * path on ext2 at least...rossta? */
	if (!(s_path = file_secure_path (path)))
		return auth_reason (reason, AUTH_INVALID);

	/* if the hook didn't suggest an action, go through the usual suspects */
	if ((hv_hint = get_hook_hint (user, path)) == -1)
	{
		/* this user has reached his limit ... force his client to queue the
		 * extra files */
		if (MAX_PERUSER_UPLOADS > 0 &&
		    upload_length (user) >= MAX_PERUSER_UPLOADS)
		{
			free (s_path);
			return auth_reason (reason, AUTH_MAX_PERUSER);
		}

		/* before we authorize a legitimate file share, check total upload
		 * count */
		if (max_uploads != -1 && upload_length (NULL) >= max_uploads)
		{
			free (s_path);
			queue_hit (user);
			return auth_reason (reason, AUTH_MAX);
		}
	} else if (hv_hint == -2) {
	  free (s_path);
	  queue_hit (user);
	  return auth_reason (reason, AUTH_MAX);
	}

	/* check to make sure we will actually share this file */
	file = share_find_file (s_path);
	free (s_path);

	if (!file || !file->sdata || !file->sdata->path)
		return auth_reason (reason, AUTH_INVALID);

	/* check if this entry is stale */
	if (share_update_entry (file))
		return auth_reason (reason, AUTH_STALE);

	auth_reason (reason, AUTH_ACCEPTED);
	return STRDUP (file->sdata->path);
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
