/*
 * download.c
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

#include <sys/types.h>
#include <sys/stat.h>

#include "gift.h"
#include "hook.h"

#include "download.h"
#include "parse.h"
#include "event.h"
#include "conf.h"
#include "file.h"
#include "share_cache.h"

/*****************************************************************************/

/* TODO -- a great bit of this file is duplicated in upload.c.  This _must_
 * be cleaned up */

extern Config *gift_conf;

/*****************************************************************************/

/* NOTE: do not enable this unless you really know what you're doing, or
 * one of the developers has told you to :) */
/* #define PAD_FILE */

/* do not subdivide the transfer if the amount left is less than this */
#define DIVIDE_THRESHOLD      500000  /* bytes */

#define MIN_INACTIVE          45      /* 45 seconds */
#define MAX_INACTIVE          900     /* 15 minutes */
#define MIN_INCREMENT         2       /* number of seconds to inc each tick */

#define MAX_PERUSER_DOWNLOADS 1       /* download queue */

/*****************************************************************************/

static unsigned long download_timer                = 0;
static List         *downloads                     = NULL;

/*****************************************************************************/

#ifdef THROTTLE_ENABLE

static unsigned int max_downstream = 0;

#define MAX_DOWNLOAD_BW max_downstream

static int            throttle_update_timer        = 0;
static int            throttle_tick_timer          = 0;
static unsigned short download_tick_count          = 0;

#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

/* keep this files organization clean */
static Chunk *unused_chunk         (Transfer *transfer);
static void   activate_chunk       (Chunk *chunk, Source *source);
static void   handle_chunk_timeout (Chunk *chunk, Transfer *transfer);
static int    deactivate_chunk     (Chunk *chunk);
static void   save_state           (Transfer *transfer);

/*****************************************************************************/

static int download_report_progress (Transfer *transfer, void *udata)
{
	if (!transfer_report_progress (transfer))
		return TRUE;

	/* write the accompanying state file (for resume) */
	if (transfer->flag < 5)
		transfer->flag++;
	else
	{
		save_state (transfer);
		transfer->flag = 0;
	}

	if (transfer->paused)
		return TRUE;

#ifdef THROTTLE_ENABLE
	transfer_calc_bw (transfer);
#endif

	list_foreach (transfer->chunks, (ListForeachFunc) handle_chunk_timeout,
	              transfer);

	return TRUE;
}

static int download_report (void *udata)
{
	list_foreach (downloads, (ListForeachFunc) download_report_progress, NULL);

	return TRUE;
}

/*****************************************************************************/

/* newly attached connections need to be given this information */
void download_report_attached (Connection *c)
{
	List *ptr, *cptr;

	for (ptr = downloads; ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;

		interface_send (c, "transfer",
		                "id=i",     transfer->id,
		                "action=s", "download",
		                "size=i",   transfer->total,
		                "hash=s",   transfer->hash,
		                "save=s",   transfer->filename,
		                NULL);

		/* add all the sources */
		for (cptr = transfer->sources; cptr; cptr = list_next (cptr))
		{
			Source *source = cptr->data;

			interface_send (c, "transfer",
			                "id=i",        transfer->id,
			                "user=s",      source->user,
			                "addsource=s", source->url,
			                NULL);
		}
	}
}

#ifdef THROTTLE_ENABLE
static void throttle_tick (void *arg)
{
	transfers_throttle_tick (downloads, &download_tick_count);
}

static void throttle_update (void *arg)
{
	transfers_throttle (downloads, MAX_DOWNLOAD_BW);
}
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

/* TODO -- merge download and upload length to transfer_length */
static int download_length (char *user, int active)
{
	List *ptr;
	List *cptr;
	int   length = 0;

	/* in order for this to be useful we have to loop all transfers then all
	 * chunks looking for a source from this user */
	for (ptr = downloads; ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;

		/* ugh */
		for (cptr = transfer->chunks; cptr; cptr = list_next (cptr))
		{
			Chunk *chunk = cptr->data;

			if (!chunk || !chunk->source)
				continue;

			/* only active sources should be counted as a download */
			if (active && chunk->source->status != SOURCE_ACTIVE)
				continue;

			/* ok, now we can actually match a source :) */
			if (user && STRCMP (chunk->source->user, user))
				continue;

			length++;
		}
	}

	return length;
}

/*****************************************************************************/

static void download_event_close (IFEvent *event)
{
	if (!event->c || event->c->data)
		if_event_reply (event->id, "transfer", "id=i", event->id, NULL);
}

