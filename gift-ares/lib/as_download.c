/*
 * $Id: as_download.c,v 1.28 2004/11/06 18:08:17 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

#ifdef WIN32
#include <io.h> /* _chsize and friends */
#endif

/*****************************************************************************/

/* Interval of maintenance timer used to check on queued sources and save
 * download state. */
#define MAINTENANCE_TIMER_INTERVAL (30 * SECONDS)

/* If defined failed downloads will not be deleted */
#define KEEP_FAILED

/* Define to get very verbose chunk logging */
/* #define CHUNK_DEBUG */

/* Define to verify that connection list is sorted correctly  */
/* #define VERIFY_CONN_LIST */

/*****************************************************************************/

static int active_conns_from_list (ASDownload *dl, int *connecting);
static void stop_all_connections (ASDownload *dl);

static as_bool conn_state_cb (ASDownConn *conn, ASDownConnState state);

static as_bool conn_data_cb (ASDownConn *conn, as_uint8 *data,
                             unsigned int len);

static as_bool verify_chunks (ASDownload *dl);
static as_bool consolidate_chunks (ASDownload *dl);
static void download_maintain (ASDownload *dl);
static as_bool maintenance_timer_func (ASDownload *dl);

static char *get_available_filename (const char *filename);
static as_bool download_failed (ASDownload *dl);
static as_bool download_complete (ASDownload *dl);
static as_bool download_finished (ASDownload *dl);

#ifdef HEAVY_DEBUG
static void dump_chunks (ASDownload *dl);
static void dump_connections (ASDownload *dl);
#endif

#ifdef VERIFY_CONN_LIST
static void verify_connections (ASDownload *dl);
#endif

/*****************************************************************************/

static as_bool download_set_state (ASDownload *dl, ASDownloadState state,
                                   as_bool raise_callback)
{
	dl->state = state;

	/* start maintenance timer if active, remove otherwise */
	if (dl->state == DOWNLOAD_ACTIVE)
	{
		assert (dl->maintenance_timer == INVALID_TIMER);
		dl->maintenance_timer = timer_add (MAINTENANCE_TIMER_INTERVAL,
		                           (TimerCallback)maintenance_timer_func, dl);
	}
	else if (dl->maintenance_timer != INVALID_TIMER)
	{
		timer_remove_zero (&dl->maintenance_timer);
	}

	/* raise callback if specified */
	if (raise_callback && dl->state_cb)
		return dl->state_cb (dl, dl->state);

	return TRUE;
}

/*****************************************************************************/

/* Create download. */
ASDownload *as_download_create (ASDownloadStateCb state_cb)
{
	ASDownload *dl;

	if (!(dl = malloc (sizeof (ASDownload))))
		return NULL;

	dl->hash     = NULL;
	dl->filename = NULL;
	dl->path     = NULL;
	dl->size     = 0;
	dl->received = 0;
	dl->fp       = NULL;

	dl->conns  = NULL;
	dl->chunks = NULL;

	dl->maintenance_timer = INVALID_TIMER;
	dl->search = NULL;

	dl->state    = DOWNLOAD_NEW;
	dl->state_cb = state_cb;
	
	dl->downman = NULL;
	dl->meta = NULL;
	dl->udata = NULL;

	return dl;
}

/* Stop and free download. */
void as_download_free (ASDownload *dl)
{
	List *l;

	if (!dl)
		return;

	/* Cancel all active connections. */
	stop_all_connections (dl);

	/* Make sure any source search is gone */
	if (dl->search)
		as_searchman_remove (AS->searchman, dl->search);

	as_hash_free (dl->hash);
	free (dl->path);
	if (dl->fp)
		fclose (dl->fp);

	for (l = dl->conns; l; l = l->next)
		as_downconn_free (l->data);
	list_free (dl->conns);

	for (l = dl->chunks; l; l = l->next)
		as_downchunk_free (l->data);
	list_free (dl->chunks);

	timer_remove (dl->maintenance_timer);

	as_meta_free (dl->meta);
	free (dl);
}

/*****************************************************************************/

/* Start download using hash, filesize and save name. */
as_bool as_download_start (ASDownload *dl, ASHash *hash, size_t filesize,
                           const char *save_path)
{
	ASDownChunk *chunk;
	char *incomplete, *dir;

	if (dl->state != DOWNLOAD_NEW)
	{
		assert (dl->state == DOWNLOAD_NEW);
		return FALSE;
	}

	if (!hash || !save_path || filesize == 0)
	{
		assert (hash);
		assert (save_path);
		assert (filesize > 0);
		return FALSE;
	}

	/* create incomplete file path */
	dir = gift_strndup (save_path, as_get_filename (save_path) - save_path);
	incomplete = stringf_dup ("%s%s%s", dir ? dir : "", AS_DOWNLOAD_INCOMPLETE_PREFIX,
	                          as_get_filename (save_path));
	free (dir);

	/* find a name which is not used already */
	if (!(dl->path = get_available_filename (incomplete)))
	{
		AS_ERR_1 ("Couldn't find available file name for download \"%s\"",
		          dl->path);
		free (incomplete);
		return FALSE;
	}
	free (incomplete);

	/* set filename after __ARESTRA__prefix */
	dl->filename = as_get_filename (dl->path);
	if (strncmp (dl->filename, AS_DOWNLOAD_INCOMPLETE_PREFIX,
	             strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX)) == 0)
		dl->filename += strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX);

	/* open file */
	if (!(dl->fp = fopen (dl->path, "w+b")))
	{
		AS_ERR_1 ("Unable to open download file \"%s\" for writing",
		          dl->path);
		free (dl->path);
		dl->path = NULL;
		dl->filename = NULL;
		return FALSE;
	}

	/* create one initial chunk for entire file */
	dl->size = filesize;

	if (!(chunk = as_downchunk_create (0, dl->size)))
	{
		AS_ERR_1 ("Couldn't create initial chunk (0,%u)", dl->size);
		free (dl->path);
		dl->path = NULL;
		dl->filename = NULL;
		dl->size = 0;
		return FALSE;
	}

	dl->chunks = list_prepend (dl->chunks, chunk);

	/* copy hash */
	dl->hash = as_hash_copy (hash);

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_ACTIVE, TRUE))
		return FALSE;

	/* Immediately save state data so we have something to recover from. */
	if (!as_downstate_save (dl))
	{
		AS_ERR_1 ("Failed to write state data for \"%s\". Pausing.",
		          dl->filename);
		as_download_pause (dl);
		/* return TRUE because the download setup has succeeded */
		return TRUE;
	}

	/* start things off if we are still in active state */
	if (dl->state == DOWNLOAD_ACTIVE)
		download_maintain (dl);

	return TRUE;
}

