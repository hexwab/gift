/*
 * $Id: as_upload.h,v 1.11 2005/11/08 20:17:32 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_UPLOAD_H
#define __AS_UPLOAD_H

/*****************************************************************************/

typedef enum
{
	UPLOAD_INVALID,   /* Used in as_upman_state to signal invalid upload. */
	UPLOAD_NEW,       /* Upload created but not yet started */
	UPLOAD_ACTIVE,    /* Sending data to other host */
	UPLOAD_FAILED,    /* 404 or 503 was sent to other host */
	UPLOAD_QUEUED,    /* Queued reply was sent to other host */
	UPLOAD_COMPLETE,  /* Upload completed successfully */
	UPLOAD_CANCELLED  /* Upload was cancelled remotely or locally */
} ASUploadState;

typedef struct as_upload_t ASUpload;

/* Callback for state changes. Return FALSE if upload was freed. */
typedef as_bool (*ASUploadStateCb) (ASUpload *up, ASUploadState state);

/* Callback triggered by as_upload_start to check queue status. Return one of
 * the following values:
 *   -1  Sends 503 but without specific queue position.
 *    0  Starts sending file.
 *   >0  Sends return value as queue position.
 * The callback should also set queue_length if it is known.
 */
typedef int (*ASUploadAuthCb) (ASUpload *up, int *queue_length);

/* Callback Raised whenever data is sent to remote host. The sent parameter
 * gives the number of bytes sent since the last callback. For the total
 * amount sent use up->sent. Return FALSE if upload was freed in callback.
 */
typedef as_bool (*ASUploadDataCb) (ASUpload *up, as_uint32 sent);

/* Callback raised whenever a block of data is to be uploaded to the other
 * node. It can modify the size of the block uploaded between 0 and max_size
 * by returning the new value.
 */
typedef as_uint32 (*ASUploadThrottleCb) (ASUpload *up, as_uint32 max_size);

struct as_upload_t
{
	TCPC         *c;
	in_addr_t     host;      /* copy of c->host since the connection may be
	                          * used but we still need the ip in the upload
	                          * manager for lookups. */
	char         *username;  /* User name of downloader. */
	ASHttpHeader *request;
	ASPacket     *binary_request;
	as_uint16     enc_key;   /* encryption key for encrypted reply */
	ASShare      *share;     /* copy of share object */
	FILE         *file;
	as_uint32     start, stop, sent;
	input_id      input;

	ASUploadState state;

	ASUploadStateCb    state_cb;
	ASUploadAuthCb     auth_cb;
	ASUploadDataCb     data_cb;
	ASUploadThrottleCb throttle_cb;

	/* pointer to upload manager controlling this upload */
	struct as_upman_t *upman; 

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* Create new upload from HTTP request. Takes ownership of tcp connection and
 * request object if successful.
 */
ASUpload *as_upload_create (TCPC *c, ASHttpHeader *request,
                            ASUploadStateCb state_cb,
                            ASUploadAuthCb auth_cb);

/* Create new upload from binary request. Takes ownership of tcp connection and
 * request object if successful.
 */
ASUpload *as_upload_create_binary (TCPC *c, ASPacket *request,
                                   ASUploadStateCb state_cb,
                                   ASUploadAuthCb auth_cb);

/* Free upload object. */
void as_upload_free (ASUpload *up);

/* Set data callback for upload. */
void as_upload_set_data_cb (ASUpload *up, ASUploadDataCb data_cb);

/* Set throttle callback for upload. */
void as_upload_set_throttle_cb (ASUpload *up, ASUploadThrottleCb throttle_cb);

/*****************************************************************************/

/* Send reply back to requester. Looks up share in shares manager, raises auth
 * callback and sends reply. Either sends 503, 404, queued status, or
 * requested data. Returns FALSE if the connection has been closed after a
 * failure reply was sent and TRUE if data transfer was started.
 */
as_bool as_upload_start (ASUpload *up);

/* Cancel data transfer and close connection. Raises state callback. */
as_bool as_upload_cancel (ASUpload *up);

/*****************************************************************************/

/* Suspend transfer using input_suspend_all on socket. */
as_bool as_upload_suspend (ASUpload *up);

/* Resume transfer using input_resume_all on socket. */
as_bool as_upload_resume (ASUpload *up);

/*****************************************************************************/

/* Returns current upload state */
ASUploadState as_upload_state (ASUpload *up);

/* Return upload state as human readable static string. */
const char *as_upload_state_str (ASUpload *up);

/*****************************************************************************/

#endif /* __AS_UPLOAD_H */
