/*
 * $Id: fst_share.c,v 1.4 2003/11/29 13:33:30 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_share.h"

/*****************************************************************************/

/* FIXME: imported from giFT, we need this in Protocol */
extern Dataset *share_index (unsigned long *files, double *size);

/*****************************************************************************/

int share_register_file (Share *share);

int share_unregister_file (Share *share);

/*****************************************************************************/

/* called by giFT so we can add custom data to shares */
void *fst_giftcb_share_new (Protocol *p, Share *share)
{
	FST_HEAVY_DBG_1 ("%s", share->path);
	return NULL;
}

/* called be giFT for us to free custom data */
void fst_giftcb_share_free (Protocol *p, Share *share, void *data)
{
	FST_HEAVY_DBG_1 ("%s", share->path);
}

/* called by giFT when share is added */
BOOL fst_giftcb_share_add (Protocol *p, Share *share, void *data)
{
	if (!fst_share_do_share ())
		return FALSE;
	
	if (!FST_PLUGIN->session || FST_PLUGIN->session->state != SessEstablished)
		return FALSE;

	FST_HEAVY_DBG_1 ("registering share %s", share->path);

	if (!share_register_file (share))
	{
		FST_DBG_1 ("registering share %s failed", share->path);
		return FALSE;
	}

	return TRUE;
}

/* called by giFT when share is removed */
BOOL fst_giftcb_share_remove (Protocol *p, Share *share, void *data)
{
	if (!fst_share_do_share ())
		return FALSE;

	if (!FST_PLUGIN->session || FST_PLUGIN->session->state != SessEstablished)
		return TRUE; /* removed since we didn't share anyway */

	FST_HEAVY_DBG_1 ("unregistering share %s", share->path);

	if (!share_unregister_file (share))
	{
		FST_DBG_1 ("unregistering share %s failed", share->path);
		return FALSE;
	}

	return TRUE;
}

/* called by giFT when it starts/ends syncing shares */
void fst_giftcb_share_sync (Protocol *p, int begin)
{
	FST_HEAVY_DBG_1 ("share sync begin: %d", begin);
}

/* called by giFT when user hides shares */
void fst_giftcb_share_hide (Protocol *p)
{
	FST_HEAVY_DBG ("hiding shares...");

	if (!fst_share_do_share ())
		return;

	if (FST_PLUGIN->session && FST_PLUGIN->session->state == SessEstablished)
	{
		FST_DBG ("hiding shares by removing them from supernode");

		if (!fst_share_unregister_all ())
			FST_DBG ("uregistering all shares failed");
	}

	FST_PLUGIN->hide_shares = TRUE;
}

/* called by giFT when user shows shares */
void fst_giftcb_share_show (Protocol *p)
{
	FST_HEAVY_DBG ("showing shares...");

	FST_PLUGIN->hide_shares = FALSE;
	
	if (!fst_share_do_share ())
		return;

	if (FST_PLUGIN->session && FST_PLUGIN->session->state == SessEstablished)
	{
		FST_DBG ("showing shares by registering them with supernode");

		if (!fst_share_register_all ())
			FST_DBG ("registering all shares failed");
	}
}

/*****************************************************************************/

int fst_share_do_share ()
{
	if (!FST_PLUGIN->server)
		return FALSE;
		
	/* we currently don't support uploading via pushing */
	if (FST_PLUGIN->external_ip != FST_PLUGIN->local_ip &&
		!FST_PLUGIN->forwarding)
		return FALSE;

	if (!FST_PLUGIN->allow_sharing)
		return FALSE;
		
	if (FST_PLUGIN->hide_shares)
		return FALSE;

	return TRUE;
}


static void share_register_all_iter (ds_data_t *key, ds_data_t *value,
                                     int *all_ok)
{
	Share *share = value->data;

	if (!share_register_file (share))
		*all_ok = FALSE;
}

/* send all shares to supernode */
int fst_share_register_all ()
{
	Dataset *shares;
	int all_ok = TRUE;

	if (!fst_share_do_share ())
		return FALSE;

	if (!FST_PLUGIN->session || FST_PLUGIN->session->state != SessEstablished)
	{
		FST_DBG ("tried to register shares with no connection to supernode");
		return FALSE;
	}

	/* FIXME: this isnt technically accessible by plugins! */
	if (!(shares = share_index (NULL, NULL)))
		return FALSE;

	/* loop through all shares and send the to the supernode */
	dataset_foreach (shares, DS_FOREACH(share_register_all_iter), (void*)&all_ok);

	if (!all_ok)
	{
		FST_DBG ("not all shares could be registered with supernode");
		return FALSE;
	}

	return TRUE;
}

static void share_unregister_all_iter (ds_data_t *key, ds_data_t *value,
                                       int *all_ok)
{
	Share *share = value->data;

	if (!share_unregister_file (share))
		*all_ok = FALSE;
}

