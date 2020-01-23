/*
 * $Id: as_download.h,v 1.13 2004/10/26 19:31:12 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_DOWNLOAD_H
#define __AS_DOWNLOAD_H

/*****************************************************************************/

typedef enum
{
	DOWNLOAD_INVALID,    /* Used in as_downman_state to signal invalid
	                      * download. */
	DOWNLOAD_NEW,        /* Initial state before download is started. */
	DOWNLOAD_ACTIVE,     /* Download is transfering/looking for sources. */
	DOWNLOAD_QUEUED,     /* Download is locally queued (because
	                      * AS_DOWNLOAD_MAX_ACTIVE is reached). */
	DOWNLOAD_PAUSED,     /* Download is paused by user (or disk full). */
	DOWNLOAD_COMPLETE,   /* Download completed successfully. */
	DOWNLOAD_FAILED,     /* Download was fully transfered but hash check
	                      * failed. */
	DOWNLOAD_CANCELLED,  /* Download was cancelled. */
	DOWNLOAD_VERIFYING   /* Download is being verified after downloading. */
} ASDownloadState;

typedef struct as_download_t ASDownload;

/* Called for every state change. Return FALSE if the download was freed and
 * must no longer be accessed.
 */
typedef as_bool (*ASDownloadStateCb) (ASDownload *dl, ASDownloadState state);

struct as_download_t
{
	ASHash *hash;      /* file hash */
	char   *path;      /* path to incomplete or completed file */
	char   *filename;  /* pointer into path where completed file name starts
	                    * (i.e. after ___ARESTRA___ prefix) */
	size_t  size;      /* file size */
	size_t  received;  /* total number of bytes already received */
	FILE   *fp;        /* file pointer */

	List *conns;       /* List of ASDownConn's with sources. Sorted by the
	                    * connection's past bandwidth and whether it is
	                    * currently in use. */

	List *chunks;      /* List of chunks sorted by chunk->start and always
	                    * kept without holes and overlap */

	timer_id maintenance_timer; /* regular timer running while download is
	                             * active to check the queued sources */

	ASSearch *search;  /* Search object when doing a source search. */

	/* download state */
	ASDownloadState state;
	ASDownloadStateCb state_cb;

	/* pointer to download manager controlling this download */
	struct as_downman_t *downman; 
	/* meta data copied from search result by download manager */
	ASMeta *meta;

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* Create download. */
ASDownload *as_download_create (ASDownloadStateCb state_cb);

/* Stop and free download. Incomplete file is left in place and can be
 * resumed.
 */
void as_download_free (ASDownload *dl);

/*****************************************************************************/

/* Start download using hash, filesize and save name. */
as_bool as_download_start (ASDownload *dl, ASHash *hash, size_t filesize,
                           const char *save_path);

/* Restart download from incomplete ___ARESTRA___ file. This will fail if the
 * file is not found/corrupt/etc.
 */
as_bool as_download_restart (ASDownload *dl, const char *path);

/* Cancels download and removes incomplete file. */
as_bool as_download_cancel (ASDownload *dl);

/* Pause download */
as_bool as_download_pause (ASDownload *dl);

/* Queue download locally (effectively pauses it) */
as_bool as_download_queue (ASDownload *dl);

/* Resume download from paused or queued state */
as_bool as_download_resume (ASDownload *dl);

/* Returns current download state */
ASDownloadState as_download_state (ASDownload *dl);

/*****************************************************************************/

/* Add source to download (copies source). */
as_bool as_download_add_source (ASDownload *dl, ASSource *source);

/*****************************************************************************/

/* Start a source search for this download */
as_bool as_download_find_sources (ASDownload *dl);

/*****************************************************************************/

/* Return download state as human readable static string. */
const char *as_download_state_str (ASDownload *dl);

/*****************************************************************************/

#endif /* __AS_DOWNLOAD_H */
