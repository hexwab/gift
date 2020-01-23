/*
 * $Id: ar_upload.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
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

static as_bool upman_state_callback (ASUpMan *man, ASUpload *up,
                                     ASUploadState state);
static void upman_progress_callback (ASUpMan *man);

/*****************************************************************************/

/* Set state and progress callbacks with upload manager */
void ar_upload_add_callbacks ()
{
	as_upman_set_state_cb (AS->upman, upman_state_callback);
	as_upman_set_progress_cb (AS->upman, upman_progress_callback);
}

/* Remove state and progress callbacks from upload manager */
void ar_upload_remove_callbacks ()
{
	as_upman_set_state_cb (AS->upman, NULL);
	as_upman_set_progress_cb (AS->upman, NULL);
}

/*****************************************************************************/

static ARUploadState export_upload_state (ASUploadState state)
{
	switch (state)
	{
	case UPLOAD_INVALID:   return AR_UPLOAD_INVALID;
	case UPLOAD_ACTIVE:    return AR_UPLOAD_ACTIVE;
	case UPLOAD_COMPLETE:  return AR_UPLOAD_COMPLETE;
	case UPLOAD_CANCELLED: return AR_UPLOAD_CANCELLED;
	}

	abort ();
}

static as_bool upman_state_callback (ASUpMan *man, ASUpload *up,
                                     ASUploadState state)
{
	ARUpload upload;

	upload.handle = (ARUploadHandle) up;
	upload.state = export_upload_state (up->state);
	upload.path = up->share->path;
	upload.filename = as_get_filename (upload.path);
	upload.filehash = up->share->hash->data;
	upload.filesize = up->share->size;
	upload.start = up->start;
	upload.stop = up->stop;
	upload.sent = up->sent;
	
	upload.ip = up->host;
	upload.username = up->username;

	ar_export_meta (upload.meta, up->share->meta);

	/* send to user app */
	ar_raise_callback (AR_CB_UPLOAD, &upload, NULL);

	return TRUE; /* FIXME: return FALSE if upload was freed */
}

/*****************************************************************************/

static void upman_progress_callback (ASUpMan *man)
{
	ARUploadProgress *progress;
	int active = 0, i = 0;
	List *l;

	/* Upload manager caches number of active uploads */
	active = man->nuploads;
	assert (active > 0);

	if (!(progress = malloc (sizeof (ARUploadProgress) + 
	                         sizeof (progress->uploads) * (active - 1))))
	{
		AS_ERR ("Insufficient memory.");
		return;
	}

	progress->upload_count = active;

	/* copy progress data for active uploads */
	for (l = man->uploads, i = 0; l; l = l->next)
	{
		ASUpload *up = l->data;

		if (as_upload_state (up) == UPLOAD_ACTIVE)
		{
			assert (i < active);
			progress->uploads[i].handle = (ARUploadHandle) up;
			progress->uploads[i].start = up->start;
			progress->uploads[i].stop = up->stop;
			progress->uploads[i].sent= up->sent;
			i++;
		}
	}

	assert (i == active);

	/* send to user app */
	ar_raise_callback (AR_CB_PROGRESS, NULL, progress);
}

/*****************************************************************************/

/*
 * Cancel upload and free all internal resources.
 */
as_bool ar_upload_cancel (ARUploadHandle upload)
{
	if (!ar_events_pause ())
		return FALSE;

	/* Cancel upload */
	if (!as_upman_cancel (AS->upman, (ASUpload *)upload))
	{
		AS_ERR_1 ("Couldn't cancel upload to %s",
		          net_ip_str (((ASUpload *)upload)->host));
		ar_events_resume ();
		return FALSE;
	}

	/* Remove cancelled upload */
	if (!as_upman_remove (AS->upman, (ASUpload *)upload))
	{
		AS_ERR_1 ("Couldn't remove cancelled upload to %s",
		          net_ip_str (((ASUpload *)upload)->host));
		ar_events_resume ();
		return FALSE;
	}

	ar_events_resume ();
	return TRUE;
}

/*****************************************************************************/
