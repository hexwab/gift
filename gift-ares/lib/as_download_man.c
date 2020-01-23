/*
 * $Id: as_download_man.c,v 1.10 2004/11/06 18:08:17 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static unsigned int active_downloads (ASDownMan *man);

/* The callback we assign to all downloads so we can intercept them for queue
 * management.
 */
static as_bool download_state_cb (ASDownload *dl, ASDownloadState state);

/* Periodic timer function which in turn calls progress callback. */
static as_bool progress_timer_func (ASDownMan *man);

/*****************************************************************************/

/* Allocate and init download manager. state_cb is called for state changes
 * in every download.
 */
ASDownMan *as_downman_create ()
{
	ASDownMan *man;

	if (!(man = malloc (sizeof (ASDownMan))))
		return FALSE;

	if (!(man->hash_index = as_hashtable_create_mem (TRUE)))
	{
		free (man);
		return NULL;
	}

	man->downloads = NULL;
	man->stopped = FALSE;
	man->state_cb = NULL;
	man->progress_cb = NULL;
	man->progress_timer = INVALID_TIMER;

	return man;
}

/* Free manager. */
void as_downman_free (ASDownMan *man)
{
	List *l;
	ASDownload *dl;

	if (!man)
		return;

	for (l = man->downloads; l; l = l->next)
	{
		dl = l->data;

		if (dl->state == DOWNLOAD_ACTIVE || dl->state == DOWNLOAD_VERIFYING)
			AS_WARN_1 ("Download active when freed \"%s\"", dl->filename);

		as_download_free (dl);
	}
	list_free (man->downloads);

	as_hashtable_free (man->hash_index, FALSE);
	timer_remove_zero (&man->progress_timer);

	free (man);
}

/* Set callback triggered for every state change in one of the downloads. */
void as_downman_set_state_cb (ASDownMan *man, ASDownManStateCb state_cb)
{
	man->state_cb = state_cb;
}

/* Set callback triggered periodically for progress updates. */
void as_downman_set_progress_cb (ASDownMan *man,
                                 ASDownManProgressCb progress_cb)
{
	if (progress_cb)
	{
		man->progress_cb = progress_cb;

		if (man->progress_timer == INVALID_TIMER &&
		    active_downloads (man) > 0)
		{
			man->progress_timer = timer_add (AS_DOWNLOAD_PROGRESS_INTERVAL,
		                             (TimerCallback)progress_timer_func, man);
		}
	}
	else
	{
		man->progress_cb = NULL;
		timer_remove_zero (&man->progress_timer);
	}
}

/*****************************************************************************/

/* If stop is TRUE all downloads are stopped (put in state DOWNLOAD_QUEUED).
 * If stop is FALSE downloads are resumed. Call this with TRUE before
 * shutting down to write clean state of all downloads to disk.
 */
as_bool as_downman_stop_all (ASDownMan *man, as_bool stop)
{
	List *l;
	ASDownload *dl;
	int active;

	if (stop)
	{
		man->stopped = TRUE;

		for (l = man->downloads; l; l = l->next)
			as_download_queue (l->data);
	}
	else
	{
		man->stopped = FALSE;

		l = man->downloads;
		active = 0;

		while (active < AS_CONF_INT (AS_DOWNLOAD_MAX_ACTIVE) && l)
		{
			dl = l->data;
		
			if (dl->state == DOWNLOAD_QUEUED)
			{
				if (as_download_resume (dl))
					active++;
			}	

			l = l->next;
		}
	}
	
	return TRUE;
}

/*****************************************************************************/

/* Create download from a search result. Looks up other search results with
 * the same hash and adds them as sources. If a download with the same hash
 * already exists it is returned and no new download is created.
 */
