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

#include "file.h"

/*****************************************************************************/

extern Config *gift_conf;

/*****************************************************************************/

#ifdef THROTTLE_ENABLE
#define CMP_THRESHOLD         0.01
#define THROTTLE_THRESHOLD    0.25
#endif /* THROTTLE_ENABLE */

/*****************************************************************************/

/* laziness.  this should be typedef'd */
void download_write (Chunk *chunk, char *segment, size_t len);
void upload_write   (Chunk *chunk, char *segment, size_t len);

/*****************************************************************************/

Transfer *transfer_new (TransferType direction,
                        char *filename, char *hash, unsigned long size)
{
	Transfer *transfer;

	if (!(transfer = malloc (sizeof (Transfer))))
		return NULL;

	memset (transfer, 0, sizeof (Transfer));

	transfer->type         = direction;
	transfer->filename     = STRDUP (filename);
	transfer->hash         = STRDUP (hash);
	transfer->transmit     = 0;
	transfer->transmit_old = 0;
	transfer->throughput   = 0;
	transfer->total        = size;
	transfer->max_seek     = 0;
#if 0
	transfer->timer        = 0;
#endif

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
	list_free (transfer->sources);

	free (transfer->hash);
	free (transfer->uniq);
	free (transfer->filename);
	free (transfer->path);
	free (transfer->state_path);

#ifdef TRANSFER_LOG
	free (transfer->log_path);
#endif

	if (transfer->f)
		fclose (transfer->f);

#if 0
	timer_remove (transfer->timer);
#endif

	if_event_remove (transfer->id);

	free (transfer);
}

/*****************************************************************************/

/* report a transfer's (upload or download) progress to the UI protocol */
int transfer_report_progress (Transfer *transfer)
{
	List *source_iter;
	List *cptr;

	/* nothing to do */
	if (!transfer->chunks)
		return FALSE;

	assert (transfer->transmit <= transfer->total);

	/* report the global progress */
	if_event_reply (transfer->id, "transfer",
	                "id=i",       transfer->id,
	                "paused=i",   transfer->paused,
	                "transmit=i", transfer->transmit,
	                "total=i",    transfer->total,
	                NULL);

	source_iter = transfer->sources;

	/* iterate all chunk divisions (while iterating sources) in order to
	 * report each individual range status */
	for (cptr = transfer->chunks; cptr; cptr = list_next (cptr))
	{
		Chunk  *chunk  = cptr->data;
		Source *source = NULL;
		char   *url;
		char   *status;

		/* if no source is associated with this chunk.  we need to try to
		 * "walk" along the sources list looking for a disassociated source
		 * to grab its status and url from.  this has no effect on giFT's
		 * internal logic whatsoever, it's just done to give more logical
		 * sense of what's going on to the UI */
		if (chunk->source)
			source = chunk->source;
		else
		{
			for (; source_iter; source_iter = list_next (source_iter))
			{
				source = source_iter->data;

				/* found something we can report with */
				if (!source->chunk)
				{
					/* dont let this one be printed twice :) */
					source_iter = list_next (source_iter);
					break;
				}

				/* make sure that the end-of-list doesn't have a valid source
				 * set, even if we decided not to use it */
				source = NULL;
			}
		}

		url = (source ? source->url : NULL);

		if (!(status = source_status (source)))
			status = "Unused";

		if_event_reply (transfer->id, "chunk",
		                "id=i",       transfer->id,
		                "href=s",     url,
		                "start=i",    chunk->start,
		                "transmit=i", chunk->transmit,
		                "total=i",    chunk->stop - chunk->start,
		                "status=s",   status,
						NULL);
	}

	return TRUE;
}

/*****************************************************************************/

void transfer_pause (Transfer *transfer)
{
	if (!transfer || transfer->paused)
		return;

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_pause (transfer);
		break;
	 default:
		break;
	}
}

void transfer_unpause (Transfer *transfer)
{
	if (!transfer || !transfer->paused)
		return;

	switch (transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		download_unpause (transfer);
		break;
	 default:
		break;
	}
}