static void download_register (Transfer *transfer, Connection *c)
{
	IFEventID id;

	/* every second dump stats */
	if (!download_timer)
	{
		download_timer = timer_add (1 * SECONDS,
		                            (TimerCallback) download_report, NULL);
	}

	/* register this download */
	id = if_event_new (c, IFEVENT_TRANSFER, (IFEventFunc) download_event_close,
	                   transfer);

	/* set the transfer id to the newly acquired event identifer */
	transfer->id = id;

#ifdef THROTTLE_ENABLE
	max_downstream = config_get_int (gift_conf, "bandwidth/downstream=0");

	if (!downloads && MAX_DOWNLOAD_BW > 0)
	{
		throttle_tick_timer =
		    timer_add (TICK_INTERVAL, (TimerCallback) throttle_tick, NULL);

		throttle_update_timer =
			timer_add (THROTTLE_TIME, (TimerCallback) throttle_update, NULL);
	}
#endif /* THROTTLE_ENABLE */

	downloads = list_append (downloads, transfer);
}

/* create a filename that can easily be identified as a "temporary"
 * download */
static char *uniq_file (Transfer *transfer)
{
	char  *uniq;
	struct timeval tv;

	if (!transfer || !transfer->filename)
		return NULL;

	if (!(uniq = malloc (strlen (transfer->filename) + 50)))
		return NULL;

	/* god I hope gettimeofday works ;) */
	do
	{
		platform_gettimeofday (&tv, NULL);

		sprintf (uniq, "%04lX%08lX%08lX.%s", tv.tv_sec % 0xffff, tv.tv_usec,
		         transfer->total, transfer->filename);
	} while (file_exists (uniq, NULL, NULL));

	return uniq;
}

Transfer *download_new (Connection *c, char *uniq, char *filename, char *hash,
                        unsigned long size)
{
	Transfer *transfer;

	if (!(transfer = transfer_new (TRANSFER_DOWNLOAD, filename, hash, size)))
		return NULL;

	if (uniq)
		transfer->uniq = STRDUP (uniq);
	else
	{
		/* create a unique temporary name
		 * NOTE: this is NOT a path!  It is simply a unique string that we
		 * can reference this transfer by in the incoming dir.  Will be used
		 * to construct path and state_path later */
		transfer->uniq = uniq_file (transfer);
	}

	download_register (transfer, c);

	transfer_log (transfer,
				  "New Download:\n"
	              "  UNIQ:     %s\n"
	              "  Filename: %s\n"
	              "  Hash:     %s\n"
	              "  Size:     %lu\n",
	              transfer->uniq, filename, hash, size);

	return transfer;
}

/*****************************************************************************/

static int find_next_queue (Transfer *transfer, Source **source)
{
	List *ptr;

	/* loop through all sources to find a user match */
	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Source *cmp = ptr->data;

		if (!cmp || !cmp->user || *source == cmp)
			continue;

		if (cmp->status != SOURCE_QUEUED_LOCAL)
			continue;

		/* found a match, return it */
		if (!strcmp ((*source)->user, cmp->user))
		{
			*source = cmp;
			return 0;
		}
	}

	return -1;
}

static void handle_next_queue (Transfer *transfer)
{
	List     *ptr;
	Chunk    *chunk;
	List     *waiting;

	TRACE (("%s", transfer->filename));

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Source *source = ptr->data;

		if (!source || !source->user)
			continue;

		/* look for a download with this user queued
		 * NOTE: when found, source will be set to the next queued source */
		waiting = list_find_custom (downloads, &source,
		                            (CompareFunc) find_next_queue);

		if (!waiting || !waiting->data)
		{
			TRACE (("unable to find a waiting transfer in the queue..."));
			return;
		}

		if (!(chunk = unused_chunk (waiting->data)))
			return;

		/* unset the queued status */
		source->status = SOURCE_WAITING;
		activate_chunk (chunk, source);
	}
}