ASDownload *as_downman_start_result (ASDownMan *man, ASResult *result,
                                     const char *save_path)
{
	ASSearch *search;
	ASDownload *dl;
	int sources = 0;

	assert (result);

	/* Check if we are already downloading this hash */
	if ((dl = as_downman_lookup_hash (man, result->hash)))
	{
		AS_DBG_1 ("Tried to start new download with hash already in use. "
		          "Returning download \"%s\"", dl->filename);
		return dl;
	}

	/* Create new download. */
	if (!(dl = as_download_create (download_state_cb)))
	{
		AS_ERR_1 ("Couldn't create download for result \"%s\"",
		          result->filename);
		return NULL;
	}

	dl->downman = man;

	/* Add source from result. */
	if (as_download_add_source (dl, result->source))
		sources++;

	/* Lookup search which returned this result and get additional sources */
	if ((search = as_searchman_lookup (AS->searchman, result->search_id)))
	{
		List *res_l;
		ASResult *res;

		/* Get list of results with same hash */
		res_l = as_search_get_results (search, result->hash);

		/* Add all result as sources */
		while (res_l)
		{
			res = res_l->data;

			if (as_download_add_source (dl, res->source))
				sources++;

			res_l = res_l->next;
		}
	}

	/* copy meta data from result to download */
	dl->meta = as_meta_copy (result->meta);

	AS_DBG_2 ("Created new download \"%s\" with %d sources",
	          save_path, sources);

	/* Add download to list and hash index */
	man->downloads = list_prepend (man->downloads, dl);

	if (!as_hashtable_insert (man->hash_index, result->hash->data,
	                          AS_HASH_SIZE, man->downloads))
	{
		AS_ERR_1 ("Failed to add new download \"%s\" to hash table",
		          dl->filename);
		man->downloads = list_remove (man->downloads, dl);
		as_download_free (dl);
		return NULL;
	}

	/* Start the download. The callback will decide if it starts out active
	 * or queued.
	 */
	if (!as_download_start (dl, result->hash, result->filesize, save_path))
	{
		/* This can happen if the state callback freed the search and returned
		 * FALSE. So only free the search if it is still in the list.
		 */
		List *link, *index_link;

		if ((link = list_find (man->downloads, dl)))
		{
			/* remove from hash index */
			index_link = as_hashtable_remove (man->hash_index,
			                                  result->hash->data,
			                                  AS_HASH_SIZE);
			assert (index_link == link);
			/* remove from list */
			man->downloads = list_remove_link (man->downloads, link);
			/* free download */
			as_download_free (dl);
		}

		AS_ERR ("Couldn't start new download");
		return NULL;
	}

	return dl;
}

/* Create download from ares hash link and start source search. If a download
 * with the same hash already exists it is returned and no new download is
 * created.
 */
ASDownload *as_downman_start_link (ASDownMan *man, const char *hash_link,
                                   const char *save_path)
{
	return NULL;
}

/*****************************************************************************/

/* Creates download from incomplete file pointed to by path. */
ASDownload *as_downman_restart_file (ASDownMan *man, const char *path)
{
	ASDownload *dl;

	/* Create new download. */
	if (!(dl = as_download_create (download_state_cb)))
	{
		AS_ERR_1 ("Couldn't create download for incomplete file \"%s\"",
		          path);
		return NULL;
	}

	dl->downman = man;

	/* Add download to list */
	man->downloads = list_prepend (man->downloads, dl);

	/* Start the download. The callback will decide if it starts out active
	 * or queued.
	 */
	if (!as_download_restart (dl, path))
	{
		/* This can happen if the state callback freed the search and returned
		 * FALSE. Since we haven't inserted into the hashtable yet just remove
		 * from linked list if download is still there.
		 */
		if (list_find (man->downloads, dl))
		{
			man->downloads = list_remove (man->downloads, dl);
			as_download_free (dl);
			AS_ERR ("Couldn't start new download");
			return NULL;
		}
	}

	/* Now that we have a hash check if we are already downloading it. */
	if (as_downman_lookup_hash (man, dl->hash))
	{
		AS_ERR_1 ("Won't restart download \"%s\" because one with the same "
		          "hash is already active.", dl->filename);
		man->downloads = list_remove (man->downloads, dl);
		as_download_free (dl);
		return NULL;
	}

	/* Insert download into hash table */
	if (!as_hashtable_insert (man->hash_index, dl->hash->data,
	                          AS_HASH_SIZE, man->downloads))
	{
		AS_ERR_1 ("Failed to add new download \"%s\" to hash table",
		          dl->filename);
		man->downloads = list_remove (man->downloads, dl);
		as_download_free (dl);
		return NULL;
	}

	AS_DBG_1 ("Started new download \"%s\" from incomplete file",
	          dl->filename);

	return dl;
}

