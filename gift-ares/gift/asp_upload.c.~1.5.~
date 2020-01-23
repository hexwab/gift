/*
 * $Id: asp_upload.c,v 1.5 2004/12/04 19:17:11 mkern Exp $
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

/* Returns static user name string from upload. */
static const char *upload_to_user (ASUpload *up)
{
	char *user = NULL;

	if (STRING_NULL (up->username))
		user = net_ip_str (up->host);
	else
		user = stringf ("%s@%s", up->username, net_ip_str (up->host));

	return user;
}

static void wrote (ASUpload *up, int len)
{
	Chunk *chunk = up->udata;
	unsigned int dummy_buffer = 0xCCCCCCCC;
	assert (chunk);

	/* yes, giFT checks for a NULL segment pointer, but never
	 * actually dereferences it... ugh */
	PROTO->chunk_write (PROTO, chunk->transfer, chunk, chunk->source,
	                    (void *)&dummy_buffer, len);
}

/* Send upload progress to giFT. */
static as_bool send_progress (ASUpload *up)
{
	Chunk *chunk = up->udata;

	assert (chunk);

/*
	if (chunk->transmit == up->sent)
		return FALSE;
*/

	wrote (up, up->sent - chunk->transmit);

	return TRUE;
}

/*****************************************************************************/

as_bool up_data_cb (ASUpload *up, as_uint32 sent)
{
	wrote (up, sent);
	
	/* Since giFT will call asp_giftcb_upload_stop if this completes the
	 * upload we don't know at this point if we were freed at this point.
	 * We thus always return FALSE with the side effect that UPLOAD_COMPLETE
	 * is never triggered by the ares library. 
	 */
	return FALSE;
}

/* Called by ares library for upload state changes. */
static as_bool up_state_cb (ASUpMan *man, ASUpload *up,
                            ASUploadState state)
{
	Share *share = up->share->udata;
	Chunk *chunk;
	Transfer *transfer;

#if 1
	AS_HEAVY_DBG_2("Upload state for %s: %s", net_ip_str (up->host),
	               as_upload_state_str (up));
#endif

	switch (state)
	{
	case UPLOAD_ACTIVE:
		transfer = PROTO->upload_start (PROTO, &chunk, upload_to_user (up),
		                                share, up->start, up->stop);

		if (!transfer)
		{
			AS_ERR_1 ("Failed to create giFT transfer object for upload to %s",
			          net_ip_str (up->host));
			as_upman_cancel (AS->upman, up);
			as_upman_remove (AS->upman, up);
			return FALSE; /* Upload was freed. */
		}

		assert (chunk->transfer == transfer);
		
		up->udata = chunk;
		chunk->udata = up;

		as_upload_set_data_cb (up, (ASUploadDataCb)up_data_cb);

		break;
	case UPLOAD_COMPLETE:
		/* This should never happen since we return FALSE from up_data_cb. */
		assert (0);
		/* May make giFT call asp_giftcb_upload_stop. */
		send_progress (up);
		break;
	case UPLOAD_FAILED:
	case UPLOAD_CANCELLED:
		/* Makes giFT call asp_giftcb_upload_stop. */
		wrote (up, 0);
		return FALSE;
	default:
		abort ();
	}

	return TRUE;
}

/* Called by ares library to determine if a new upload is allowed. */
static as_bool up_auth_cb (ASUpMan *man, ASUpload *up,
                           int *queue_length)
{
	upload_auth_t auth;
	int ret;
	Share *share = up->share->udata;
	const char *user = upload_to_user (up);
	assert (share);

	/* Ask giFT what to do. */
	ret = PROTO->upload_auth (PROTO, user, share, &auth);
	
	switch (ret)
	{
	case UPLOAD_AUTH_ALLOW:
		return 0;
		
	case UPLOAD_AUTH_STALE:
	case UPLOAD_AUTH_MAX_PERUSER:
	case UPLOAD_AUTH_HIDDEN:
		return -1;

	case UPLOAD_AUTH_MAX:
		if (queue_length)
			*queue_length = auth.queue_ttl;
		return auth.queue_pos ? auth.queue_pos : -1;
		
	case UPLOAD_AUTH_NOTSHARED:
		/* can't happen? */
		/* I think it can. */
		assert (0);
		return -1;
		
	default:
		abort ();
	}
}

#if 0
/* Called periodically by ares library while there are active uploads. */
static void up_progress_cb (ASUpMan *man)
{
	List *l;

	for (l = man->uploads; l; l = l->next)
	{
		ASUpload *up = l->data;

		if (up->state == UPLOAD_ACTIVE)
			send_progress (up);
	}
}
#endif

/*****************************************************************************/

/* Register internal upload callbacks with ares library. */
void asp_upload_register_callbacks ()
{
	as_upman_set_state_cb (AS->upman, (ASUpManStateCb)up_state_cb);
	as_upman_set_auth_cb (AS->upman, (ASUpManAuthCb)up_auth_cb);
#if 0
	as_upman_set_progress_cb (AS->upman, (ASUpManProgressCb)up_progress_cb);
#endif
}

/* Called by giFT to stop upload on user's request. */
void asp_giftcb_upload_stop (Protocol *p, Transfer *transfer,
                             Chunk *chunk, Source *source)
{
	ASUpload *up = chunk->udata;
	assert (up);

	AS_HEAVY_DBG_1 ("Stopped upload to %s", net_ip_str (up->host));

	as_upman_remove (AS->upman, up);
	chunk->udata = NULL;
}

/*****************************************************************************/