void download_free (Transfer *transfer, int premature)
{
	List *ptr;
	List *next;

	transfer_log (transfer, "closing download: %i", premature);

	/* deactivate all chunks */
	ptr = transfer->chunks;

	while (ptr)
	{
		Chunk *chunk = ptr->data;

		next = ptr->next;

		/* this condition completely fucks with giFT.  The protocol needs
		 * to be notified that this chunk is going to be freed, but it has
		 * absolutely no way of doing so because there's no source (and thus
		 * no protocol) to reply to!  Pretty serious design flaw.  A temporary
		 * work around is to catch the condition and simply emulate a reply
		 * based on the sources list
		 *
		 * NOTE: this is CLEARLY temporary */
		if (chunk && !chunk->source)
		{
			if (!transfer->sources || !transfer->sources->data)
				TRACE (("*sigh*...notify jasta"));
			else
			{
				Source *source = transfer->sources->data;

				if (source->p && source->p->download)
				{
					(*source->p->download) (chunk, PROTOCOL_TRANSFER_CANCEL,
					                        NULL);
				}
			}
		}

		/* notify the protocol that this chunk is finished
		 * NOTE: this function may attempt to remove itself from this list
		 * if the chunk is complete */
		deactivate_chunk (chunk);

		ptr = next;
	}

	/* start any queued transfers that are waiting on these sources */
	handle_next_queue (transfer);

	/* unregister */
	downloads = list_remove (downloads, transfer);

	transfer_free (transfer);

	if (!downloads)
	{
		timer_remove (download_timer);
		download_timer = 0;

#ifdef THROTTLE_ENABLE
		timer_remove (throttle_tick_timer);
		timer_remove (throttle_update_timer);
		throttle_tick_timer = throttle_update_timer = 0;
#endif /* THROTTLE_ENABLE */
	}
}

/*****************************************************************************/

static char *calculate_final_path (Transfer *transfer)
{
	char *final_path;
	unsigned int uniq = 0;

	do
	{
		final_path =
		    COMPLETED_PATH (("%s%s%s", transfer->filename,
		                     (uniq ? "." : ""), (uniq ? ITOA (uniq) : "")));

		uniq++;
	}
	while (file_exists (final_path, NULL, NULL));

	return STRDUP (final_path);
}

static void download_complete (Transfer *transfer)
{
	char *final_path;

	TRACE (("transfer completed %lu/%lu",
	        transfer->transmit, transfer->total));

	final_path = calculate_final_path (transfer);

	if (transfer->f)
	{
		fclose (transfer->f);
		transfer->f = NULL;
	}

	/* remove the mangling */
	file_mv (transfer->path, final_path);

	free (transfer->path);
	transfer->path = final_path;

	hook_event ("download_complete", FALSE,
	            HOOK_VAR_STR, transfer->path,
	            HOOK_VAR_STR, transfer->hash,
	            HOOK_VAR_STR, transfer->state_path,
	            HOOK_VAR_NUL, NULL);

	/* state file is no longer required */
	unlink (transfer->state_path);

	share_add_entry (transfer->path);

	download_free (transfer, FALSE);
}

/*****************************************************************************/

static int chunk_pause (Chunk *chunk, Transfer *transfer)
{
	deactivate_chunk (chunk);
	return TRUE;
}

static int source_pause (Source *source, Transfer *transfer)
{
	source->status = SOURCE_PAUSED;
	return TRUE;
}

void download_pause (Transfer *transfer)
{
	transfer->paused = TRUE;

	list_foreach (transfer->chunks,  (ListForeachFunc) chunk_pause, transfer);
	list_foreach (transfer->sources, (ListForeachFunc) source_pause, transfer);
}

static int source_unpause (Source *source, Transfer *transfer)
{
	Chunk *chunk;

	source->status = SOURCE_WAITING;

	if (!(chunk = unused_chunk (transfer)))
		return TRUE;

	activate_chunk (chunk, source);

	return TRUE;
}

void download_unpause (Transfer *transfer)
{
	transfer->paused = FALSE;

	list_foreach (transfer->sources, (ListForeachFunc) source_unpause, transfer);
}

void download_stop (Transfer *transfer, int cancel)
{
	if (cancel)
	{
		if (transfer->f)
		{
			fclose (transfer->f);
			transfer->f = NULL;
		}

		TRACE (("removing transfer files for %s", transfer->uniq));

		if (transfer->path)
			unlink (transfer->path);

		if (transfer->state_path)
			unlink (transfer->state_path);
	}

	download_free (transfer, TRUE);
}

/*****************************************************************************/
/*
 * LOGIC ROUTINES FOR MULTISOURCED DOWNLOADING
 */

/* check to see if this chunk has reached its maximum amount of time w/o
 * seeing data */