/* Restart download from incomplete ___ARESTRA___ file. This will fail if the
 * file is not found/corrupt/etc.
 */
as_bool as_download_restart (ASDownload *dl, const char *path)
{
	if (dl->state != DOWNLOAD_NEW)
	{
		assert (dl->state == DOWNLOAD_NEW);
		return FALSE;
	}

	if (!path)
	{
		assert (path);
		return FALSE;
	}

	/* copy file path */
	dl->path = strdup (path);

	/* set filename after __ARESTRA__prefix */
	dl->filename = as_get_filename (dl->path);
	if (strncmp (dl->filename, AS_DOWNLOAD_INCOMPLETE_PREFIX,
		         strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX)) == 0)
		dl->filename += strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX);

	/* make sure the file exists */
	if (!as_file_exists (dl->path))
	{
		AS_ERR_1 ("Incomplete file \"%s\" does not exist.", dl->path);
		free (dl->path);
		dl->path = NULL;
		dl->filename = NULL;
		return FALSE;
	}

	/* open file */
	if (!(dl->fp = fopen (dl->path, "r+b")))
	{
		AS_ERR_1 ("Unable to open download file \"%s\" for writing",
		          dl->path);
		free (dl->path);
		dl->path = NULL;
		dl->filename = NULL;
		return FALSE;
	}

	/* read download state from file */
	if (!as_downstate_load (dl))
	{
		AS_ERR_1 ("Unable to load state for incomplete download file \"%s\"",
		          dl->path);
		fclose (dl->fp);
		dl->fp = NULL;
		free (dl->path);
		dl->path = NULL;
		dl->filename = NULL;
		return FALSE;	
	}
	
	AS_HEAVY_DBG_3 ("Loaded state for \"%s\", size: %u, received: %u",
	                dl->filename, dl->size, dl->received);

#ifdef HEAVY_DEBUG
	assert (verify_chunks (dl));

	AS_HEAVY_DBG ("Chunk state after restoring download:");
	dump_chunks (dl);

	AS_HEAVY_DBG ("Connection state after restoring download:");
	dump_connections (dl);
#endif

	/* raise callback with state set by as_downstate_load */
	if (!download_set_state (dl, dl->state, TRUE))
		return FALSE;

	/* start things off if we are in active state */
	if (dl->state == DOWNLOAD_ACTIVE)
		download_maintain (dl);

	return TRUE;
}

/* Cancels download and removes incomplete file. */
as_bool as_download_cancel (ASDownload *dl)
{
	if (dl->state != DOWNLOAD_ACTIVE && dl->state != DOWNLOAD_QUEUED &&
	    dl->state != DOWNLOAD_PAUSED)
	{
		return FALSE;
	}

	AS_DBG_1 ("Cancelling download \"%s\"", dl->filename);

	/* stop all connections */
	stop_all_connections (dl);

	/* Make sure any source search is gone */
	if (dl->search)
		as_searchman_remove (AS->searchman, dl->search);
	dl->search = NULL;

	/* close fd */
	if (dl->fp)
	{
		fclose (dl->fp);
		dl->fp = NULL;
	}

	/* delete incomplete file. */
	if (unlink (dl->path) < 0)
		AS_ERR_1 ("Failed to unlink incomplete file \"%s\"", dl->path);

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_CANCELLED, TRUE))
		return FALSE;

	return TRUE;
}

/* Pause download */
as_bool as_download_pause (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_PAUSED)
		return TRUE;

	if (dl->state != DOWNLOAD_ACTIVE && dl->state != DOWNLOAD_QUEUED)
		return FALSE;

	AS_DBG_1 ("Pausing download \"%s\"", dl->filename);

	/* Stop all active chunk downloads. */
	stop_all_connections (dl);

	/* Clean up chunks so state is saved with only completely full or empty
	 * chunks
	 */
	if (!consolidate_chunks (dl))
	{
		AS_ERR_1 ("Consolidating chunks failed on pausing for \"%s\"",
		          dl->filename);
		/* Fail download */
		download_failed (dl);
		assert (0);
		return FALSE;
	}

	/* Set state to paused so it is saved in file. */
	download_set_state (dl, DOWNLOAD_PAUSED, FALSE);
	
	/* Save state data. */
	if (!as_downstate_save (dl))
	{
		AS_ERR_1 ("Failed to write state data on pausing for \"%s\"",
		          dl->filename);
		/* Fall through and complete pause */
	}

	/* Raise callback after saving so not matter what the callback does the
	 * state data is on disk.
	 */
	if (!download_set_state (dl, DOWNLOAD_PAUSED, TRUE))
		return FALSE;

	return TRUE;
}

/* Queue download locally (effectively pauses it) */
as_bool as_download_queue (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_QUEUED)
		return TRUE;

	if (dl->state != DOWNLOAD_ACTIVE && dl->state != DOWNLOAD_PAUSED)
		return FALSE;

	AS_DBG_1 ("Queuing download \"%s\"", dl->filename);

	/* Stop all active chunk downloads. */
	stop_all_connections (dl);

	/* Clean up chunks so state is saved with only completely full or empty
	 * chunks
	 */
	if (!consolidate_chunks (dl))
	{
		AS_ERR_1 ("Consolidating chunks failed on queued for \"%s\"",
		          dl->filename);
		/* Fail download */
		download_failed (dl);
		assert (0);
		return FALSE;
	}

	/* Save state data. */
	if (!as_downstate_save (dl))
	{
		AS_ERR_1 ("Failed to write state data on queued for \"%s\"",
		          dl->filename);
		/* Pause the download instead */
		as_download_pause (dl);
		return FALSE;
	}

	if (!download_set_state (dl, DOWNLOAD_QUEUED, TRUE))
		return FALSE;

	return TRUE;
}