static as_bool restart_file_itr (ASDownMan *man, const char *dir,
                                 const char* file)
{
	char *path;

	/* only resume files with prefix */
	if (strncmp (file, AS_DOWNLOAD_INCOMPLETE_PREFIX,
		         strlen (AS_DOWNLOAD_INCOMPLETE_PREFIX)) != 0)
	{
		return FALSE;
	}

	if (!(path = stringf_dup ("%s/%s", (dir && *dir) ? dir : ".", file)))
		return FALSE;

	AS_HEAVY_DBG_1 ("Restoring download '%s'", path);

	if (!as_downman_restart_file (man, path))
	{
		free (path);
		return FALSE;
	}

	free (path);
	return TRUE;
}

/* Resumes all incomplete downloads in specified directory. Returns the number
 * of resumed downloads or -1 on error. Callback is raised for each started
 * download as usual.
 */
int as_downman_restart_dir (ASDownMan *man, const char *dir)
{
	int files = 0;

#ifdef HAVE_DIRENT_H
	DIR *dh;
	struct dirent *de;

	if (!(dh = opendir ((dir && *dir) ? dir : ".")))
	{
		AS_ERR_1 ("Couldn't open directory '%s'", dir);
		return -1;
	}

	while ((de = readdir (dh)))
	{
		if (restart_file_itr (man, dir, de->d_name))
			files++;
	}

	closedir (dh);
#else
	long fh;
	struct _finddata_t fi;
	char *glob = stringf_dup ("%s/%s*", (dir && *dir) ? dir : ".",
	                          AS_DOWNLOAD_INCOMPLETE_PREFIX);
	assert (glob);

	if ((fh = _findfirst (glob, &fi)) != -1)
	{
		do
		{
			if (restart_file_itr (man, dir, fi.name))
				files++;
		}
		while (_findnext (fh, &fi) == 0);

		_findclose (fh);
	}

	free (glob);
#endif

	return files;
}

/*****************************************************************************/

/* Pause download if pause is TRUE or resume it if pause is FALSE. */
as_bool as_downman_pause (ASDownMan *man, ASDownload *dl, as_bool pause)
{
	if (!list_find (man->downloads, dl))
	{
		AS_HEAVY_DBG ("Tried to pause invalid download");
		return FALSE;
	}

	if (pause)
		return as_download_pause (dl);
	else if (dl->state != DOWNLOAD_QUEUED) /* don't resume queued */
		return as_download_resume (dl);

	return FALSE;
}

/* Cancel download but do not remove it. */
as_bool as_downman_cancel (ASDownMan *man, ASDownload *dl)
{
	if (!list_find (man->downloads, dl))
	{
		AS_HEAVY_DBG ("Tried to cancel invalid download");
		return FALSE;
	}

	return as_download_cancel (dl);
}

/* Remove and free finished, failed or cancelled download. */
as_bool as_downman_remove (ASDownMan *man, ASDownload *dl)
{
	List *link, *index_link;

	if (!(link = list_find (man->downloads, dl)))
	{
		AS_HEAVY_DBG ("Tried to remove invalid download");
		return FALSE;
	}

	if (dl->state != DOWNLOAD_NEW &&
	    dl->state != DOWNLOAD_COMPLETE &&
	    dl->state != DOWNLOAD_FAILED &&
	    dl->state != DOWNLOAD_CANCELLED)
	{
		AS_DBG_1 ("Tried to remove active download \"%s\"", dl->filename);
		return FALSE;
	}

	/* remove from hash index */
	index_link = as_hashtable_remove (man->hash_index, dl->hash->data,
	                                  AS_HASH_SIZE);
	assert (index_link == link);

	/* remove from list */
	man->downloads = list_remove_link (man->downloads, link);

	/* free download */
	as_download_free (dl);

	return TRUE;
}

