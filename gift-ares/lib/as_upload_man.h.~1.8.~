/*
 * $Id: as_upload_man.h,v 1.8 2004/12/04 11:37:41 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_UPLOAD_MAN_H
#define __AS_UPLOAD_MAN_H

/*****************************************************************************/

typedef struct as_upman_t ASUpMan;

/* Called for every state change of a managed upload. Return FALSE if the
 * upload was freed and must no longer be accessed.
 */
typedef as_bool (*ASUpManStateCb) (ASUpMan *man, ASUpload *up,
                                   ASUploadState state);

/* Called before every upload to decide if it should be started. Return one
 * of the following values:
 *   -1  Upload is not started and requester is sent 503 without a specific
 *       queue position.
 *    0  Upload starts immediately.
 *   >0  Upload is not started and return value is sent as queue position to
 *       requester.
 * The callback should also set queue_length if available.
 */
typedef as_bool (*ASUpManAuthCb) (ASUpMan *man, ASUpload *up,
                                  int *queue_length);

/* Called every AS_UPLOAD_PROGRESS_INTERVAL while there are active
 * uploads.
 */
typedef void (*ASUpManProgressCb) (ASUpMan *man);

struct as_upman_t
{
	List *uploads;        /* list of uploads */
	List *queue;          /* list of struct queues */
	
	int max_active;       /* max concurrent uploads */
	int nuploads;         /* Number of _active_ downloads. Not the
	                       * necessarily same as number of uploads in hash
	                       * table! */
	int nqueued;          /* Number of queued hosts */
	int bandwidth;

	/* The state callback we trigger for all uploads */
	ASUpManStateCb state_cb;
	ASUpManAuthCb auth_cb;
	/* Periodic progress callback */
	ASUpManProgressCb progress_cb;
	timer_id progress_timer;
};

/*****************************************************************************/

/* Create upload manager. */
ASUpMan *as_upman_create ();

/* Free upload manager and stop all uploads. */
void as_upman_free (ASUpMan *man);

/* Set callback triggered for every state change in one of the uploads. */
void as_upman_set_state_cb (ASUpMan *man, ASUpManStateCb state_cb);

/* Set callback before every upload to authorize transfer. */
void as_upman_set_auth_cb (ASUpMan *man, ASUpManAuthCb auth_cb);

/* Set callback triggered periodically for progress updates. */
void as_upman_set_progress_cb (ASUpMan *man,
                               ASUpManProgressCb progress_cb);

/*****************************************************************************/

/* Create and register a new upload from http request. Takes ownership of
 * connection and request in all cases (even if no download is created).
 */
ASUpload *as_upman_start (ASUpMan *man, TCPC *c, ASHttpHeader *req);

/*****************************************************************************/

/* Cancel upload but do not remove it. */
as_bool as_upman_cancel (ASUpMan *man, ASUpload *up);

/* Remove and free finished, failed or cancelled upload. */
as_bool as_upman_remove (ASUpMan *man, ASUpload *up);

/* Return state of upload. The advantage to as_upload_state is that this
 * makes sure the upload is actually still in the list and thus valid
 * before accessing it. If the upload is invalid UPLOAD_INVALID is
 * returned.
 */
ASUploadState as_upman_state (ASUpMan *man, ASUpload *up);

/*****************************************************************************/

#endif /* __AS_UPLOAD_MAN_H */