static int data_timeout (Chunk *chunk)
{
	if (!chunk)
		return FALSE;

#if 0
	/* data hasn't timed out because we havent requested it */
	if (chunk->source && chunk->source->status == SOURCE_QUEUED_LOCAL)
		return FALSE;
#endif

	/* starts at 0 */
	if (chunk->timeout_max < MIN_INACTIVE)
		chunk->timeout_max = MIN_INACTIVE;

	if (chunk->timeout_cnt >= chunk->timeout_max)
	{
		/* reset timeout */
		chunk->timeout_cnt = 0;

		/* apply an incremental timeout in order to be polite */
		if (chunk->timeout_max < MAX_INACTIVE)
			chunk->timeout_max += MIN_INCREMENT;

		return TRUE;
	}

	/* increment timeout count or reset timeout values depending on whether
	 * or not data is was available */
	if (chunk->tmp_recv == 0)
		chunk->timeout_cnt++;
	else
	{
		/* valid data responded, make sure timeout is still at its default */
		chunk->timeout_cnt = 0;
		chunk->timeout_max = MIN_INACTIVE;
	}

	chunk->tmp_recv = 0;

	/* hasn't reached the maximum yet */
	return FALSE;
}

/*****************************************************************************/

/* find an unused division of the download (or create a new one)
 * NOTE: if the file is small enough, no chunk will be returned.  You must
 * wait for the chunks timeout to reassign if needed */
static Chunk *unused_chunk (Transfer *transfer)
{
	List  *ptr;
	Chunk *largest = NULL;
	unsigned long start = 0;
	unsigned long stop  = 0;
	unsigned long diff  = 0;
	unsigned long largest_diff = 0;

	/* first check to see if we don't already have a nulled chunk somewhere */
	for (ptr = transfer->chunks; ptr; ptr = ptr->next)
	{
		Chunk *tmp = ptr->data;

		/* found an unused chunk, no need to look further */
		if (!tmp->source)
			return tmp;

		/* locate the largest available chunk */
		if (tmp->start + tmp->transmit > tmp->stop)
		{
			TRACE (("chunk overrun..."));
			continue;
		}

		diff = tmp->stop - tmp->start - tmp->transmit;

		if (!largest || diff > largest_diff)
		{
			largest      = tmp;
			largest_diff = diff;
		}
	}

	/* do not redivide a chunk less than the desired threshold */
	if (largest && largest_diff < DIVIDE_THRESHOLD)
	{
		transfer_log (transfer, "cant redivide a file < %lu bytes",
		              DIVIDE_THRESHOLD);
		return NULL;
	}

	if (!largest)
		stop = transfer->total;
	else
	{
		/* subdivide the largest inactive chunk */
		diff = largest_diff / 2;
		stop = largest->stop;
		largest->stop -= diff;
		start = largest->stop;
	}

	/* no chunk was found unused, create a new one with the above divisions */
	return chunk_new (transfer, NULL, start, stop);
}

/*****************************************************************************/

static void activate_chunk (Chunk *chunk, Source *source)
{
	if (!chunk || !source)
		return;

	assert (source->chunk == NULL);

	/* set this only when status has never changed */
	if (source->status == SOURCE_UNUSED)
		source->status = SOURCE_WAITING;

	/* setup the circular reference */
	chunk->source = source;
	source->chunk = chunk;

	chunk->tmp_recv = 0;

	transfer_log (chunk->transfer, "activate_chunk: %lu - %lu",
	              chunk->start + chunk->transmit, chunk->stop);

	/* actually request to download this source now */
	if (source->p && source->p->download)
		(*source->p->download) (chunk, PROTOCOL_TRANSFER_START, NULL);
}

static void remove_chunk (Chunk *chunk)
{
	Transfer *transfer;

	if (!chunk)
		return;

	transfer = chunk->transfer;

	transfer->chunks = list_remove (transfer->chunks, chunk);
	chunk_free (chunk);
}

/* returns TRUE if the chunk has completed and was removed from the list
 * NOTE: you must not continue to use chunk if this function returns TRUE */
