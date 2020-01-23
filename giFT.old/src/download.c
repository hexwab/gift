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

#ifdef WIN32
#include <windows.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <unistd.h>

#include "gift.h"

#include "download.h"
#include "parse.h"
#include "event.h"
#include "conf.h"
#include "file.h"

/*****************************************************************************/

/* undefine if you wish to download from multiple sources at once */
#define USE_MULTISOURCE

#define STATE_FILE_PREFIX "."
#define STATE_FILE_SUFFIX "state"

#define DIVIDE_THRESHOLD  500 /* bytes */

#define MAX_INACTIVE      30  /* seconds */

static List *downloads = NULL;

/*****************************************************************************/

static int find_inactive (Transfer *a, Transfer *b)
{
	return a->active;
}

static Source *find_inactive_source (Transfer *transfer)
{
	List *ls;

	ls = list_find_custom (transfer->sources, NULL,
						   (CompareFunc) find_inactive);

	return (ls ? ls->data : NULL);
}

static void handle_timeout (Chunk *chunk, Transfer *transfer)
{
	Source *source;

	if (!chunk)
		return;

	if (chunk->timeout < MAX_INACTIVE)
	{
		if (chunk->tmp_recv == 0)
			chunk->timeout++;
		else
			chunk->timeout = 0;

		chunk->tmp_recv = 0;

		return;
	}

	GIFT_DEBUG (("chunk %lu-%lu (%lu) timed out (%s), fetching new source",
				 chunk->start, chunk->stop, chunk->transmit,
				 chunk->source ? chunk->source->url : "no source"));

	chunk->timeout = 0;

	/* check for no sources before we cancel this one */
	if (!transfer->sources)
	{
		GIFT_DEBUG (("NO SOURCES!"));
		return;
	}

	/* detach the source from this chunk if one exists, and notify the protocol
	 * that it timed out */
	if (source)
	{
		/* this chunk still thinks it's active, but has just timed out...cancel
		 * it with the protocol */
		chunk_cancel (chunk);
	}

	/* find an available source */
	source = find_inactive_source (transfer);

	GIFT_DEBUG (("using source: %s",
	             source ? source->url : "no source found"));

	/* use the source found */
	if (source && source->url)
	{
		chunk->source  = source;
		chunk->active  = TRUE;
		source->active = TRUE;

		/* give the protocol instructions to download this file */
		if (chunk->source->p && chunk->source->p->download)
			chunk->source->p->download (chunk);

		chunk->tmp_recv = 0;
	}
}

static int download_report (Transfer *transfer)
{
	if (transfer->chunks)
	{
		if_event_reply (transfer->id, "transfer",
		                "id=i",       transfer->id,
		                "transmit=i", transfer->transmit,
		                "total=i",    transfer->total,
		                NULL);
	}

	list_foreach (transfer->chunks, (ListForeachFunc) handle_timeout,
	              transfer);

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
		                "save=s",   transfer->filename,
		                NULL);

		/* add all the sources */
		for (cptr = transfer->chunks; cptr; cptr = list_next (cptr))
		{
			Chunk *chunk = cptr->data;

			if (!chunk->source)
				continue;

			interface_send (c, "transfer",
			                "id=i",        transfer->id,
			                "user=s",      chunk->source->user,
			                "hash=s",      chunk->source->hash,
			                "addsource=s", chunk->source->url,
			                NULL);
		}
	}
}

/*****************************************************************************/

static void download_handle_change (Transfer *transfer)
{
	if (!transfer->chunks)
	{
		GIFT_WARN (("no sources for %s", transfer->filename));
		download_report (transfer);
		download_free (transfer, TRUE);
		return;
	}
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
	transfer->timer = timer_add (1, (TimerCallback) download_report,
	                             transfer);

	id = if_event_new (c, IFEVENT_TRANSFER, (IFEventFunc) download_event_close,
	                   transfer);

	transfer->id = id;

	downloads = list_append (downloads, transfer);
}

Transfer *download_new (Connection *c, char *filename, unsigned long size)
{
	Transfer *transfer;

	transfer = transfer_new (TRANSFER_DOWNLOAD, filename, size);
	download_register (transfer, c);

	return transfer;
}

/* TODO - this function is very incomplete */
void download_free (Transfer *transfer, int premature)
{
	List *ptr;

	TRACE_FUNC ();

	for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
	{
		Chunk *chunk = ptr->data;

		if (premature)
		{
			/* notify the protocol that this chunk is finished */
			chunk_cancel (chunk);
		}
	}

	downloads = list_remove (downloads, transfer);

	transfer_free (transfer);
}

/*****************************************************************************/