/* Resume download from paused or queued state */
as_bool as_download_resume (ASDownload *dl)
{
	if (dl->state == DOWNLOAD_ACTIVE)
		return TRUE;

	if (dl->state != DOWNLOAD_PAUSED && dl->state != DOWNLOAD_QUEUED)
		return FALSE;

	/* Activate chunk downloads */
	if (!download_set_state (dl, DOWNLOAD_ACTIVE, TRUE))
		return FALSE;

	/* start things off if we are still in active state */
	if (dl->state == DOWNLOAD_ACTIVE)
		download_maintain (dl);

	return TRUE;
}

/* Returns current download state */
ASDownloadState as_download_state (ASDownload *dl)
{
	return dl->state;
}

/*****************************************************************************/

/* Used to sort connection list by b/w */
static int conn_cmp_func (ASDownConn *a, ASDownConn *b)
{
	/* First put sources with chunk at the back of the list */
	if (a->udata2 && !b->udata2)
		return 1;
	else if (!a->udata2 && b->udata2)
		return -1;

	/* Then sort by bandwidth by historical speed */
	if (as_downconn_hist_speed (a) < as_downconn_hist_speed (b))
		return 1;
	else if (as_downconn_hist_speed (a) > as_downconn_hist_speed (b))
		return -1;

	return 0;
}

/* Remove connection from current position in connection list and move it to
 * new correct position according to conn_cmp_func. O(2n).
 */
static void conn_reinsert (ASDownload *dl, ASDownConn *conn)
{
	List *link;

	/* Find link. */
	link = list_find (dl->conns, conn);
	assert (link);

	/* Remove entry. */
	dl->conns = list_remove_link (dl->conns, link);

	/* Reinsert at correct pos. */
	dl->conns = list_insert_sorted (dl->conns, (CompareFunc) conn_cmp_func,
	                                conn);

#ifdef VERIFY_CONN_LIST
	verify_connections (dl);
#endif
}

/* Add source to download (copies source). */
as_bool as_download_add_source (ASDownload *dl, ASSource *source)
{	
	List *l;
	ASDownConn *conn;

	/* Sources can be added in any state but we need to make sure there are
	 * no duplicates
	 */
	for (l = dl->conns; l; l = l->next)
	{
		conn = l->data;
		if (as_source_equal (conn->source, source))
		{
			AS_DBG_1 ("Source \"%s\" already added.", as_source_str (source));
			return FALSE;
		}
	}

	/* create new connection and add it */
	if (!(conn = as_downconn_create (source, conn_state_cb, conn_data_cb)))
		return FALSE;

	/* point udata1 to download, we will use udata2 for the chunk */
	conn->udata1 = dl;

	/* Insert sorted. */
	dl->conns = list_insert_sorted (dl->conns, (CompareFunc) conn_cmp_func,
	                                conn);

	/* check if the source should be used now */
	if (dl->state == DOWNLOAD_ACTIVE)
		download_maintain (dl);

	return TRUE;
}

/*****************************************************************************/

static void search_result_cb (ASSearch *search, ASResult *result,
                              as_bool duplicate)
{
	ASDownload *dl = search->udata;

	if (!result)
	{
		/* Search finished. */
		if (!as_searchman_remove (AS->searchman, search))
		{
			AS_ERR_1 ("Couldn't remove finished source search for download \"%s\"",
			          dl->filename);
		}

		dl->search = NULL;

		AS_DBG_1 ("Finished source search for download \"%s\"", dl->filename);
		return;
	}

	/* Make sure this result has correct hash */
	if (!as_hash_equal (result->hash, dl->hash))
	{
		AS_WARN_1 ("Ignoring source result with wrong hash for download \"%s\"",
		           dl->filename);
		return;
	}

	/* Add source do download */
	if (!as_download_add_source (dl, result->source))
		return;

	AS_HEAVY_DBG_3 ("Added hash result source %s:%d to download \"%s\"",
	                net_ip_str (result->source->host), result->source->port,
	                dl->filename);
}

/* Start a source search for this download */
as_bool as_download_find_sources (ASDownload *dl)
{
	if (dl->search)
		return TRUE; /* we are already looking for sources */

	if (!(dl->search = as_searchman_locate (AS->searchman, search_result_cb,
	                                        dl->hash)))
	{
		AS_ERR_1 ("Couldn't start hash search for download \"%s\"",
		          dl->filename);
		return FALSE;
	}

	dl->search->udata = dl;
	dl->search->intern = TRUE;

	AS_DBG_1 ("Started source search for download \"%s\"", dl->filename);

	return TRUE;
}

/*****************************************************************************/

/* Return download state as human readable static string. */
const char *as_download_state_str (ASDownload *dl)
{
	switch (dl->state)
	{
	case DOWNLOAD_INVALID:   return "Invalid";
	case DOWNLOAD_NEW:       return "New";
	case DOWNLOAD_ACTIVE:    return "Active";
	case DOWNLOAD_QUEUED:    return "Queued";
	case DOWNLOAD_PAUSED:    return "Paused";
	case DOWNLOAD_COMPLETE:  return "Completed";
	case DOWNLOAD_FAILED:    return "Failed";
	case DOWNLOAD_CANCELLED: return "Cancelled";
	case DOWNLOAD_VERIFYING: return "Verifying";
	}
	return "UNKNOWN";
}

/*****************************************************************************/

/* Returns number of active and connecting downloads by traversing connection
 * list.
 */
static int active_conns_from_list (ASDownload *dl, int *connecting)
{
	List *conn_l;
	ASDownConn *conn;
	int active = 0, conning = 0;


	for (conn_l = dl->conns; conn_l; conn_l = conn_l->next)
	{
		conn = conn_l->data;
		if (conn->state == DOWNCONN_TRANSFERRING)
			active++;
		if (conn->state == DOWNCONN_CONNECTING)
			conning++;
	}

	if (connecting)
		*connecting = conning;

	return active;
}