static int deactivate_chunk (Chunk *chunk)
{
	Transfer *transfer;
	Source   *source;
	int       completed;

	/* already deactivated */
	if (!chunk || !chunk->source)
		return FALSE;

	transfer  = chunk->transfer;
	source    = chunk->source;
	completed = chunk->start + chunk->transmit >= chunk->stop;

	/* this chunk was prematurely deactivated, punish the source by reducing
	 * its priority */
	if (!completed)
	{
		/* TODO -- actually use a priority rating and sort by it :) */
		transfer->sources = list_remove (transfer->sources, source);
		transfer->sources = list_append (transfer->sources, source);
	}

	if (completed)
		chunk->source->status = SOURCE_COMPLETE;
	else if (chunk->transmit)
		chunk->source->status = SOURCE_CANCELLED;

	/* only set this if the protocol never gave us any other change */
	if (chunk->source->status == SOURCE_WAITING)
		chunk->source->status = SOURCE_UNUSED;

	chunk->tmp_recv       = 0;

	transfer_log (transfer, "deactivate_chunk: %lu - %lu",
	              chunk->start + chunk->transmit, chunk->stop);

	/* notify the protocol that they need to clean up
	 * NOTE: we do this regardless of whether or not the chunk is completed
	 * just in case the protocol didn't wanna clean itself up */
	if (source->p && source->p->download)
		(*source->p->download) (chunk, PROTOCOL_TRANSFER_CANCEL, NULL);

	/* cleanup the data links */
	chunk->source->chunk  = NULL;
	chunk->source         = NULL;
	chunk->data           = NULL;

	/* this chunk completed, make sure we remove it from the chunks list */
	if (completed)
	{
		remove_chunk (chunk);
		return TRUE;
	}

	return FALSE;
}

/*****************************************************************************/

void relocate_source (Source *source)
{
	Transfer *transfer;
	Chunk    *chunk;

	/* what the fuck...this happened once when the source was somehow
	 * added twice...i have NO idea how this happened, but giFT shouldn't
	 * be crashing because of it */
	if (!source)
		return;

	assert (source->chunk);

	transfer = source->chunk->transfer;

	/* this source is moving on, disable it's old placement
	 * NOTE: this will handle removal from the chunk list if completed */
	if (!deactivate_chunk (source->chunk))
	{
		/* this chunk did NOT complete...do not attempt to relocate this
		 * source (but do not remove it either) */
		return;
	}

	/* locate a new chunk to use */
	if (!(chunk = unused_chunk (transfer)))
		return;

	activate_chunk (chunk, source);
}

/*****************************************************************************/

static int find_inactive (Source *a, void *b)
{
	/* local queues should NOT be handled automagically.  see download_free */
	if (a->status == SOURCE_QUEUED_LOCAL)
		return -1;

	/* if there is no chunk associated, return 0 to indicate we have found
	 * an inactivate source */
	return (a->chunk != NULL);
}

static Source *find_inactive_source (Transfer *transfer)
{
	List *ls;

	ls = list_find_custom (transfer->sources, NULL,
						   (CompareFunc) find_inactive);

	return (ls ? ls->data : NULL);
}

/* this function is actually called above from download_report */
static void handle_chunk_timeout (Chunk *chunk, Transfer *transfer)
{
	Source *source;

	if (!chunk)
		return;

	/* check to see if this chunk's segment timed out */
	if (!data_timeout (chunk))
		return;

	TRACE (("%p: chunk %lu-%lu (%lu) timed out...",
	        transfer, chunk->start, chunk->stop, chunk->transmit));

	if (chunk->source)
		chunk->source->status = SOURCE_TIMEOUT;

	/* if this is not the first time this source has timed out, cancel it
	 * in order to possibly breathe new life into the connection */
	if (chunk->timeout_max > MIN_INACTIVE + MIN_INCREMENT)
		deactivate_chunk (chunk);

	/* check to see if we actually have something to use before we proceed */
	if (!(source = find_inactive_source (transfer)))
		return;

	/* we have found a new source for this chunk, make sure it's cancelled.
	 * NOTE: this function is safe to call multiple times in this fashion, so
	 * dont worry :) */
	deactivate_chunk (chunk);

	/* activate w/ the new source */
	activate_chunk (chunk, source);
}

/*****************************************************************************/

static Source *locate_source (Transfer *transfer, Source *source)
{
	List   *ptr;
	Source *lsource;

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		lsource = ptr->data;

		if (!source_cmp (lsource, source))
			return lsource;
	}

	return NULL;
}

static int handle_source_queue (Transfer *transfer, Source *source)
{
	if (download_length (source->user, FALSE) < MAX_PERUSER_DOWNLOADS)
		return FALSE;

	source->status = SOURCE_QUEUED_LOCAL;
	/* sources_queue = list_append (sources_queue, source); */

	return TRUE;
}


/*****************************************************************************/

/*
 * this function removes the source from the transfer, disassociates with the
 * chunk, calls both PROTOCOL_SOURCE_REMOVE and PROTOCOL_TRANSFER_CANCEL in
 * protocol space and cleans up everything but the chunk associated.
 */
