/*
 * $Id: ft_share.c,v 1.26 2003/06/26 09:24:21 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "ft_openft.h"

#include "lib/file.h"
#include "src/meta.h"

#include "ft_netorg.h"
#include "ft_html.h"
#include "ft_search_db.h"
#include "ft_xfer.h"

#include "ft_share.h"

/*****************************************************************************/

static void add_meta (ds_data_t *key, ds_data_t *value, FTPacket *packet)
{
	ft_packet_put_str (packet, key->data);
	ft_packet_put_str (packet, value->data);
}

static void send_packet (TCPC *c, FTStream *stream, FTPacket *packet)
{
	if (stream)
		ft_stream_send (stream, packet);
	else
		ft_packet_send (c, packet);
}

static BOOL submit_addshare (TCPC *c, FTStream *stream, Share *share)
{
	FTPacket *pkt;
	Hash     *hash;
	char     *path;

	if (!(pkt = ft_packet_new (FT_SHARE_ADD_REQUEST, 0)))
		return FALSE;

	if (!(hash = share_get_hash (share, "MD5")))
		return FALSE;

	/* get the hidden path (the path without the leading sharing/root) */
	path = share_get_hpath (share);
	assert (path != NULL);

	ft_packet_put_ustr   (pkt, hash->data, hash->len);
	ft_packet_put_str    (pkt, path);
	ft_packet_put_str    (pkt, share->mime);
	ft_packet_put_uint32 (pkt, (uint32_t)share->size, TRUE);

	share_foreach_meta (share, DS_FOREACH(add_meta), pkt);

	send_packet (c, stream, pkt);
	return TRUE;
}

static BOOL submit_remshare (TCPC *c, FTStream *stream, Share *share)
{
	FTPacket *pkt;
	Hash     *hash;

	if (!(pkt = ft_packet_new (FT_SHARE_REMOVE_REQUEST, 0)))
		return FALSE;

	if (!(hash = share_get_hash (share, "MD5")))
		return FALSE;

	ft_packet_put_ustr (pkt, hash->data, hash->len);

	send_packet (c, stream, pkt);
	return TRUE;
}

/*****************************************************************************/

static BOOL share_sync_begin (FTNode *node)
{
	TCPC *c = FT_CONN(node);           /* shorthand */

	/*
	 * This is used both by giFT's request and OpenFT's (when
	 * ft_share_local_submit is called), so in an effort to catch a
	 * potential race condition we will assert the condition where an
	 * active submission is already in progress.
	 */
	assert (node->session->submit == NULL);
	assert (node->session->submit_del == NULL);

	node->session->submit     = ft_stream_get (c, FT_STREAM_SEND, NULL);
	node->session->submit_del = ft_stream_get (c, FT_STREAM_SEND, NULL);

	ft_packet_sendva (c, FT_SHARE_SYNC_BEGIN, 0, NULL);

	return TRUE;
}

static BOOL share_sync_end (FTNode *node)
{
	ft_packet_sendva (FT_CONN(node), FT_SHARE_SYNC_END, 0, NULL);
	ft_packet_sendva (FT_CONN(node), FT_CHILD_PROP, 0, "l",
	                  (uint32_t)(ft_upload_avail()));

	ft_stream_finish (node->session->submit);
	ft_stream_finish (node->session->submit_del);

	node->session->submit = NULL;
	node->session->submit_del = NULL;

	return TRUE;
}

/*****************************************************************************/

static int local_flush (FTNode *node, void *udata)
{
	/* remove our shares from this node */
	ft_packet_sendva (FT_CONN(node), FT_SHARE_REMOVE_REQUEST, 0, NULL);
	return TRUE;
}

void ft_share_local_flush ()
{
	ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(local_flush), NULL);
}

/*****************************************************************************/

static int locate_future_parent (FTNode *node, FTNode **parent)
{
	if (node->klass & FT_NODE_PARENT)
		return FALSE;

	*parent = node;
	return TRUE;
}

static void get_new_parent ()
{
	FTNode *node = NULL;

	/* bail on the first suitable non-parent search node we find, setting
	 * c's value */
	ft_netorg_foreach (FT_NODE_SEARCH, FT_NODE_CONNECTED, 1,
	                   FT_NETORG_FOREACH(locate_future_parent), &node);

	/* request this parent authorize us as a child */
	if (node && node->version >= OPENFT_0_0_9_6)
		ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);
}

/*****************************************************************************/

static void submit_write (ds_data_t *key, ds_data_t *value, FTNode *node)
{
	Share *share = value->data;

	submit_addshare (FT_CONN(node), node->session->submit, share);
}

void ft_share_local_submit (TCPC *c)
{
	Dataset *shares;

	/* FIXME: this isnt technically accessible by plugins! */
	if (!(shares = share_index (NULL, NULL)))
		return;

	/* some whacky exception to find a new parent or something, whatever... */
	if (!c)
	{
		get_new_parent ();
		return;
	}

	FT->DBGSOCK (FT, c, "submitting shares...");

	/* construct the submit streams and notify the remote node */
	if (!(share_sync_begin (FT_NODE(c))))
	{
		FT->DBGSOCK (FT, c, "aborting share submission!");
		return;
	}

	/* hmm, this submission will be uncompressed...should we abort? */
	if (!FT_SESSION(c)->submit)
		FT->DBGSOCK (FT, c, "unable to fetch a new stream, proceeding without");

	/*
	 * Loop through all local shares issueing a direct stream write, this
	 * will require an excess blocking compression stage and additional
	 * buffering than would normally required, but it's much better than
	 * requiring a dataset_flatten here to use the queue subsystem.
	 * Likewise, we would need to loop the structure anyway in order to
	 * increment the reference counts while we access our local shares.
	 */
	dataset_foreach (shares, DS_FOREACH(submit_write), FT_NODE(c));

	/* clean up the stream buffers and notify the remote node of the sync
	 * status change as well as update the new availability */
	share_sync_end (FT_NODE(c));
}

/*****************************************************************************/

void *openft_share_new (Protocol *p, Share *share)
{
	return ft_share_new_data (share, FT_SELF);
}

void openft_share_free (Protocol *p, Share *share, void *data)
{
	ft_share_free_data (share, data);
}

/*****************************************************************************/

static BOOL share_add (FTNode *node, Share *share)
{
	if (node->session->submit)
		submit_addshare (FT_CONN(node), node->session->submit, share);

	return TRUE;
}

BOOL openft_share_add (Protocol *p, Share *share, void *data)
{
	ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(share_add), share);

	return TRUE;
}

static BOOL share_remove (FTNode *node, Share *share)
{
	if (node->session->submit)
		submit_remshare (FT_CONN(node), node->session->submit_del, share);

	return TRUE;
}

BOOL openft_share_remove (Protocol *p, Share *share, void *data)
{
	ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(share_remove), share);
	return TRUE;
}

/*****************************************************************************/

static BOOL share_sync (FTNode *node, int *begin)
{
	BOOL ret;

	if (*begin)
		ret = share_sync_begin (node);
	else
		ret = share_sync_end (node);

	return ret;
}

void openft_share_sync (Protocol *p, int start)
{
	FT->DBGFN (FT, "%s share sync...", (start ? "beginning" : "finishing"));

	ft_netorg_foreach (FT_NODE_PARENT, FT_NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(share_sync), &start);

	html_cache_flush ("shares");
}

/*****************************************************************************/

void openft_share_hide (Protocol *p)
{
	/* nothing */
}

void openft_share_show (Protocol *p)
{
	/* nothing */
}