/* Cancels connections and disassociates them from chunks */
static void stop_all_connections (ASDownload *dl)
{
	List *conn_l;
	ASDownChunk *chunk;
	ASDownConn *conn;

	/* loop through connections so we can also close those which are
	 * persistent but not currently associated with a chunk.
	 */
	for (conn_l = dl->conns; conn_l; conn_l = conn_l->next)
	{
		conn = conn_l->data;

		/* cancel connection */
		as_downconn_cancel (conn);

		/* if there is a chunk disassociate it */
		if ((chunk = conn->udata2))
		{
			conn->udata2 = NULL;
			chunk->udata = NULL;
		}
	}

	/* Resort entire list. */
	dl->conns = list_sort (dl->conns, (CompareFunc) conn_cmp_func);
}

/*****************************************************************************/

static as_bool disassociate_conn (ASDownConn *conn)
{
	ASDownload *dl = conn->udata1;
	ASDownChunk *chunk = conn->udata2;

	/* disassociate chunk */
	conn->udata2 = NULL;
	chunk->udata = NULL;

	/* remove connection if it failed too often */
	if (conn->fail_count >= AS_DOWNLOAD_SOURCE_MAX_FAIL)
	{
		AS_DBG_3 ("Removing source %s:%d after it failed %d times",
		          net_ip_str (conn->source->host), conn->source->port,
		          conn->fail_count);

		/* remove connection */
		as_downconn_free (conn);
		dl->conns = list_remove (dl->conns, conn);
		return FALSE;
	}

	/* Reinsert at correct pos. */
	conn_reinsert (dl, conn);

	return TRUE;
}

/* Called for every state change. Return FALSE if the connection was freed and
 * must no longer be accessed.
 */
static as_bool conn_state_cb (ASDownConn *conn, ASDownConnState state)
{
	ASDownload *dl = conn->udata1;
	ASDownChunk *chunk = conn->udata2;

	assert (chunk);
	assert (dl);

	switch (state)
	{
	case DOWNCONN_UNUSED:
		break;

	case DOWNCONN_CONNECTING:
		/* This is triggered by as_downconn_start called in start_chunks. The
		 * following limitations apply:
		 *   - Chunk used by connection may not yet be in chunk list.
		 *   - Connection list may not be sorted to reflect the new use of
		 *     the connection yet.
		 */
		AS_HEAVY_DBG_4 ("DOWNCONN_CONNECTING: %s:%d for chunk (%u,%u).",
		                net_ip_str (conn->source->host), conn->source->port,
		                chunk->start, chunk->size);
		break;

	case DOWNCONN_TRANSFERRING:
		AS_HEAVY_DBG_4 ("DOWNCONN_TRANSFERRING: chunk (%u,%u) from %s:%d.", chunk->start,
		                chunk->size, net_ip_str (conn->source->host),
		                conn->source->port);

		/* Abort transfer if we already have enough sources for download */
		if (active_conns_from_list (dl, NULL) >=
		    AS_CONF_INT (AS_DOWNLOAD_MAX_ACTIVE_SOURCES))
		{
			AS_HEAVY_DBG_2 ("Closing connection to %s:%d because we already have enough sources.",
			                net_ip_str (conn->source->host),
			                conn->source->port);

			as_downconn_cancel (conn);

			/* disassociate chunk */
			conn->udata2 = NULL;
			chunk->udata = NULL;

			/* Reinsert at correct pos. */
			conn_reinsert (dl, conn);

			/* no need to call download_maintain here */
			return FALSE;
		}

		break;

	case DOWNCONN_COMPLETE:
		/* This happens if remote closes connection or number of requested
		 * bytes are read.
		 */
		assert (chunk->received <= chunk->size);

		AS_HEAVY_DBG_4 ("DOWNCONN_COMPLETE: Chunk (%u,%u), conn %s:%d.",
		                chunk->start, chunk->size,
		                net_ip_str (conn->source->host), conn->source->port);

		disassociate_conn (conn);
		download_maintain (dl);
		break;

	case DOWNCONN_FAILED:
		AS_HEAVY_DBG_4 ("DOWNCONN_FAILED: Chunk (%u,%u), conn %s:%d.",
		                chunk->start, chunk->size,
		                net_ip_str (conn->source->host), conn->source->port);

		disassociate_conn (conn);
		download_maintain (dl);
		break;

	case DOWNCONN_QUEUED:

		AS_HEAVY_DBG_4 ("DOWNCONN_QUEUED: %s:%d, pos: %d, length: %d.",
		                net_ip_str (conn->source->host), conn->source->port,
		                conn->queue_pos, conn->queue_len);

		disassociate_conn (conn);
		download_maintain (dl);
		break;
	}

	return TRUE;
}

/* Called for every piece of data downloaded. Return FALSE if the connection
 * was freed and must no longer be accessed.
 */
static as_bool conn_data_cb (ASDownConn *conn, as_uint8 *data,
                             unsigned int len)
{
	ASDownload *dl = conn->udata1;
	ASDownChunk *chunk = conn->udata2;
	size_t write_len;

	assert (len > 0);
	assert (chunk);
	assert (dl);
	assert (dl->fp);
	assert (chunk->size - chunk->received <= conn->chunk_size);

	/* seek to correct place in file */
	if (fseek (dl->fp, chunk->start + chunk->received, SEEK_SET) != 0)
	{
		AS_ERR_1 ("Seek failed for download \"%s\". Pausing.", dl->filename);
		as_download_pause (dl);	
		return FALSE;
	}

	/* make sure we are not writing past the chunk end */
	write_len = len;

	if (write_len > chunk->size - chunk->received)
	{
		write_len = chunk->size - chunk->received;
		AS_HEAVY_DBG_1 ("Got more data than needed for chunk, truncated to %u.",
		                len);
	}

	/* write the data */
	if (fwrite (data, 1, write_len, dl->fp) != write_len)
	{
		AS_ERR_1 ("Write failed for download \"%s\". Pausing.", dl->filename);
		as_download_pause (dl);	
		return FALSE;
	}

	dl->received += write_len;
	chunk->received += write_len;
	assert (chunk->received <= chunk->size);
	assert (dl->received <= dl->size);

	/* chunk complete? */
	if (chunk->received == chunk->size)
	{
		AS_HEAVY_DBG_4 ("Chunk (%u,%u) from %s:%d complete.", chunk->start,
		                chunk->size, net_ip_str (conn->source->host),
		                conn->source->port);

		/* Cancel connection if we cannot keep it open because there we got
		 * or requested more data than the chunk needs. Otherwise just leave
		 * and let DOWNCONN_COMPLETE reuse the connection.
		 */
		if (len != write_len ||
		    conn->chunk_start + conn->chunk_size > chunk->start + chunk->size)
		{
			AS_HEAVY_DBG_2 ("Cancelling connection to %s:%d because chunk is"
			                "complete before transfer end.",
			                net_ip_str (conn->source->host),
			                conn->source->port);

			as_downconn_cancel (conn);

			/* disassociate chunk */
			disassociate_conn (conn);

			/* clean up / start new connections / etc */
			download_maintain (dl);

			/* don't return TRUE after the connection might have been reused */
			return FALSE;
		}
	}

	return TRUE;
}