static char *calculate_final_path (Transfer *transfer)
{
	char *final_path;
	unsigned int uniq = 0;

	do
	{
		final_path =
		    gift_conf_path ("incoming/%s%s%s", transfer->filename,
		                    (uniq ? "." : ""), (uniq ? ITOA (uniq) : ""));

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

	/* remove the mangling */
	rename (transfer->path, final_path);

	free (final_path);

	/* state file is no longer required */
	unlink (transfer->state_path);

	download_free (transfer, FALSE);
}

/*****************************************************************************/

void download_stop (Transfer *transfer, int cancel)
{
	TRACE_FUNC ();

	if (cancel)
	{
		GIFT_WARN (("removing %s", transfer->state_path));
		unlink (transfer->state_path);
	}

	/* TODO - handle cancel */
	download_free (transfer, TRUE);
}

/*****************************************************************************/

static void add_source (Transfer *transfer, Source *source)
{
	List   *ptr;
	Source *lsource;

	/* do not add duplicates */
	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		lsource = ptr->data;

		if (!strcmp (lsource->url, source->url))
			return;
	}

	transfer->sources = list_append (transfer->sources, source);
}

void download_add_source (Transfer *transfer, Source *source)
{
	List  *ptr;
	Chunk *chunk   = NULL;
	Chunk *largest = NULL;
	unsigned long start = 0;
	unsigned long stop  = 0;
	unsigned long diff  = 0;
	unsigned long largest_diff = 0;

	if (!source)
		return;

#ifndef USE_MULTISOURCE
	/* already have a source */
	if (transfer->chunks && transfer->chunks->data &&
		((Chunk *)transfer->chunks->data)->source)
	{
		return;
	}
#endif

	for (ptr = transfer->chunks; ptr; ptr = ptr->next)
	{
		Chunk *tmp = ptr->data;

		/* partitioned chunk w/ a dead source */
		if (!tmp->source)
		{
			tmp->source = source;
			tmp->active = TRUE;

			/* indicate that we need a new chunk */
			chunk = tmp;

			break;
		}

		/* locate the largest available chunk */
		diff = tmp->stop - tmp->start - tmp->transmit;
		if (!largest || diff > largest_diff)
		{
			largest      = tmp;
			largest_diff = diff;
		}
	}

	/* register the source w/ the list */
	add_source (transfer, source);

	/* do not redivide a chunk less than the desired threshold */
	if (largest && largest_diff < DIVIDE_THRESHOLD)
		return;

	if (largest)
	{
		diff = largest_diff / 2;
		stop = largest->stop;
		largest->stop -= diff;
		start = largest->stop;
	}
	else
	{
		stop = transfer->total;
	}

	/* a new division must be made */
	if (!chunk)
	{
		chunk = chunk_new (transfer, source, start, stop);
		chunk->source->p->download (chunk);
	}

	download_handle_change (transfer);
}

/*****************************************************************************/

static void remove_source (Transfer *transfer, char *url)
{
	List   *ptr;
	Source *source;

	for (ptr = transfer->sources; ptr; ptr = list_next (ptr))
	{
		source = ptr->data;

		if (strcmp (source->url, url))
			continue;

		TRACE (("*** removing %s", source->url));
		transfer->sources = list_remove (transfer->sources, source);
		source_free (source);

		return;
	}
}

void download_remove_source (Transfer *transfer, char *url)
{
	List *ptr;

	if (!url)
		return;

	/* search the list of chunks for this source */
	for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
	{
		Chunk *chunk = ptr->data;

		if (!chunk->source)
			continue;

		if (strcmp (chunk->source->url, url))
			continue;

		/* this chunk completed? */
		if (chunk->start + chunk->transmit >= chunk->stop)
		{
			transfer->chunks = list_remove (transfer->chunks, chunk);
			chunk_free (chunk);
		}
		else
		{
			TRACE (("dead segment from %s, %lu - %lu (%lu)",
			        chunk->source->url,
			        chunk->start, chunk->stop, chunk->transmit));

			/* don't remove it from the list of chunks, it is a dead
			 * segment, simply nullify it */
			chunk->source->active = FALSE;
			chunk->source = NULL;
			chunk->active = FALSE;
		}

		download_handle_change (transfer);

		break;
	}

	/* now remove the offending source from the list */
	remove_source (transfer, url);
}

/*****************************************************************************/

static void disable_source (Transfer *transfer, Source *source)
{
	if (!source)
		return;

	source->active = FALSE;

	/* this is a very dirty trick.  download_remove_source will attempt to
	 * free source if found.  remove it from the list here, nullify the
	 * chunk with download_remove_source, then append the source again.
	 * NOTE: either way, the source must be moved to the end of the list even
	 * if the use of download_remove_source goes away */
	transfer->sources = list_remove (transfer->sources, source);
	download_remove_source (transfer, source->url);
	transfer->sources = list_append (transfer->sources, source);
}