/* Start source search for download. */
as_bool as_downman_find_sources (ASDownMan *man, ASDownload *dl)
{
	if (!list_find (man->downloads, dl))
	{
		AS_HEAVY_DBG ("Tried to find sources for invalid download");
		return FALSE;
	}

	return as_download_find_sources (dl);
}

/*****************************************************************************/

/* Return state of download. The advantage to as_download_state is that this
 * makes sure the download is actually still in the list and thus valid
 * before accessing it. If the download is invalid DOWNLOAD_INVALID is
 * returned.
 */
ASDownloadState as_downman_state (ASDownMan *man, ASDownload *dl)
{
	if (!list_find (man->downloads, dl))
		return DOWNLOAD_INVALID;

	return as_download_state (dl);
}

/* Return download by file hash or NULL if there is none. */
ASDownload *as_downman_lookup_hash (ASDownMan *man, ASHash *hash)
{
	List *link;
	ASDownload *dl;

	assert (hash);

	if (!(link = as_hashtable_lookup (man->hash_index, hash->data,
	                                  AS_HASH_SIZE)))
	{
		return NULL;
	}

	dl = link->data;

	assert (as_hash_equal (dl->hash, hash));

	return dl;
}

/*****************************************************************************/

static unsigned int active_downloads (ASDownMan *man)
{
	unsigned int active = 0;
	List *l;
	ASDownload *dl;

	for (l = man->downloads; l; l = l->next)
	{
		dl = l->data;
	
		if (dl->state == DOWNLOAD_ACTIVE)
			active++;
	}

	return active;
}


/* The callback we assign to all downloads so we can intercept them for queue
 * management.
 */
static as_bool download_state_cb (ASDownload *dl, ASDownloadState state)
{
	ASDownMan *man = dl->downman;
	as_bool ret;
	int active = active_downloads (dl->downman);

	assert (man);

	/* If we are at the limit don't allow any download to become active. */
	if (state == DOWNLOAD_ACTIVE &&
	    (active > AS_CONF_INT (AS_DOWNLOAD_MAX_ACTIVE) || man->stopped))
	{
		/* This triggers us again and the user is then notified of the new
		 * queued state.
		 */
		return as_download_queue (dl);
	}

	/* Notify user. */
	if (man->state_cb)
		ret = man->state_cb (man, dl, state);
	else
		ret = TRUE;

	/* If there now is a free slot activate another download */
	if (!man->stopped)
	{
		ASDownload *queued_dl;
		List *l = man->downloads;

		while (active < AS_CONF_INT (AS_DOWNLOAD_MAX_ACTIVE) && l)
		{
			queued_dl = l->data;
		
			if (queued_dl->state == DOWNLOAD_QUEUED)
			{
				if (as_download_resume (queued_dl))
					active++;
			}	

			l = l->next;
		}
	}

	/* start or stop progress timer */
	if (active > 0)
	{
		if (man->progress_timer == INVALID_TIMER && man->progress_cb)
		{
			man->progress_timer = timer_add (AS_DOWNLOAD_PROGRESS_INTERVAL,
		                             (TimerCallback)progress_timer_func, man);
		}
	}
	else
	{
		timer_remove_zero (&man->progress_timer);
	}

	return ret;
}

/* Periodic timer function which in turn calls progress callback. */
static as_bool progress_timer_func (ASDownMan *man)
{
	assert (man->progress_cb);
	assert (active_downloads (man) > 0); /* REMOVE ME */

	/* raise callback */
	man->progress_cb (man);

	return TRUE; /* reset timer */
}

/*****************************************************************************/