/*****************************************************************************/

/* Verify chunk list is consistent */
static as_bool verify_chunks (ASDownload *dl)
{
	List *link;
	ASDownChunk *chunk, *next_chunk;

	link = dl->chunks; 

	/* there must always be at least one chunk */
	if (!link)
	{
		AS_ERR ("Chunk list empty.");
		return FALSE;
	}

	while (link)
	{
		chunk = link->data;

		/* chunk cannot have received more than its size  */
		if (chunk->received > chunk->size)
		{
			AS_ERR_2 ("Chunk received more than its size."
			          "size: %u, received: %u",
			          chunk->size, chunk->received);
			return FALSE;
		}

		/* complete chunks must be without connection */
		if (chunk->received == chunk->size && chunk->udata != NULL)
		{
			AS_ERR ("Complete chunk still associated with connection");
			return FALSE;
		}

		/* next chunk must begin at end of this chunk or it must be the end of
		 * the file.
		 */
		if (link->next)
		{
			next_chunk = link->next->data;

			if (chunk->start + chunk->size != next_chunk->start)
			{
				AS_ERR_2 ("Start of next chunk is %u, should be %u.",
				          next_chunk->start, chunk->start + chunk->size);
				return FALSE;
			}
		}
		else
		{
			if (chunk->start + chunk->size != dl->size)
			{
				AS_ERR_2 ("Last chunk ends at %u but file size is %u",
				          chunk->start + chunk->size, dl->size);
				return FALSE;
			}
		}

		link = link->next;
	}

	return TRUE;
}

/* This mangels chunks as follows:
 *   - Leave active chunks alone
 *   - Split half complete chunks in a complete and an empty chunk
 *   - Merge consecutive complete chunks
 *   - Merge consecutive empty chunks
 */
static as_bool consolidate_chunks (ASDownload *dl)
{
	List *link, *last_link, *new_link;
	ASDownChunk *chunk, *last_chunk, *new_chunk;
	size_t new_start, new_size;

	link = dl->chunks;
	last_link = NULL;
	last_chunk = NULL;

	while (link)
	{
		chunk = link->data;

		/* Split chunk if it is not active and half complete */
		if (!chunk->udata &&
		    chunk->received > 0 &&
		    chunk->received < chunk->size)
		{
			AS_HEAVY_DBG_3 ("Splitting half complete chunk (%u, %u, %u)",
			                chunk->start, chunk->size, chunk->received);

			/* create the new empty chunk */
			new_start = chunk->start + chunk->received;
			new_size  = chunk->size - chunk->received;

			if (!(new_chunk = as_downchunk_create (new_start, new_size)))
			{
				/* This is not recoverable if we rely on chunk downloads always
				 * beginning at chunk->received == 0 so quit here.
				 */
				AS_ERR_2 ("Couldn't create chunk (%u,%u)", new_start, new_size);
				return FALSE;						
			}

			/* Shorten old chunk. */
			chunk->size = chunk->received;

			/* Insert new chunk after old one. */
			new_link = list_prepend (NULL, new_chunk);
			list_insert_link (link, new_link);

			assert (chunk->received == chunk->size);
			assert (new_chunk->received == 0);

			/* Adjust last_link and link. */
			last_link = link;
			link = new_link;

			last_chunk = last_link->data;
			chunk = link->data;
		}

		/* Merge last_chunk and chunk if they are either both complete or both
		 * empty.
		 */
		if (last_chunk && !chunk->udata && !last_chunk->udata &&
		    ((last_chunk->received == last_chunk->size &&
		     chunk->received == chunk->size) ||
			(last_chunk->received == 0 && chunk->received == 0)))
		{	
			AS_HEAVY_DBG_5 ("Merging %s chunks (%u,%u) and (%u,%u)",
			                (chunk->received == 0) ? "empty" : "complete",
			                last_chunk->start, last_chunk->size,
			                chunk->start, chunk->size);

			/* increase last chunk by size of chunk */
			last_chunk->size += chunk->size;
			last_chunk->received += chunk->received;

			/* free chunk and remove link */
			as_downchunk_free (chunk);
			last_link = list_remove_link (last_link, link);

			/* adjust link so loop can continue */
			link = last_link;
		}

		/* Move on to next chunk */
		last_link = link;
		link = link->next;
		last_chunk = last_link->data;
	};

	return TRUE;
}

/* Assign new connections to unused chunks. Split large chunks if there are
 * unused connections. Since our connection list is sorted by bandwidth this
 * will pick fast connections first.
 */