/*****************************************************************************/

/* <jasta> * ANGRY FIST * */
static char *uniq_file (Transfer *transfer)
{
	char *uniq;
	unsigned long sec;
	unsigned long usec;
#ifdef WIN32
	SYSTEMTIME st;
#else
	struct timeval tv;
#endif

	if (!transfer || !transfer->filename)
		return NULL;

	if (!(uniq = malloc (strlen (transfer->filename) + 50)))
		return NULL;

	/* god windows conventions are ugly... */
#ifdef WIN32
	GetLocalTime (&st);
	sec = st.wSecond;
	usec = st.wMilliseconds;
#else
	gettimeofday (&tv, NULL);
	sec = tv.tv_sec;
	usec = tv.tv_usec;
#endif

	sprintf (uniq, "%lX%lX%lX.%s", sec % 0xffff, usec,
	         transfer->total, transfer->filename);

	return uniq;
}

static int open_output (Transfer *transfer)
{
	char *uniq;

	/* transfer->path may have already been supplied by a recovered state
	 * file, so we probably shouldnt mess w/ it */
	if (!transfer->path)
	{
		/* create a unique temporary file in the incoming dir...I am strictly
		 * opposed to this but it does actually help out */
		uniq = uniq_file (transfer);

		transfer->path       = STRDUP (gift_conf_path ("incoming/%s", uniq));
		transfer->state_path = STRDUP (gift_conf_path ("incoming/.%s.state", uniq));

		free (uniq);
	}

	/* i'm not really sure why judge did this...if anyone has a problem
	 * with it, blame him */
	if (!(transfer->f = fopen (transfer->path, "r+")) &&
		!(transfer->f = fopen (transfer->path, "w")))
	{
		return FALSE;
	}

	GIFT_DEBUG (("opened %s", transfer->path));

	return TRUE;
}

/*****************************************************************************/

static void save_state (Transfer *transfer)
{
	FILE *f;
	List *ptr;
	int   i = 0;

	/* TODO - update conf.c to have write support */

	if (!(f = fopen (transfer->state_path, "w")))
		return;

	fprintf (f, "[transfer]\n");
	fprintf (f, "filename = %s\n",  transfer->filename);
	fprintf (f, "path = %s\n",      transfer->path);
	fprintf (f, "transmit = %lu\n", transfer->transmit);
	fprintf (f, "total = %lu\n",    transfer->total);
	fprintf (f, "max_seek = %lu\n", transfer->max_seek);
	fprintf (f, "\n");

	for (ptr = transfer->chunks; ptr; ptr = list_next (ptr))
	{
		Chunk *chunk = ptr->data;

		if (!chunk->source)
			continue;

		fprintf (f, "[chunk%i]\n",   i++);
		fprintf (f, "user = %s\n",   chunk->source->user);
		fprintf (f, "hash = %s\n",   chunk->source->hash);
		fprintf (f, "source = %s\n", chunk->source->url);
		fprintf (f, "start = %lu\n", chunk->start + chunk->transmit);
		fprintf (f, "stop = %lu\n",  chunk->stop);
		fprintf (f, "\n");
	}

	fclose (f);
}