void transfer_stop (Transfer *transfer, int cancel)
{
	if (!transfer)
		return;

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

/* determine the path to store the incoming/completed files, given the
 * filename destined for the path */
char *transfer_output_path (char *key, char *name, char *file)
{
	char *dir;
	char *fqp = NULL;

	/* file is memory from stringf */
	if (!(file = STRDUP (file)))
		return NULL;

	key = stringf ("%s=%s", key, gift_conf_path (name));
	dir = file_host_path (file_expand_path (config_get_str (gift_conf, key)));

	/* still host order, even though we used a /, sigh */
	if (dir)
		fqp = stringf ("%s/%s", dir, file);

	free (dir);
	free (file);

	/* make sure this path exists */
	file_create_path (fqp);

	return fqp;
}

/*****************************************************************************/
/* LOGGING */

void transfer_log (Transfer *transfer, char *fmt, ...)
{
#ifdef TRANSFER_LOG
	FILE   *f;
    va_list args;

	if (!transfer || transfer->type != TRANSFER_DOWNLOAD || !transfer->uniq)
		return;

	if (!transfer->log_path)
	{
		transfer->log_path =
		    STRDUP (INCOMING_PATH ((".%s.log", transfer->uniq)));
	}

	if (!(f = fopen (transfer->log_path, "a")))
		return;

	va_start (args, fmt);
	vfprintf (f, fmt, args);
	fprintf (f, "\n");
	va_end (args);

	fclose (f);
#endif /* TRANSFER_LOG */
}

/*****************************************************************************/

#ifdef THROTTLE_ENABLE
/* calculate if the transfers should be removed from input list */
int transfer_calc_bw (Transfer *transfer)
{
	if(transfer->transmit_old)
	{
		transfer->throughput = transfer->transmit - transfer->transmit_old;
		transfer->transmit_old = transfer->transmit;
	}
	else
		transfer->transmit_old = transfer->transmit;

	transfer->throughput = MAX (0, transfer->throughput);

	return transfer->throughput;
}
#endif /* THROTTLE_ENABLE */

static void chunk_throttle_op (Chunk *chunk, ProtocolCommand command,
                               void *data)
{
	Protocol *p;

	if (!chunk || !chunk->source || !chunk->source->p)
		return;

	p = chunk->source->p;

	switch (chunk->transfer->type)
	{
	 case TRANSFER_DOWNLOAD:
		if (p->download)
			(*chunk->source->p->download) (chunk, command, data);
		break;
	 case TRANSFER_UPLOAD:
		if (chunk->source->p->upload)
			(*chunk->source->p->upload) (chunk, command, data);
		break;
	}
}

int transfers_throttle_tick (List *transfers, unsigned short *tick_count)
{
	List *ptr, *cptr;
	Transfer *trans;
	Chunk *chunk;

	/* yes, technically tick_count should be checked for NULL */
	(*tick_count)++;

	if (*tick_count >= TICKS_PER_SEC)
		*tick_count = 0;

	for (ptr = transfers; ptr; ptr = list_next (ptr))
	{
		trans = ptr->data;

		for (cptr = trans->chunks; cptr; cptr = list_next (cptr))
		{
			chunk = cptr->data;

			if (*tick_count == 0)
				chunk_throttle_op (chunk, PROTOCOL_CHUNK_RESUME, NULL);
			else if (*tick_count == chunk->tick_cap)
			{
				chunk_throttle_op (chunk, PROTOCOL_CHUNK_SUSPEND,
				                   &chunk->adjust);
			}
		}
	}

	return TRUE;
}

/*****************************************************************************/

/* rounding functions... didn't want to rely on the math library :) */
#if 0
static unsigned short clamp_up (float x)
{
	return ((unsigned short)x) + 1;
}
#endif

static unsigned short clamp_down (float x)
{
	return (unsigned short)x;
}

int transfers_throttle (List *transfers, unsigned long max_bw)
{
	List          *ptr, *cptr;
	Transfer      *trans;
	Chunk         *chunk;
	unsigned long  total_bw = 0;
	float          ratio;
	unsigned short (*adjust_func)(float x);

	for (ptr = transfers; ptr; ptr = list_next (ptr))
	{
		trans = ptr->data;
		total_bw += trans->throughput;
	}

	/* prevent the connections from stalling */
	total_bw++;

	ratio = ((float) max_bw / (float) total_bw);

#if 0
	TRACE(("total_bw=%ld max_bw=%ld", total_bw, max_bw));
#endif

	/* TODO - use CLAMP */
	if (ratio > 1.0)
	{
		if (ratio > (1.0 + THROTTLE_THRESHOLD))
			ratio = 1.0 + THROTTLE_THRESHOLD;

		adjust_func = clamp_down;
	}
	else
	{
		if (ratio < (1.0 - THROTTLE_THRESHOLD))
			ratio = 1.0 - THROTTLE_THRESHOLD;

		adjust_func = clamp_down;
	}

	/* now we go through and throttle back the individual chunks */
	for (ptr = transfers; ptr; ptr = list_next (ptr))
	{
		trans = ptr->data;

		for (cptr = trans->chunks; cptr; cptr = list_next (cptr))
		{
			chunk = cptr->data;

			chunk->adjust *= ratio;

			if (chunk->adjust > 1.0)
				chunk->adjust = 1.0;

			chunk->tick_cap =
			    (unsigned short) adjust_func ((float)chunk->tick_cap * ratio);

			/* % is slow */
			if (chunk->tick_cap > (TICKS_PER_SEC - 1))
				chunk->tick_cap = (TICKS_PER_SEC - 1);
			else if (chunk->tick_cap == 0)
				chunk->tick_cap++;
		}
	}

	return TRUE;
}

/*****************************************************************************/

Source *source_new (char *user, char *hash, char *url)
{
	Protocol *p = NULL;
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

	source->chunk  = NULL;
	source->user   = STRDUP (user);
	source->hash   = STRDUP (hash);
	source->url    = STRDUP (url);
	source->p      = p;

	return source;
}

void source_free (Source *source)
{
	if (!source)
		return;

	if (source->chunk)
		source->chunk->source = NULL;

	free (source->user);
	free (source->hash);
	free (source->url);
	free (source->status_data);

	free (source);
}

int source_cmp (Source *a, Source *b)
{
	int ret = 0;

	if (!a || !a->p)
		return -1;

	if (!b || !b->p)
		return 1;

	if (a->p > b->p)
		return 1;
	else if (a->p < b->p)
		return -1;

	if (a->p->source_cmp)
		ret = (*a->p->source_cmp) (a, b);
	else
	{
		TRACE (("falling back to manual comparison"));

		ret |= STRCMP (a->user, b->user);
		ret |= STRCMP (a->hash, b->hash);
		ret |= STRCMP (a->url,  b->url);
	}

	return ret;
}

char *source_status (Source *source)
{
	char *status = NULL;

	if (!source)
		return NULL;

	switch (source->status)
	{
	 case SOURCE_UNUSED:
	 case SOURCE_WAITING:
		status = "Waiting";
		break;
	 case SOURCE_PAUSED:         status = "Paused";               break;
	 case SOURCE_QUEUED_REMOTE:
		{
			if (!source->status_data)
				status = "Queued (Remotely)";
			else
			{
				static char text[32];
				snprintf (text, sizeof (text) - 1, "Queued (%s)",
				          source->status_data);
				status = text;
			}
		}
		break;
	 case SOURCE_QUEUED_LOCAL:   status = "Queued";               break;
	 case SOURCE_ACTIVE:         status = "Active";               break;
	 case SOURCE_COMPLETE:       status = "Complete";             break;
	 case SOURCE_CANCELLED:      status = "Cancelled (Remotely)"; break;
	 case SOURCE_TIMEOUT:        status = "Timed out";            break;
	 default:
		break;
	}

	return status;
}

/*****************************************************************************/

Chunk *chunk_new (Transfer *transfer, Source *source,
                  size_t start, size_t stop)
{
	Chunk *chunk;

	if (!(chunk = malloc (sizeof (Chunk))))
		return NULL;

	memset (chunk, 0, sizeof (Chunk));

	chunk->transfer     = transfer;
	chunk->source       = source;
	chunk->data         = NULL;

	chunk->adjust       = 1.0;
	chunk->transmit     = 0;
	chunk->transmit_old = 0;
	chunk->throughput   = 0;
	chunk->start        = start;
	chunk->stop         = stop;
	chunk->tick_cap     = TICKS_PER_SEC - 1;

	/* activate this source as well */
	if (source)
		source->chunk = chunk;

	transfer->chunks = list_append (transfer->chunks, chunk);

	transfer_log (transfer, "chunk_new: %lu - %lu",
	              chunk->start, chunk->stop);

	return chunk;
}

void chunk_free (Chunk *chunk)
{
	if (!chunk)
		return;

	if (chunk->source)
		chunk->source->chunk = NULL;

	transfer_log (chunk->transfer, "chunk_free: %lu - %lu",
	              chunk->start, chunk->stop);

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
		assert (0);
		break;
	}
}

int chunk_calc_bw (Chunk *chunk)
{
	if (!chunk)
		return FALSE;

	if (!chunk->transmit_old)
		chunk->throughput   = chunk->transmit - chunk->transmit_old;

	chunk->transmit_old = chunk->transmit;

	return chunk->throughput;
}