static as_bool start_chunks (ASDownload *dl)
{
	List *chunk_l;
	List *conn_l, *tmp_l, *slow_conn_l;
	ASDownChunk *chunk, *new_chunk;
	ASDownConn *conn, *slow_conn;
	time_t now = time (NULL);
	size_t remaining, new_start, new_size;
	int active_conns, pending_conns, max_new_conns;
	unsigned int speed;

	if (!(conn_l = dl->conns))
		return TRUE; /* nothing to do */

	/* We do nothing if there are already enough connections. If not we limit
	 * the number of our attempts to (maximum - active) * 2, assuming half of
	 * the connection attempts will fail.
	 * TODO: Is 2 a good factor? Does it reflect the real failure rate?
	 */
	active_conns = active_conns_from_list (dl, &pending_conns);

	if (active_conns >= AS_CONF_INT (AS_DOWNLOAD_MAX_ACTIVE_SOURCES))
		return TRUE;

	max_new_conns = (AS_CONF_INT (AS_DOWNLOAD_MAX_ACTIVE_SOURCES) -
	                 active_conns) * 2;

	if (pending_conns >= max_new_conns)
		return TRUE;

	max_new_conns -= pending_conns;

	AS_HEAVY_DBG_1 ("Attempting a maximum of %d new connections",
	                max_new_conns);

	/* First assign connections to unused chunks */

	for (chunk_l = dl->chunks; chunk_l; chunk_l = chunk_l->next)
	{
		chunk = chunk_l->data;

		/* skip active and complete chunks */
		if (chunk->udata || chunk->received == chunk->size)
			continue;
		
		/* find a connection for this unused chunk */
		while (conn_l)
		{
			conn = conn_l->data;

			if (!conn->udata2 &&
			    (conn->state != DOWNCONN_QUEUED || conn->queue_next_try <= now))
			{
				/* associate chunk with connection */
				chunk->udata = conn;
				conn->udata2 = chunk;

				/* We always split half complete chunks in consolidate_chunks
				 * before starting them.
				 */
				assert (chunk->received == 0);

				/* use the connection we found with this chunk */
				if (!as_downconn_start (conn, dl->hash, chunk->start,
				                        chunk->size))
				{
					/* download start failed, remove connection. */
					AS_DBG_2 ("Failed to start download from %s:%d, removing source.",
					          net_ip_str (conn->source->host),
					          conn->source->port);

					as_downconn_free (conn);
					chunk->udata = NULL;

					tmp_l = conn_l->next;
					dl->conns = list_remove_link (dl->conns, conn_l);
					conn_l = tmp_l;

					/* try next connection */
					continue;
				}

				max_new_conns--;

				AS_HEAVY_DBG_5 ("Started unused chunk (%u,%u) of \"%s\" with %s:%d",
			                    chunk->start, chunk->size, dl->filename,
				                net_ip_str (conn->source->host),
				                conn->source->port);

				/* move on to next chunk */
				conn_l = conn_l->next;
				break;
			}

			conn_l = conn_l->next;
		}

		if (!conn_l || max_new_conns <= 0)
			break; /* no more suitable connections */
	}

	/* Now loop through remaining connections and create new chunks for them
	 * by splitting large ones.
	 */
	while (conn_l && max_new_conns > 0)
	{
		conn = conn_l->data;

		/* if we see a used connection we can stop since there are no more
		 * unused ones
		 */
		if (conn->udata2)
			break;

		/* skip queued connections */
		if (conn->state == DOWNCONN_QUEUED && conn->queue_next_try > now)
		{
			conn_l = conn_l->next;
			continue;
		}

		/* Find chunk with largest remaining size */
		remaining = 0;
		tmp_l = NULL;
		for (chunk_l = dl->chunks; chunk_l; chunk_l = chunk_l->next)
		{
			chunk = chunk_l->data;
			if (chunk->size - chunk->received > remaining)
			{
				remaining = chunk->size - chunk->received;
				tmp_l = chunk_l;
			}
		}

		chunk_l = tmp_l;

		if (!chunk_l || remaining <= AS_DOWNLOAD_MIN_CHUNK_SIZE * 2)
		{
			/* No more chunks to break up */
			break;
		}

		/* break up this chunk in middle of remaining size */
		chunk = chunk_l->data;
		new_size = (chunk->size - chunk->received) / 2;
		new_start = chunk->start + chunk->size - new_size;

		if (!(new_chunk = as_downchunk_create (new_start, new_size)))
		{
			/* Nothing we can do but bail out. Overall state should still be
			 * consistent and other connections and chunks can go on.
			 */
			AS_ERR_2 ("Couldn't create chunk (%u,%u)", new_start, new_size);
			return FALSE;						
		}

		/* associate new chunk with connection */
		new_chunk->udata = conn;
		conn->udata2 = new_chunk;

		/* Start new connection */
		if (!as_downconn_start (conn, dl->hash, new_chunk->start,
		                        new_chunk->size))
		{
			/* download start failed, remove connection. */
			AS_WARN_2 ("Failed to start download from %s:%d, removing source.",
			           net_ip_str (conn->source->host),
			           conn->source->port);

			as_downchunk_free (new_chunk);
			as_downconn_free (conn);

			tmp_l = conn_l->next;
			dl->conns = list_remove_link (dl->conns, conn_l);
			conn_l = tmp_l;

			/* We now have a chunk in the list which has no source. Fixing
			 * this here would be complicated so we break and leave it to
			 * a later maintenance timer invokation.
			 */
			break;
		}

		max_new_conns--;

		AS_HEAVY_DBG_5 ("Started new chunk (%u,%u) of \"%s\" with %s:%d",
	                    new_chunk->start, new_chunk->size, dl->filename,
		                net_ip_str (conn->source->host),
		                conn->source->port);

		/* Shorten old chunk. */
		chunk->size -= new_size;

		AS_HEAVY_DBG_4 ("Reduced old chunk from (%u,%u) to (%u,%u)",
	                    chunk->start, chunk->size + new_size,
	                    chunk->start, chunk->size);

		/* Insert new chunk after old one */
		tmp_l = list_prepend (NULL, new_chunk);
		list_insert_link (chunk_l, tmp_l);

		/* go on with next connection */
		conn_l = conn_l->next;
	}

	/* If there are still unused sources left at this point we are in endgame 
	 * mode. Replace slow connections with unused faster ones. The 
	 * max_new_conns check prevents looping through a lot of unused sources
	 * if we are not yet in endgame mode.
	 */
	while (conn_l && max_new_conns > 0)
	{
		conn = conn_l->data;

		/* if we see a used connection we can stop since there are no more
		 * unused ones
		 */
		if (conn->udata2)
			break;

		/* Skip queued sources. */
		if (conn->state == DOWNCONN_QUEUED && conn->queue_next_try > now)
		{
			conn_l = conn_l->next;
			continue;
		}
		
		/* Find slowest connection to replace. Ideally we would start at the
		 * end of the list since that's where the slowest connections are. Use
		 * current b/w of sources instead of historical one so we have a value
		 * other than 0 for first time requests.
		 */
		speed = 0xFFFFFFFF;
		slow_conn_l = NULL;

		for (tmp_l = conn_l->next; tmp_l; tmp_l = tmp_l->next)
		{
			slow_conn = tmp_l->data;

			if (slow_conn->udata2 && as_downconn_speed (slow_conn) < speed)
			{
				speed = as_downconn_speed (slow_conn);
				slow_conn_l = tmp_l;
			}
		}

		/* Changing the connection must be worth it so only do it if we
		 * can become at least 1 kb/s faster.
		 */
		if (!slow_conn_l || speed + 1024 >= as_downconn_speed (conn))
			break;

		slow_conn = slow_conn_l->data;

		AS_HEAVY_DBG_3 ("Removing slow source %s (%2.2f kb/s) for potentially faster %2.2f kb/s",
		                net_ip_str (slow_conn->source->host),
		                (float)speed / 1024,
		                (float)as_downconn_speed (conn) / 1024);

		/* cancel slow connection */
		as_downconn_cancel (slow_conn);

		/* disassociate chunk */
		chunk = slow_conn->udata2;
		slow_conn->udata2 = NULL;
		chunk->udata = NULL;

		/* Remove old connection, don't need it anymore. This is safe because
		 * the old connection comes after conn_l.
		 */
		dl->conns = list_remove_link (dl->conns, slow_conn_l);
		as_downconn_free (slow_conn);

		/* Start chunk again with faster connection. This is the only point
		 * where we start a chunk without chunk->received being 0.
		 */
		chunk->udata = conn;
		conn->udata2 = chunk;

		if (!as_downconn_start (conn, dl->hash,
		                        chunk->start + chunk->received,
		                        chunk->size - chunk->received))
		{
			/* Download start failed, remove connection. This leaves the chunk
			 * list in a non-consolidated state which is bad form but still
			 * works since consolidate_chunks will be called later before we
			 * do anything else.
			 */
			AS_WARN_2 ("Failed to start replacement download from %s:%d, removing source.",
			           net_ip_str (conn->source->host),
			           conn->source->port);

			/* We removed a slow connection and couldn't replace it. */
			max_new_conns++;

			as_downconn_free (conn);
			chunk->udata = NULL;

			tmp_l = conn_l->next;
			dl->conns = list_remove_link (dl->conns, conn_l);
			conn_l = tmp_l;

			/* What we should do now is use the next unused source for the
			 * chunk we just disconnected and couldn't reconnect. Since that
			 * would become pretty ugly to do here and this is an unlikely
			 * error case we just break the loop and let the maintenance timer
			 * handle it later.
			 */
			break;
		}

		conn_l = conn_l->next;
	}

	/* Get entire connection list in order. A bit inefficient. Would be better
	 * to have two connection lists, one unsorted for used and one sorted for
	 * unused connections.
	 */
	dl->conns = list_sort (dl->conns, (CompareFunc) conn_cmp_func);

	return TRUE;
}