/* writes the data to disk */
void download_write (Chunk *chunk, char *segment, size_t len)
{
	Transfer     *transfer  = chunk->transfer;
	unsigned long seek_pos  = chunk->start + chunk->transmit;
	signed long   remainder = chunk->stop - seek_pos;

	/* protocol closed the connection */
	if (!segment || len == 0)
	{
		disable_source (chunk->transfer, chunk->source);

		return;
	}

	if (!chunk->active)
		return;

	/* make sure we don't write too much */
	if (len > remainder)
		len = remainder;

	if (!transfer->f &&
		!open_output (transfer))
	{
		download_free (transfer, TRUE);
		return;
	}

	/* incremental padding -- this is some ugly code here :) */
	if (seek_pos > transfer->max_seek)
	{
		int diff;

		/* seek to the current end */
		if (fseek (transfer->f, transfer->max_seek, SEEK_SET) == -1)
			perror ("fseek");

		/* pad the file with bs data */
		while ((diff = (seek_pos - transfer->max_seek)) > 0)
		{
			int n;

			if (len < diff)
				diff = len;

			n = fwrite (segment, sizeof (char), diff, transfer->f);
			if (n == -1)
			{
				perror ("fwrite");
				fclose (transfer->f);
				transfer->f = NULL;
				return;
			}

			transfer->max_seek += n;
		}
	}

	/* TODO - error handling */
	fseek (transfer->f, seek_pos, SEEK_SET);

	len = fwrite (segment, sizeof (char), MIN (len, remainder), transfer->f);

	chunk->transmit    += len; /* this chunk */
	chunk->tmp_recv    += len; /* temporary transmit recording */
	transfer->transmit += len; /* total transfer stats */

	/* new seek max */
	if (seek_pos == transfer->max_seek)
		transfer->max_seek += len;

	remainder -= len;

	if (remainder <= 0)
	{
		chunk->active = FALSE;

		TRACE (("chunk start (%lu) completed successfully (%lu/%lu)...%lu",
		        chunk->start, chunk->transmit, chunk->stop - chunk->start,
		        remainder));

		/* transfer has completed [successfully] */
		if (transfer->transmit >= transfer->total)
		{
			download_report (transfer);
			download_complete (transfer);

			return;
		}

		/* remove this chunk, re-add the source associated and free the chunk
		 * data */
		transfer->chunks = list_remove (transfer->chunks, chunk);

		download_add_source (transfer, chunk->source);

		/* chunk->source has moved to a new chunk via download_add_source,
		 * set as NULL to prevent it from being freed */
		chunk->source = NULL;
		chunk_free (chunk);

		download_handle_change (transfer);
	}
	else
	{
		save_state (transfer);
	}
}

/*****************************************************************************/

static Transfer *download_read_state (char *file)
{
	Transfer     *transfer;
	Config       *conf;
	Chunk        *chunk;
	char          key_name[256];
	int           chunk_offs;
	char         *filename;
	unsigned long total;

	file = STRDUP (file);

	if (!(conf = config_new (file)))
		return NULL;

	filename =        config_get_str (conf, "transfer/filename");
	total    = ATOUL (config_get_str (conf, "transfer/total"));

	/* make sure it's at least reasonably complete...if they fuck with their
	 * state file too much though, fuck them */
	if (!filename || !total)
	{
		free (file);
		return NULL;
	}

	TRACE (("awaking state transfer %s", filename));

	transfer = download_new (NULL, filename, total);
	transfer->state_path = file;
	transfer->path       = STRDUP (config_get_str (conf, "transfer/path"));
	transfer->transmit   =         config_get_int (conf, "transfer/transmit");
	transfer->total      =         config_get_int (conf, "transfer/total");
	transfer->max_seek   =         config_get_int (conf, "transfer/max_seek");

	/*
	 * YUCK
	 */
	for (chunk_offs = 0 ;; chunk_offs++)
	{
		char *user;
		char *hash;
		char *source;
		unsigned long start;
		unsigned long stop;

		sprintf (key_name, "chunk%i/source", chunk_offs);

		/* check to see if we really have a chunk here or not */
		if (!config_get_str (conf, key_name))
			break;

		source = config_get_str (conf, key_name);

		sprintf (key_name, "chunk%i/start", chunk_offs);
		start = config_get_int (conf, key_name);

		sprintf (key_name, "chunk%i/stop", chunk_offs);
		stop = config_get_int (conf, key_name);

		sprintf (key_name, "chunk%i/user", chunk_offs);
		user = config_get_str (conf, key_name);

		sprintf (key_name, "chunk%i/hash", chunk_offs);
		hash = config_get_str (conf, key_name);

		if (!user || !hash || !source)
		{
			GIFT_DEBUG (("corrupt resume entry: chunk%i", chunk_offs));
			continue;
		}

		/* allocate the chunk structure */
		chunk = chunk_new (transfer, source_new (user, hash, source),
		                   start, stop);

		if (!chunk)
			continue;

		/* this function really wants to call add_source ;) */
		add_source (transfer, chunk->source);

		/* tell the protocol (which was received from source) to finish
		 * downloading this file */
		if (chunk->source->p->download)
			chunk->source->p->download (chunk);
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
	if (!(dir = opendir (gift_conf_path ("incoming/"))))
	{
		perror ("opendir");
		return;
	}

	/* TODO - abstract open/readdir into file.c for
	 * portability/maintainability */
	while ((d = readdir (dir)))
	{
		size_t name_len = strlen (d->d_name);

		/* not long enough for .a.state */
		if (name_len < 8)
			continue;

		/* look for ^. and state$ */
		if (*d->d_name == '.' &&
			!strcmp (d->d_name + (name_len - 5), "state"))
		{
			/* restore this individual download */
			download_read_state (gift_conf_path ("incoming/%s", d->d_name));
		}
	}

	closedir (dir);
}