/* remove all shares from supernode */
int fst_share_unregister_all ()
{
	Dataset *shares;
	int all_ok = TRUE;

	if (!fst_share_do_share ())
		return FALSE;

	if (!FST_PLUGIN->session || FST_PLUGIN->session->state != SessEstablished)
	{
		FST_DBG ("tried to unregister shares with no connection to supernode");
		return FALSE;
	}

	/* FIXME: this isnt technically accessible by plugins! */
	if (!(shares = share_index (NULL, NULL)))
		return FALSE;

	/* loop through all shares and remove them from supernode */
	dataset_foreach (shares, DS_FOREACH(share_unregister_all_iter), (void*)&all_ok);

	if (!all_ok)
	{
		FST_DBG ("not all shares could be unregistered from supernode");
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

typedef struct
{
	FSTPacket *data;
	int ntags;
} ShareAddTagParam;

/* adds meta tag to packet */
static void share_add_meta_tag (ds_data_t *key, ds_data_t *value,
                                ShareAddTagParam *tag_param)
{
	FSTPacket *data;

	if (! (data = fst_meta_packet_from_giftstr (key->data, value->data)))
		return;

	fst_packet_rewind (data);
	fst_packet_append (tag_param->data, data);
	fst_packet_free (data);
	tag_param->ntags++;

	FST_HEAVY_DBG_2 ("\tmeta tag \"%s\" = \"%s\" to share", key->data, value->data);

}

static void share_add_filename (Share *share, ShareAddTagParam *tag_param)
{
	FSTPacket *data = tag_param->data;
	char *filename;
	int len;
	
	filename = file_basename (share->path);

	if (!filename)
		return;

	len = strlen (filename);
	fst_packet_put_uint8 (data, FILE_TAG_FILENAME);
	fst_packet_put_dynint (data, len);
	fst_packet_put_ustr (data, (unsigned char*)filename, len);
	tag_param->ntags++;
}


int share_register_file (Share *share)
{
	FSTPacket *packet;
	Hash *hash;
	ShareAddTagParam tag_param;
	FSTMediaType media_type;

	if (!share)
		return FALSE;

	if (!(packet = fst_packet_create()))
		return FALSE;

	FST_HEAVY_DBG_1 ("registering file \"%s\"", share->path);

	/* unknown */
	fst_packet_put_ustr (packet, "\x00", 1);

	/* media type */
	media_type = fst_meta_mediatype_from_mime (share->mime);
	fst_packet_put_uint8 (packet, media_type);
	FST_HEAVY_DBG_1 ("\tmedia_type: 0x%02X", media_type);
	
	/* unknown */
	fst_packet_put_ustr (packet, "\x00\x00", 2);

	/* hash */
	if (! (hash = share_get_hash (share, FST_HASH_NAME)))
	{
		fst_packet_free (packet);
		return FALSE;
	}
	assert (hash->len == FST_HASH_LEN);
	fst_packet_put_ustr (packet, hash->data, FST_HASH_LEN);

	/* checksum */
	fst_packet_put_dynint (packet, fst_hash_checksum (hash->data));

	/* file size */
	fst_packet_put_dynint (packet, share->size);


	/* collect tags */
	if (!(tag_param.data = fst_packet_create()))
	{
		fst_packet_free (packet);
		return FALSE;
	}
	tag_param.ntags = 0;

	share_add_filename (share, &tag_param);

	share_foreach_meta (share,
	                    (DatasetForeachFn) share_add_meta_tag,
						(void*)&tag_param);

	/* number of meta tags */
	fst_packet_put_dynint (packet, tag_param.ntags);
	/* append data */
	fst_packet_rewind (tag_param.data);
	fst_packet_append (packet, tag_param.data);
	fst_packet_free (tag_param.data);


	/* now send it */
	if (!fst_session_send_message (FST_PLUGIN->session, SessMsgShareFile, packet))
	{
		fst_packet_free (packet);
		return FALSE;
	}

	fst_packet_free (packet);
	return TRUE;
}

int share_unregister_file (Share *share)
{
	FSTPacket *packet;
	Hash *hash;
	ShareAddTagParam tag_param;

	if (!share)
		return FALSE;

	if (!(packet = fst_packet_create()))
		return FALSE;

	FST_HEAVY_DBG_1 ("unregistering file \"%s\"", share->path);

	/* hash */
	if (! (hash = share_get_hash (share, FST_HASH_NAME)))
	{
		fst_packet_free (packet);
		return FALSE;
	}
	assert (hash->len == FST_HASH_LEN);
	fst_packet_put_ustr (packet, hash->data, FST_HASH_LEN);

	/* checksum */
	fst_packet_put_dynint (packet, fst_hash_checksum (hash->data));

	/* file size */
	fst_packet_put_dynint (packet, share->size);


	/* collect tags */
	if (!(tag_param.data = fst_packet_create()))
	{
		fst_packet_free (packet);
		return FALSE;
	}
	tag_param.ntags = 0;

	share_add_filename (share, &tag_param);

	share_foreach_meta (share,
	                    (DatasetForeachFn) share_add_meta_tag,
						(void*)&tag_param);

	/* number of meta tags */
	fst_packet_put_dynint (packet, tag_param.ntags);
	/* append data */
	fst_packet_rewind (tag_param.data);
	fst_packet_append (packet, tag_param.data);
	fst_packet_free (tag_param.data);


	/* now send it */
	if (!fst_session_send_message (FST_PLUGIN->session, SessMsgUnshareFile, packet))
	{
		fst_packet_free (packet);
		return FALSE;
	}

	fst_packet_free (packet);
	return TRUE;
}

/*****************************************************************************/
