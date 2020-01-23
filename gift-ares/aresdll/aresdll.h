/*
 * $Id: aresdll.h,v 1.2 2005/12/18 17:41:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

/*****************************************************************************/
/*
 * This file specifies the complete interface of our Ares dll.
 *
 * The internal code runs in a separate thread created on initialization. The
 * interface is thread safe meaning all functions can be called at any time.
 * All callbacks from the library are executed in the context of the thread
 * which initialized the dll. This is achieved by creating an internal window
 * at initialization and using it as a message target for SendMessage. When
 * the internal thread wants to raise a callback it uses SendMessage to send
 * a message to the internal window. The thread which created this message
 * window (the one which initialized the dll) is blocked and the message is
 * delievered immediately to the windowproc which then raises the callback.
 * Since this will block the internal thread callbacks should return in a
 * timely manner.
 * 
 */
/*****************************************************************************/

#ifndef __ARES_DLL_H
#define __ARES_DLL_H

/*****************************************************************************/

#include <winsock2.h>

/*****************************************************************************/

typedef signed char    as_int8;
typedef unsigned char  as_uint8;
typedef signed short   as_int16;
typedef unsigned short as_uint16;
typedef signed int     as_int32;
typedef unsigned int   as_uint32;
typedef int            as_bool;

#define in_addr_t u_long
#define in_port_t u_short

#define TRUE 1
#define FALSE 0

#define AR_EXPORT __declspec(dllexport)
#define AR_CC __cdecl

/*****************************************************************************/

#pragma pack(push)
#pragma pack(1)

/*****************************************************************************/

#define AR_INVALID_HANDLE NULL

typedef void* ARSearchHandle;
typedef void* ARDownloadHandle;
typedef void* ARUploadHandle;

#define AR_MAX_META_TAGS 20 /* max number of meta data tags per file */
#define AR_HASH_SIZE 20     /* size of sha1 file hash in bytes */

typedef as_uint8* ARHash;   /* points to AR_HASH_SIZE bytes */

typedef enum
{
	AR_REALM_ANY      = 0,
	AR_REALM_AUDIO    = 1,
	AR_REALM_VIDEO    = 2,
	AR_REALM_DOCUMENT = 3,
	AR_REALM_SOFTWARE = 4,
	AR_REALM_IMAGE    = 5,
	AR_REALM_ARCHIVE  = 6,  /* You cannot search for this realm */

    AR_REALM_MAX = 0x7FFFFFFF  /* force 32 bit enums */
} ARRealm;

typedef struct
{
	char *name;
	char *value;
} ARMetaTag;

typedef struct
{
	char      *path;    /* path of file */
	as_uint32  size;    /* file size */
	ARHash    *hash;    /* SHA1 of file */

	/* Meta data strings. Set name of unused entries to NULL. */
	ARMetaTag meta[AR_MAX_META_TAGS];
} ARShare;

typedef enum
{
	AR_DOWNLOAD_INVALID   = 0,   
	AR_DOWNLOAD_NEW       = 1, /* Initial state before download is started. */
	AR_DOWNLOAD_ACTIVE    = 2, /* Download is transfering/looking for sources. */
	AR_DOWNLOAD_QUEUED    = 3, /* Download is locally queued */
	AR_DOWNLOAD_PAUSED    = 4, /* Download is paused by user (or disk full). */
	AR_DOWNLOAD_COMPLETE  = 5, /* Download completed successfully. */
	AR_DOWNLOAD_FAILED    = 6, /* Download was fully transfered but hash
	                            * check failed. */
	AR_DOWNLOAD_CANCELLED = 7, /* Download was cancelled. */
	AR_DOWNLOAD_VERIFYING = 8, /* Download is being verified after
	                            * downloading. */

    AR_DOWNLOAD_MAX = 0x7FFFFFFF  /* force 32 bit enums */
} ARDownloadState;

typedef enum
{
	AR_UPLOAD_INVALID   = 0,   
	AR_UPLOAD_ACTIVE    = 1, /* Upload is transfering. */
	AR_UPLOAD_COMPLETE  = 2, /* Upload completed successfully. */
	AR_UPLOAD_CANCELLED = 3, /* Upload was cancelled (locally or remotely). */

    AR_UPLOAD_MAX = 0x7FFFFFFF  /* force 32 bit enums */
} ARUploadState;