/*
 * The heart of the download system. It is called whenever there are new
 * sources, finished chunks, etc.
 * Merges complete chunks and tries to assign sources to inactive ones. If 
 * there are more sources than chunks the chunks are split up.
 */
static void download_maintain (ASDownload *dl)
{
	if (dl->state != DOWNLOAD_ACTIVE)
	{
		/* Must not happen. */
		assert (dl->state == DOWNLOAD_ACTIVE);
		return;
	}

#ifdef VERIFY_CONN_LIST
	/* Verify integrity of connection list */
	verify_connections (dl);
#endif

	/* Verify integrity of chunk list. */
	if (!verify_chunks (dl))
	{
		AS_ERR_1 ("Corrupted chunk list detected for \"%s\"", dl->filename);
		
		/* Fail download */
		download_failed (dl);

		assert (0);
		return;
	}

	/* Clean up chunks. */
	if (!consolidate_chunks (dl))
	{
		AS_ERR_1 ("Consolidating chunks failed for \"%s\"", dl->filename);
		
		/* Fail download */
		download_failed (dl);

		assert (0);
		return;
	}

#ifdef CHUNK_DEBUG
	AS_HEAVY_DBG ("Chunk state after consolidating:");
	dump_chunks (dl);


	AS_HEAVY_DBG ("Connection state after consolidating:");
	dump_connections (dl);
#endif

	/* Is the download complete? */
	if (((ASDownChunk *)dl->chunks->data)->received == dl->size)
	{
		/* Download complete */
		download_finished (dl);
		return;
	}

	/* Download not complete. Start more chunk downloads. */
	if (!start_chunks (dl))
	{
		/* This should be harmless. */
		AS_WARN_1 ("Starting chunks failed for \"%s\"", dl->filename);
	}

	/* Check if we need more sources */
	if (dl->conns == NULL)
	{
		/* TODO: start source search  */
		AS_ERR_1 ("FIXME: No more sources for \"%s\". Make me find more.",
		          dl->filename);
	}
}

/* Called by a regular timer while download is active */
static as_bool maintenance_timer_func (ASDownload *dl)
{
	AS_HEAVY_DBG ("Download maintenance timer invoked");

	/* Save state data. */
	if (!as_downstate_save (dl))
	{
		AS_ERR_1 ("Failed to write state data for \"%s\". Pausing.",
		          dl->filename);
		as_download_pause (dl);	/* will remove timer */
		return FALSE;
	}

	/* Check on queued connections */
	download_maintain (dl);

	return TRUE; /* invoke again */
}

/*****************************************************************************/

/* Returns filename which is not yet used. Caller frees result. */
static char *get_available_filename (const char *filename)
{
	char *ext, *uniq, *name;
	int i = 0;

	if (!filename)
		return NULL;

	name = strdup (filename);

	if ((ext = strrchr (name, '.')))
		*ext++ = 0;

	/* start with original name */
	uniq = stringf_dup ("%s.%s", name, ext);

	/* find a free filename */
	while (as_file_exists (uniq))
	{
		free (uniq);

		if ((++i) == 1000)
		{
			free (name);
			return NULL;
		}

		uniq = stringf_dup ("%s(%d).%s", name, i, ext);
	}

	free (name);

	return uniq;
}

