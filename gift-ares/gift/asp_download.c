/*
 * $Id: asp_download.c,v 1.12 2005/11/26 14:17:41 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#include "asp_plugin.h"

/*****************************************************************************/

/* Called by ares library for downconn state changes. */
static as_bool dl_state_callback (ASDownConn *conn, ASDownConnState state)
{
	Source *source = conn->udata1;
	SourceStatus status;
	const char *status_str;
	
	switch (state)
	{
	case DOWNCONN_CONNECTING:
		status = SOURCE_WAITING;
		status_str = "Connecting";
		break;
	case DOWNCONN_TRANSFERRING:
		status = SOURCE_ACTIVE;
		status_str = "Active";
		break;
	case DOWNCONN_COMPLETE:
		/* This can happen only if we have received zero bytes.  See 
		 * dl_data_callback. */

		/* fall-through */
	case DOWNCONN_FAILED:
		status = SOURCE_CANCELLED;
		status_str = "Failed";
		break;
	case DOWNCONN_QUEUED:
	{
		status = SOURCE_QUEUED_REMOTE;

		/* Ares sometimes returns a higher queue position than length.
		 * Apparently this is supposed to tell us we a SOL so don't show the
		 * position in this case.
		 */
		if (conn->queue_pos != 0 && conn->queue_len != 0 &&
		    conn->queue_pos <= conn->queue_len)
		{
			status_str = stringf ("Queued (%d of %d)", conn->queue_pos,
			                      conn->queue_len);
		}
		else
			status_str = "Queued";
		break;
	}
	default:
		abort ();
	}

#if 0
	AS_HEAVY_DBG_3 ("Downconn state callback: %p (%s) '%s'", conn,
	                source->url, status_str);
#endif

	/* Update giFT's status. */
	PROTO->source_status (PROTO, source, status, status_str);

	/* Handle failure case in a special way. */
	if (state == DOWNCONN_FAILED || state == DOWNCONN_COMPLETE)
	{
#if 1
		/* Remove failed source. */
		PROTO->source_abort (PROTO, source->chunk->transfer, source);
#else
		/* Keep failed source but tell giFT some went wrong. */
		PROTO->chunk_write (PROTO, source->chunk->transfer, source->chunk,
	                        source, NULL, 0);
#endif
		/* The above will make giFT call asp_giftcb_download_stop or
		 * asp_giftcb_source_remove which both free downconn so return
		 * FALSE here.
		 */
		return FALSE;
	}

	return TRUE; /* Downconn was not freed. */
}

/* Called by ares library for received data. */
static as_bool dl_data_callback (ASDownConn *conn, as_uint8 *data,
                                 unsigned int len)
{
	Source *source = conn->udata1;

	/* This will make giFT call asp_giftcb_download_stop and/or 
	 * asp_giftcb_source_remove if the chunk is complete.
	 */
	PROTO->chunk_write (PROTO, source->chunk->transfer, source->chunk,
	                    source, data, len);

	/* HACK: ASDownConn will access the downconn if we return TRUE. But this
	 * may have already been freed when giFT called us back for complete
	 * chunks. Since we have no way to determine if this happened we always
	 * return FALSE. This has the consequence that DOWNCONN_COMPLETE is never
	 * triggered.
	 */
	return FALSE;
}

/*****************************************************************************/

/* Called by gift to start downloading of a chunk. */
BOOL asp_giftcb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
                                Source *source)
{
	ASDownConn *dc = source->udata;
	ASHash *hash;
	assert (dc);

	/* Create hash object. */
	if (strcasecmp (hashstr_algo (source->hash), "SHA1") ||
	    !(hash = asp_hash_decode (hashstr_data (source->hash))))
	{
		AS_WARN_1 ("Malformed source hash '%s'.", source->hash);
		return FALSE;
	}

#ifdef EVIL_HASHMAP
	/* Insert into the evil hash map by reading stuff we shouldn't
	   from giFT's internal structures.  (It's not called evil for
	   nothing, y'know...)
	*/
	asp_hashmap_insert (hash, transfer->filename, transfer->total);
#endif

	/* Start transfer. */
	if (!as_downconn_start (dc, hash, chunk->start + chunk->transmit,
	                        chunk->stop - chunk->start - chunk->transmit))
	{
		AS_ERR_1 ("Failed to start downconn for '%s'.", source->url);
		as_hash_free (hash);
		return FALSE;
	}

	as_hash_free (hash);

	AS_HEAVY_DBG_3 ("Started downconn [%d,%d) from '%s'.", 
	                chunk->start + chunk->transmit, chunk->stop, source->url);

	/* Update giFT's source status. */
	PROTO->source_status (PROTO, source, SOURCE_WAITING, "Connecting");

	return TRUE;
}

/* Called by gift to stop download. */
void asp_giftcb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
                               Source *source, int complete)
{
	ASDownConn *dc;

	/* You will probably not believe this but giFT may call this for a given
	 * chunk with a totally random source (see cancel_sources in download.c).
	 * I am so sick of this crap.
	 */
	if (!chunk->source)
		return;

	/* This may be NULL because giFT sometimes removes the source before
	 * stopping the download. I am not kídding you.
	 */
	if (!(dc = chunk->source->udata))
		return;

	/* This will always close the underlying tcp connection which means that
	 * even if the chunk was complete we will not reuse the connection.
	 */
	as_downconn_cancel (dc);
}

/* Called by gift when a source is added to a download. */
BOOL asp_giftcb_source_add (Protocol *p,Transfer *transfer, Source *source)
{
	ASSource *s;
	ASDownConn *dc;

	/* Create downconn object for this new source. */
	assert (source->udata == NULL);
	assert (source->url);

	/* Create source object from URL. */
	if (!(s = as_source_unserialize (source->url)))
	{	
		AS_WARN_1 ("Malformed source url '%s'.", source->url);
		return FALSE; /* Makes giFT drop the source. */
	}
	
	/* Create the ares download connection object. */
	dc = as_downconn_create (s, (ASDownConnStateCb)dl_state_callback,
	                         (ASDownConnDataCb)dl_data_callback);

	as_source_free (s);

	if (!dc)
	{	
		AS_ERR_1 ("Failed to create downconn from '%s'.", source->url);
		return FALSE;
	}

	source->udata = dc;
	dc->udata1 = source;

	return TRUE;
}

/* Called by gift when a source is removed from a download. */
void asp_giftcb_source_remove (Protocol *p, Transfer *transfer,
                               Source *source)
{
	ASDownConn *dc = source->udata;
	assert (dc);

	/* Free our downconn object. */
	as_downconn_cancel (dc);
	as_downconn_free (dc);
	source->udata = NULL;
}

/*****************************************************************************/