typedef enum
{
	AR_CONF_PORT         = 0, /* Listen port (integer). */
	AR_CONF_USERNAME     = 1, /* User name (string). */
	AR_CONF_MAX_DOWNLODS = 2, /* Max concurrent downloads (integer). */
	AR_CONF_MAX_SOURCES  = 3, /* Max active sources per download (integer). */
	AR_CONF_MAX_UPLOADS  = 4  /* Max concurrent uploads (integer). */
} ARConfigKey;

/*****************************************************************************/

typedef enum
{
	AR_CB_STATUS   = 0, /* connect and stats update */
	AR_CB_RESULT   = 1, /* search result update */
	AR_CB_DOWNLOAD = 2, /* download state change */
	AR_CB_UPLOAD   = 3, /* upload state change */
	AR_CB_PROGRESS = 4, /* periodic download and upload progress notify */

    AR_CB_MAX = 0x7FFFFFFF  /* force 32 bit enums */
} ARCallbackCode;

/*
 * The callback is raised whenever something interesting happens. The meaning
 * of the parameters depends on the code.
 */
typedef void (AR_CC * ARCallback) (ARCallbackCode code, void *param1, void *param2);

/*
 * AR_CB_STATUS
 *   param1: ARStatusData
 *   param2: NULL
 */
typedef struct
{
	as_bool connected;  /* TRUE if we have at least one connection to the
	                     * network */
	as_bool connecting; /* TRUE if we are still looking for more connections
	                     * to the network */

	as_uint32 users;
	as_uint32 files;
	as_uint32 size;     /* total size of network in GB */
} ARStatusData;

/*
 * AR_CB_RESULT
 *   param1: ARSearchHandle
 *   param2: ARSearchResult (NULL when search is complete)
 */
typedef struct
{
	as_bool    duplicate; /* TRUE if a result with the same hash has been
	                       * reported before */
	char      *filename;
	as_uint32  filesize;
	ARHash     filehash;  /* SHA1 of file */
	ARRealm    realm;     /* media type of file */

	/* Meta data strings. Read until name is NULL or end of array is
	 * reached */
	ARMetaTag meta[AR_MAX_META_TAGS];
} ARSearchResult;

/*
 * AR_CB_DOWNLOAD
 *   param1: ARDownload
 *   param2: NULL
 */
typedef struct
{
	ARDownloadHandle handle; /* handle to access this download */
	ARDownloadState  state;  /* current download state */

	char     *path;      /* path of file we are writing to */
	char     *filename;  /* final file name without path */
	ARHash    filehash;  /* SHA1 of file */

	as_uint32 filesize;
	as_uint32 received;   /* bytes already downloaded */

	/* Meta data strings. Read until name is NULL or end of array is
	 * reached */
	ARMetaTag meta[AR_MAX_META_TAGS];
} ARDownload;

/*
 * AR_CB_UPLOAD
 *   param1: ARUpload
 *   param2: NULL
 */
typedef struct
{
	ARUploadHandle handle; /* handle to access this upload */
	ARUploadState  state;  /* current upload state */

	char      *path;       /* path of file we are reading from */
	char      *filename;   /* file name without path */
	ARHash     filehash;   /* SHA1 of file */

	as_uint32  filesize;
	as_uint32  start;      /* start of requested range */
	as_uint32  stop;       /* end of requested range (exclusive) */
	as_uint32  sent;       /* bytes already uploaded */

	in_addr_t  ip;         /* ip of user we are uploading to */
	char      *username;   /* name of user we are uploading to */

	/* Meta data strings. Read until name is NULL or end of array is
	 * reached */
	ARMetaTag meta[AR_MAX_META_TAGS];
} ARUpload;

/*
 * AR_CB_PROGRESS
 *   param1: ARDownloadProgress
 *   param2: ARUploadProgress
 * Note: One of the params may be NULL.
 */
typedef struct
{
	as_uint32 download_count;       /* number of following downloads */

	/* the following struct is repeated download_count times */
	struct
	{
		ARDownloadHandle handle;
		as_uint32 filesize;
		as_uint32 received;
	} downloads[1];

} ARDownloadProgress;