static void remove_source (Transfer *transfer, Source *source)
{
	if (!source)
		return;

	transfer_log (transfer, "removing source: %s", source->url);

	TRACE (("%s", source->url));

	if (source->p && source->p->download)
		(*source->p->download) (source->chunk, PROTOCOL_SOURCE_REMOVE, source);

	/* make sure the chunk is aware of this change */
	deactivate_chunk (source->chunk);

	/* remove from the list(s) and free */
	transfer->sources = list_remove (transfer->sources, source);
	/* sources_queue     = list_remove (sources_queue, source); */

	source_free (source);
}

/* called from the interface protocol */
void download_remove_source (Transfer *transfer, char *url)
{
	Source *tmp;
	Source *source; /* actual source located to be removed */

	if (!transfer || !url)
		return;

	/* construct a source structure to keep the interfaces sane */
	tmp = source_new ("", NULL, url);

	if ((source = locate_source (transfer, tmp)))
		remove_source (transfer, source);

	source_free (tmp);
}

/*****************************************************************************/

/* insert a source into the transfer list and activate it using either one
 * of the previously nulled chunks or create a new one for this source */
static int add_source (Transfer *transfer, Source *source)
{
	Chunk  *chunk;
	Source *old_source;

	if (!transfer || !source)
		return FALSE;

	transfer_log (transfer, "adding new source: %s", source->url);

	if ((old_source = locate_source (transfer, source)))
	{
		if (old_source->status == SOURCE_ACTIVE ||
		    old_source->status == SOURCE_QUEUED_LOCAL)
		{
			TRACE (("ignoring duplicate source %s", source->url));
			return FALSE;
		}

		TRACE (("replacing %s with %s...", old_source->url, source->url));
		remove_source (transfer, old_source);
	}

	transfer->sources = list_append (transfer->sources, source);

	if (transfer->paused)
	{
		source->status = SOURCE_PAUSED;
		return TRUE;
	}

	if (!(chunk = unused_chunk (transfer)))
		return TRUE;

	/* if need be, mark this source as queued and process it later */
	if (handle_source_queue (transfer, source))
		return TRUE;

	activate_chunk (chunk, source);

	return TRUE;
}

/* called from the interface protocol */
void download_add_source (Transfer *transfer,
                          char *user, char *hash, char *url)
{
	Source *source;

	if (!(source = source_new (user, hash, url)))
		return;

	if (!add_source (transfer, source))
		source_free (source);
}

/*****************************************************************************/
/*
 * CHUNK RECORDING
 */

static int open_output (Transfer *transfer)
{
	assert (transfer->uniq != NULL);

	/* transfer->path may have already been supplied by a recovered state
	 * file, so we probably shouldnt mess w/ it */
	if (!transfer->path)
		transfer->path = STRDUP (INCOMING_PATH (("%s", transfer->uniq)));

	/* i'm not really sure why judge did this...if anyone has a problem
	 * with it, blame him */
	if (!(transfer->f = fopen (transfer->path, "rb+")) &&
		!(transfer->f = fopen (transfer->path, "wb")))
	{
		GIFT_ERROR (("Can't open %s for writing: %s", transfer->path,
		            GIFT_STRERROR()));
		return FALSE;
	}

	TRACE (("%s", transfer->path));

	return TRUE;
}

/*****************************************************************************/

/* save resume state file */
static void save_state (Transfer *transfer)
{
	FILE       *f;
	List       *ptr;
	static char tmp_state[PATH_MAX];
	int         i = 0;

	if (!transfer || !transfer->chunks)
		return;

	assert (transfer->uniq != NULL);

	if (!transfer->state_path)
	{
		transfer->state_path =
		    STRDUP (INCOMING_PATH ((".%s.state", transfer->uniq)));
	}

	/* some dumbasses cant allocate enough disk space (eg soapy).  as a
	 * result, we get to add an ineffiency here to avoid truncating the
	 * statefile */
	snprintf (tmp_state, sizeof (tmp_state) - 1, "%s.tmp",
	          transfer->state_path);

	if (!(f = fopen (tmp_state, "w")))
		return;

	fprintf (f, "[transfer]\n");
	fprintf (f, "uniq = %s\n",      transfer->uniq);
	fprintf (f, "filename = %s\n",  transfer->filename);
	fprintf (f, "hash = %s\n",      transfer->hash);

	if (transfer->path)
		fprintf (f, "path = %s\n",      transfer->path);

	fprintf (f, "transmit = %lu\n", transfer->transmit);
	fprintf (f, "total = %lu\n",    transfer->total);
	fprintf (f, "max_seek = %lu\n", transfer->max_seek);
	fprintf (f, "chunks = ");

	for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
	{
		Chunk *chunk = ptr->data;

		fprintf (f, "%lu-%lu ", chunk->start + chunk->transmit, chunk->stop);
	}

	/* one newline for the loop above, another to start a new header */
	fprintf (f, "\n\n");

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		Source *source = ptr->data;

		fprintf (f, "[source%i]\n", i++);
		fprintf (f, "user = %s\n", source->user);
		fprintf (f, "url = %s\n", source->url);
		fprintf (f, "\n");
	}

	/* quick little write test */
	if (fprintf (f, "\n") <= 0)
	{
		GIFT_ERROR (("unable to write to %s: %s",
		             tmp_state, GIFT_STRERROR ()));
		fclose (f);
		return;
	}

	fclose (f);

	if (!file_mv (tmp_state, transfer->state_path))
	{
		GIFT_ERROR (("unable to move %s to %s: %s",
		             tmp_state, transfer->state_path, GIFT_STRERROR ()));
	}
}