static as_bool download_failed (ASDownload *dl)
{
	AS_DBG_1 ("Failed download \"%s\"", dl->filename);

	/* Stop all chunk downloads if there are still any */
	stop_all_connections (dl);

	/* Make sure any source search is gone */
	if (dl->search)
		as_searchman_remove (AS->searchman, dl->search);
	dl->search = NULL;

	/* close fd */
	if (dl->fp)
	{
		fclose (dl->fp);
		dl->fp = NULL;
	}

	/* delete incomplete file. */
#ifndef KEEP_FAILED
	if (unlink (dl->path) < 0)
		AS_ERR_1 ("Failed to unlink incomplete file \"%s\"", dl->path);
#else
	AS_WARN_1 ("Keeping failed download \"%s\" for debugging.", dl->path);
#endif

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_FAILED, TRUE))
		return FALSE;

	return TRUE;
}

static as_bool download_complete (ASDownload *dl)
{
	char *prefixed_name;

	AS_DBG_1 ("Completed download \"%s\"", dl->filename);

	/* close fd */
	if (dl->fp)
	{
		fclose (dl->fp);
		dl->fp = NULL;
	}

	/* rename complete file to not include the prefix. */
	prefixed_name = as_get_filename (dl->path);

	if (strncmp (prefixed_name, AS_DOWNLOAD_INCOMPLETE_PREFIX,
	             strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX)) == 0)
	{
		char *completed_path, *uniq_path, *dir;

		/* construct path without ___ARESTRA___ prefix */
		dir = gift_strndup (dl->path, prefixed_name - dl->path);
		completed_path = stringf_dup ("%s%s", dir ? dir : "",
		                        prefixed_name + strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX));
		free (dir);

		if ((uniq_path = get_available_filename (completed_path)))
		{
			if (rename (dl->path, uniq_path) >= 0)
			{
				AS_DBG_2 ("Moved complete file \"%s\" to \"%s\"",
				          dl->path, uniq_path);

				/* update download filename */
				free (dl->path);
				dl->path = uniq_path;
				dl->filename = as_get_filename (dl->path);
			}
			else
			{
				AS_ERR_2 ("Renaming file from \"%s\" to \"%s\" failed.",
				          dl->path, uniq_path);
				free (uniq_path);
			}
		}
		else
		{
			AS_ERR_1 ("No unique name found for \"%s\"", dl->path);
		}
	}
	else
	{
		AS_WARN_1 ("Complete file \"%s\" has no prefix. No renaming performed.",
		           dl->path);
	}

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_COMPLETE, TRUE))
		return FALSE;

	return TRUE;
}

/* Checks finished transfer. */
static as_bool download_finished (ASDownload *dl)
{
	ASHash *hash;
#ifdef WIN32
	int fd;
#endif

	/* Stop all chunk downloads if there are still any */
	stop_all_connections (dl);

	/* Make sure any source search is gone */
	if (dl->search)
		as_searchman_remove (AS->searchman, dl->search);
	dl->search = NULL;

	AS_DBG_1 ("Verifying download \"%s\"", dl->filename);

	/* Do some sanity checks */
	assert (dl->chunks->next == NULL);
	assert (((ASDownChunk *)dl->chunks->data)->udata == NULL);
	assert (((ASDownChunk *)dl->chunks->data)->size == dl->size);
	assert (dl->fp != NULL);

	/* Close file pointer */
	fclose (dl->fp);
	dl->fp = NULL;

	/* raise callback */
	if (!download_set_state (dl, DOWNLOAD_VERIFYING, TRUE))
		return FALSE;

	/* Truncate incomplete file to correct size removing the state data at
	 * the end.
	 */
	assert (dl->size > 0);

#ifndef WIN32
	if (truncate (dl->path, dl->size) < 0)
#else
	if ((fd = _open (dl->path, _O_BINARY | _O_WRONLY)) < 0 ||
	    _chsize (fd, dl->size) != 0 ||
	    _close (fd) != 0)
#endif
	{
		AS_ERR_1 ("Failed to truncate complete download \"%s\"",
		          dl->path);
		/* File is probably still useful so continue. */
	}

	/* Hash file and compare hashes.
	 * TODO: Make non-blocking.
	 */
	if (!(hash = as_hash_file (dl->path)))
	{
		AS_ERR_1 ("Couldn't hash \"%s\" for verification", dl->path);
		return download_failed (dl);
	}

	if (!as_hash_equal (dl->hash, hash))
	{
		AS_ERR_1 ("Downloaded file \"%s\" corrupted!", dl->path);
		as_hash_free (hash);
		return download_failed (dl);
	}

	as_hash_free (hash);

	/* Download is OK */
	return download_complete (dl);
}

/*****************************************************************************/

#ifdef HEAVY_DEBUG

static void dump_chunks (ASDownload *dl)
{
	List *l;
	for (l = dl->chunks; l; l = l->next)
	{
		ASDownChunk *chunk = l->data;
		ASDownConn *conn = chunk->udata;

		AS_HEAVY_DBG_5 ("Chunk: (%7u, %7u, %7u) conn %15s %s",
		                chunk->start, chunk->size, chunk->received,
						conn ? net_ip_str (conn->source->host) : "-",
		                (chunk->received == chunk->size) ? " complete" : "");
	}
}

static void dump_connections (ASDownload *dl)
{
	List *l;
	for (l = dl->conns; l; l = l->next)
	{
		ASDownConn *conn = l->data;

		AS_HEAVY_DBG_6 ("Connection: %15s, %4u KB, %2.2f KB/s, (%7u, %7u), %s",
		                net_ip_str (conn->source->host),
		                (conn->hist_downloaded + conn->curr_downloaded) / 1024,
						(float)as_downconn_speed (conn) / 1024,
		                conn->chunk_start, conn->chunk_size,
		                as_downconn_state_str (conn));
	}
}

#endif

#ifdef VERIFY_CONN_LIST
static void verify_connections (ASDownload *dl)
{
	List *link;
	unsigned int bw = 0xFFFFFFFF;
	as_bool in_use = FALSE;

	/* Verify we are still sorted correctly */
	for (link = dl->conns; link; link = link->next)
	{
		ASDownConn *conn = link->data;

		/* If we saw one used node all following must be in use as well */
		if (in_use)
			assert (conn->udata2 != NULL);

		/* If this is the first used node reset b/w. */
		if (conn->udata2 && !in_use)
		{
			in_use = TRUE;
			bw = 0xFFFFFFFF;
		}

		/* b/w must be descending. */
		assert (as_downconn_hist_speed (conn) <= bw);
		bw = as_downconn_hist_speed (conn);
	}
}
#endif

/*****************************************************************************/