typedef struct
{
	as_uint32 upload_count;       /* number of following uploads */

	/* the following struct is repeated upload_count times */
	struct
	{
		ARUploadHandle handle;
		as_uint32 start;
		as_uint32 stop;
		as_uint32 sent;
	} uploads[1];

} ARUploadProgress;

/*****************************************************************************/

#pragma pack(pop)

/*****************************************************************************/

/*
 * Startup library and set callback. If logfile is not NULL it specifies the
 * path the logfile will be written to.
 */
AR_EXPORT
as_bool AR_CC ar_startup (ARCallback callback, const char *logfile);

/*
 * Shutdown everything. Downloads will be stopped and saved to to disk,
 * uploads will be stopped, etc.
 */
AR_EXPORT
as_bool AR_CC ar_shutdown ();

/*****************************************************************************/

/*
 * Connect to network, share files and allow searches.
 */
AR_EXPORT
as_bool AR_CC ar_connect ();

/* 
 * Disconnect from network. Searches are no longer possible but downloads and
 * already started uploads continue.
 */
AR_EXPORT
as_bool AR_CC ar_disconnect ();

/*****************************************************************************/

/*
 * Start a new search. Returns opaque handle to search or AR_INVALID_HANDLE
 * on failure.
 */
AR_EXPORT
ARSearchHandle ar_search_start (const char *query, ARRealm realm);

/*
 * Remove search. You cannot start any downloads using the results of a
 * download after removing it.
 */
AR_EXPORT
as_bool ar_search_remove (ARSearchHandle search);

/*****************************************************************************/

/*
 * Start new download from search result. The complete file will be saved as
 * save_path but during download the filename will have the ___ARESTRA___
 * prefix. If a download for the same hash already exists the handle of that
 * download is returned.
 */
AR_EXPORT
ARDownloadHandle ar_download_start (ARSearchHandle search, ARHash hash,
                                    const char *save_path);

/*
 * Restart all incomplete downloads in specified directory. Returns number
 * of restarted downloads or -1 on failure.
 */
AR_EXPORT
int ar_download_restart_dir (const char *dir);

/*
 * Get current state of download.
 */
AR_EXPORT
ARDownloadState ar_download_state (ARDownloadHandle download);

/*
 * Pause download.
 */
AR_EXPORT
as_bool ar_download_pause (ARDownloadHandle download);

/*
 * Resume paused download.
 */
AR_EXPORT
as_bool ar_download_resume (ARDownloadHandle download);

/*
 * Cancel download but don't remove it. It will just be there to look at until
 * you remove it.
 */
AR_EXPORT
as_bool ar_download_cancel (ARDownloadHandle download);

/*
 * Remove cancelled/complete/failed download and free all associated data.
 */
AR_EXPORT
as_bool ar_download_remove (ARDownloadHandle download);

/*****************************************************************************/

/*
 * Cancel upload and free all internal resources.
 */
AR_EXPORT
as_bool ar_upload_cancel (ARUploadHandle upload);

/*****************************************************************************/

/*
 * Begin addition of shares. For efficiency reasons multiple calls to
 * ar_share_add should be sandwiched using ar_share_begin and ar_share_end.
 * Will fail if ar_share_begin has been called before without matching
 * ar_share_end.
 */
AR_EXPORT
as_bool ar_share_begin ();

/*
 * End addition of shares. 
 */
AR_EXPORT
as_bool ar_share_end ();

/*
 * Add share. If share->hash is NULL the hash will be calculated before the
 * function returns (very inefficient to do all the time). If the file does
 * not fall into one of Ares' realms or it is an incomplete download it will
 * not be shared and FALSE will be returned.
 */
AR_EXPORT
as_bool ar_share_add (const ARShare *share);

/*****************************************************************************/

/*
 * Set integer config key to new value.
 */
AR_EXPORT
as_bool ar_config_set_int (ARConfigKey key, as_int32 value);

/*
 * Set string config key to new value.
 */
AR_EXPORT
as_bool ar_config_set_str (ARConfigKey key, const char *value);

/*
 * Get integer config key.
 */
AR_EXPORT
as_int32 ar_config_get_int (ARConfigKey key);

/*
 * Get string config key.
 */
AR_EXPORT
const char *ar_config_get_str (ARConfigKey key);

/*****************************************************************************/

#endif /* __ARES_DLL_H */