/* pads the file up to seek_pos */
static int pad_file (Transfer *transfer, unsigned long seek_pos)
{
#ifdef PAD_FILE
	int diff;
#endif

	/* no need to pad */
	if (seek_pos <= transfer->max_seek)
		return TRUE;

#ifndef PAD_FILE

	transfer->max_seek = seek_pos;

#else /* !PAD_FILE */

	/* seek to the current end */
	if (fseek (transfer->f, transfer->max_seek, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
		return FALSE;
	}

	/* actually null pad the file now */
	while ((diff = (seek_pos - transfer->max_seek)) > 0)
	{
		size_t n;   /* bytes written */
		size_t len;

		/* calculate the size of this write */
		len = MIN (diff, sizeof (null_pad));

		if ((n = fwrite (null_pad, sizeof (char), len, transfer->f)) < len)
		{
			GIFT_ERROR (("fwrite: %s", GIFT_STRERROR()));
			return FALSE;
		}

		/* adjust max seek */
		transfer->max_seek += n;
	}

#endif /* !PAD_FILE */

	return TRUE;
}

/* actually write chunk data to disk */
static int write_data (Transfer *transfer, Chunk *chunk,
                       char *segment, size_t len)
{
	unsigned long seek_pos;
	signed long   remainder;
	size_t        written;

	if (!transfer || !chunk)
		return -1;

	seek_pos  = chunk->start + chunk->transmit;
	remainder = chunk->stop - seek_pos;

	/* make sure we aren't going to write past our alotted chunk */
	if (len > remainder)
		len = remainder;

	/* incremental padding
	 * NOTE: this code makes room in the file for a new chunk division */
	if (!(pad_file (transfer, seek_pos)))
		return -1;

	/* TODO - error handling */
	if (fseek (transfer->f, seek_pos, SEEK_SET) == -1)
	{
		GIFT_ERROR (("fseek: %s", GIFT_STRERROR()));
		return -1;
	}

	if ((written = fwrite (segment, sizeof (char), len, transfer->f)) < len)
	{
		GIFT_ERROR (("fwrite: %s", GIFT_STRERROR()));
		return -1;
	}

	transfer_log (transfer, "write_data: writing %lu bytes at %lu",
	              len, seek_pos);

	chunk->transmit    += len; /* this chunk */
	chunk->tmp_recv    += len; /* temporary transmit recording */
	transfer->transmit += len; /* total transfer stats */

	remainder -= len;

	/* new seek max */
	if (seek_pos == transfer->max_seek)
		transfer->max_seek += len;

	/* -1 is reserved as an error from this function, so ensure that remainder
	 * is either 0 or greater (as this condition is not an error) */
	return (remainder >= 0 ? remainder : 0);
}

/* writes the data to disk, also handles chunk completion/failures */
void download_write (Chunk *chunk, char *segment, size_t len)
{
	Transfer *transfer  = chunk->transfer;
	int       remainder;

	transfer_log (transfer, "download_write: %p, %i", segment, len);

	/* protocol closed the connection (the chunk may have completed
	 * successfully) */
	if (!segment || len == 0)
	{
		relocate_source (chunk->source);
		return;
	}

	/* this cant possibly be receiving valid data
	 * TODO - assert? */
	if (!chunk->source)
		return;

	/* open the file for writing if we have not done so already */
	if (!transfer->f &&	!open_output (transfer))
	{
		download_pause (transfer);
		return;
	}

	if ((remainder = write_data (transfer, chunk, segment, len)) < 0)
	{
		/* write_data returns negative on fseek or fwrite errors...uh oh */
		download_pause (transfer);
		return;
	}

	if (chunk->source->status != SOURCE_ACTIVE)
		chunk->source->status = SOURCE_ACTIVE;

	/* check to see if a chunk has completed */
	if (remainder == 0)
	{
		/* transfer has completed [successfully] */
		if (transfer->transmit >= transfer->total)
		{
			download_report (transfer);
			download_complete (transfer);

			return;
		}

		/* put this source to work for us again */
		relocate_source (chunk->source);
	}
}

/*****************************************************************************/
/*
 * RESUME INCOMPLETE TRANSFERS VIA STATE FILES
 */

/* load all the chunks into the transfer
 * FORMAT: 0-100 100-200 200-300 300-400 */
static void fill_chunks (Transfer *transfer, char *chunks)
{
	char   *token;
	char   *chunks0;
	size_t  start;
	size_t  stop;

	/* make sure we're dealing w/ our own memory here */
	chunks0 = chunks = STRDUP (chunks);

	while ((token = string_sep (&chunks, " ")))
	{
		start = ATOUL (string_sep (&token, "-"));
		stop  = ATOUL (token);

		/* huh? */
		if (start >= stop)
			continue;

		/* create this unfulfilled chunk division */
		chunk_new (transfer, NULL, start, stop);
	}

	free (chunks0);
}

/* reads a state file and attempts to construct a transfer from it */
static Transfer *download_read_state (char *file)
{
	Transfer     *transfer;
	Config       *conf;
	char         *uniq;
	char         *filename;
	char         *hash;
	char         *chunks;
	char          key_name[256];
	size_t        s_offs = 0;
	unsigned long total;

	file = STRDUP (file);

	if (!(conf = config_new (file)))
		return NULL;

	uniq     =        config_get_str (conf, "transfer/uniq");
	filename =        config_get_str (conf, "transfer/filename");
	hash     =        config_get_str (conf, "transfer/hash");
	total    = ATOUL (config_get_str (conf, "transfer/total"));
	chunks   =        config_get_str (conf, "transfer/chunks");

	/* make sure it's at least reasonably complete...if they fuck with their
	 * state file too much though, fuck them */
	if (!filename || !hash || !chunks || !total)
	{
		TRACE (("%s: corrupt state file", file));
		config_free (conf);
		free (file);
		return NULL;
	}

	TRACE (("awaking state transfer %s", filename));

	transfer = download_new (NULL, uniq, filename, hash, total);
	transfer->state_path = file;
	transfer->path       = STRDUP (config_get_str (conf, "transfer/path"));
	transfer->transmit   =         config_get_int (conf, "transfer/transmit");
	transfer->total      =         config_get_int (conf, "transfer/total");
	transfer->max_seek   =         config_get_int (conf, "transfer/max_seek");

	fill_chunks (transfer, chunks);

	/* loop the source headers */
	for (s_offs = 0 ;; s_offs++)
	{
		char *user;
		char *url;

		snprintf (key_name, sizeof (key_name) - 1, "source%i/url", s_offs);

		/* check to see if we really have a conf header here or not */
		if (!(url = config_get_str (conf, key_name)))
			break;

		snprintf (key_name, sizeof (key_name) - 1, "source%i/user", s_offs);
		user = config_get_str (conf, key_name);

		if (!user)
		{
			GIFT_DEBUG (("corrupt resume entry: source%i", s_offs));
			continue;
		}

		/* WARNING: this isn't the proper usage of download_add_source! */
		download_add_source (transfer, user, transfer->hash, url);
	}

	config_free (conf);

	return transfer;
}

/* restore the previously recorded transfer states from disk */
void download_recover_state ()
{
	DIR           *dir;
	struct dirent *d;

	/* scan the incoming directory for any incomplete state files */
	if (!(dir = file_opendir (INCOMING_PATH (("")))))
	{
		GIFT_ERROR (("opendir: %s", GIFT_STRERROR()));
		return;
	}

	while ((d = file_readdir (dir)))
	{
		size_t name_len = strlen (d->d_name);

		/* not long enough for .a.state */
		if (name_len < 8)
			continue;

		/* look for ^. and state$ */
		if (d->d_name[0] == '.' &&
		    !strcmp (d->d_name + (name_len - 5), "state"))
		{
			/* restore this individual download */
			download_read_state (INCOMING_PATH (("%s", d->d_name)));
		}
	}

	file_closedir (dir);
}
