/*
 * $Id: ar_download.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "aresdll.h"
#include "as_ares.h"
#include "ar_threading.h"
#include "ar_callback.h"
#include "ar_misc.h"

/*****************************************************************************/

static ARDownloadState export_download_state (ASDownloadState state)
{
	switch (state)
	{
	case DOWNLOAD_INVALID:   return AR_DOWNLOAD_INVALID;
	case DOWNLOAD_NEW:       return AR_DOWNLOAD_NEW;
	case DOWNLOAD_ACTIVE:    return AR_DOWNLOAD_ACTIVE;
	case DOWNLOAD_QUEUED:    return AR_DOWNLOAD_QUEUED;
	case DOWNLOAD_PAUSED:    return AR_DOWNLOAD_PAUSED;
	case DOWNLOAD_COMPLETE:  return AR_DOWNLOAD_COMPLETE;
	case DOWNLOAD_FAILED:    return AR_DOWNLOAD_FAILED;
	case DOWNLOAD_CANCELLED: return AR_DOWNLOAD_CANCELLED;
	case DOWNLOAD_VERIFYING: return AR_DOWNLOAD_VERIFYING;
	}

	abort ();
}

/*****************************************************************************/

static as_bool downman_state_callback (ASDownMan *man, ASDownload *dl,
                                       ASDownloadState state)
{
	ARDownload download;

	download.handle = (ARDownloadHandle) dl;
	download.state = export_download_state (dl->state);
	download.path = dl->path;
	download.filename = dl->filename;
	download.filehash = dl->hash->data;
	download.filesize = dl->size;
	download.received = dl->received;
	
	ar_export_meta (download.meta, dl->meta);

	/* send to user app */
	ar_raise_callback (AR_CB_DOWNLOAD, &download, NULL);

	return TRUE; /* FIXME: return FALSE if download was freed */
}

/*****************************************************************************/

static void downman_progress_callback (ASDownMan *man)
{
	ARDownloadProgress *progress;
	int active = 0, i = 0;
	List *l;

	/* FIXME: Optimize memory allocation */
	for (l = man->downloads; l; l = l->next)
	{
		if (((ASDownload *)l->data)->state == DOWNLOAD_ACTIVE)
			active++;
	}

	assert (active > 0);

	if (!(progress = malloc (sizeof (ARDownloadProgress) + 
	                         sizeof (progress->downloads) * (active - 1))))
	{
		AS_ERR ("Insufficient memory.");
		return;
	}

	progress->download_count = active;

	/* copy progress data for active downloads */
	for (l = man->downloads; l; l = l->next)
	{
		ASDownload *dl = l->data;

		if (dl->state == DOWNLOAD_ACTIVE)
		{
			assert (i < active);
			progress->downloads[i].handle = (ARDownloadHandle) dl;
			progress->downloads[i].filesize = dl->size;
			progress->downloads[i].received = dl->received;
			i++;
		}
	}

	/* send to user app */
	ar_raise_callback (AR_CB_PROGRESS, progress, NULL);
}

/*****************************************************************************/

/*
 * Start new download from search result. The complete file will be saved as
 * save_path but during download the filename will have the ___ARESTRA___
 * prefix. If a download for the same hash already exists or there is another
 * problem AR_INVALID_HANDLE is returned.
 */
ARDownloadHandle ar_download_start (ARSearchHandle search, 
                                    as_uint8 hash[AR_HASH_SIZE],
                                    const char *save_path)
{
	ASDownload *dl;
	List *l;
	ASHash *h;

	if (!ar_events_pause ())
		return AR_INVALID_HANDLE;

	/* Make sure the downman callbacks are set to us */
	as_downman_set_state_cb (AS->downman, downman_state_callback);
	as_downman_set_progress_cb (AS->downman, downman_progress_callback);

	/* crude safety check against remotely controlled paths */
	if (strstr (save_path, "..") != NULL)
	{
		AS_ERR_1 ("Download path '%s' contains insecure '..' element.",
		          save_path);
		ar_events_resume ();
		return AR_INVALID_HANDLE;
	}

	/* find result using supplied hash */
	if (!as_searchman_valid_search (AS->searchman, (ASSearch *)search))
	{
		AS_ERR_1 ("Tried to start download '%s' from invalid search",
		          save_path);
		ar_events_resume ();
		return AR_INVALID_HANDLE;
	}

	if (!(h = as_hash_create (hash, AR_HASH_SIZE)))
	{
		AS_ERR_1 ("Failed to create hash for download '%s'", save_path);
		ar_events_resume ();
		return AR_INVALID_HANDLE;
	}

	if (!(l = as_search_get_results ((ASSearch *)search, h)))
	{
		AS_ERR_1 ("Tried to start download '%s' with a hash not in search results",
		          save_path);
		as_hash_free (h);
		ar_events_resume ();
		return AR_INVALID_HANDLE;
	}

	as_hash_free (h);

	/* start download from result */
	if (!(dl = as_downman_start_result (AS->downman, l->data, save_path)))
	{
		AS_ERR_1 ("Failed to create download object for '%s'", save_path);
		ar_events_resume ();
		return AR_INVALID_HANDLE;
	}

	ar_events_resume ();
	return (ARSearchHandle) dl;
}

/*
 * Restart all incomplete downloads in specified directory. Returns number
 * of restarted downloads or -1 on failure.
 */
int ar_download_restart_dir (const char *dir)
{
	int ret;

	if (!ar_events_pause ())
		return -1;

	/* Make sure the downman callbacks are set to us */
	as_downman_set_state_cb (AS->downman, downman_state_callback);
	as_downman_set_progress_cb (AS->downman, downman_progress_callback);

	/* restart all files in dir */
	ret = as_downman_restart_dir (AS->downman, dir);

	ar_events_resume ();
	return ret;
}

/*
 * Get current state of download.
 */
ARDownloadState ar_download_state (ARDownloadHandle download)
{
	ASDownloadState state;

	if (!ar_events_pause ())
		return AR_DOWNLOAD_INVALID;

	state = as_downman_state (AS->downman, (ASDownload *)download);

	ar_events_resume ();
	return export_download_state (state);
}

/*
 * Pause download.
 */
as_bool ar_download_pause (ARDownloadHandle download)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_downman_pause (AS->downman, (ASDownload *)download, TRUE);

	ar_events_resume ();
	return ret;
}

/*
 * Resume paused download.
 */
as_bool ar_download_resume (ARDownloadHandle download)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_downman_pause (AS->downman, (ASDownload *)download, FALSE);

	ar_events_resume ();
	return ret;
}

/*
 * Cancel download but don't remove it. It will just be there to look at until
 * you remove it.
 */
as_bool ar_download_cancel (ARDownloadHandle download)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_downman_cancel (AS->downman, (ASDownload *)download);

	ar_events_resume ();
	return ret;
}

/*
 * Remove cancelled/complete/failed download and free all associated data.
 */
as_bool ar_download_remove (ARDownloadHandle download)
{
	as_bool ret;

	if (!ar_events_pause ())
		return FALSE;

	ret = as_downman_remove (AS->downman, (ASDownload *)download);

	ar_events_resume ();
	return ret;
}

/*****************************************************************************/
