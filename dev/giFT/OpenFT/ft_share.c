/*
 * ft_share.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include "ft_netorg.h"
#include "ft_html.h"

#include "queue.h"
#include "file.h"
#include "meta.h"

#include "ft_share.h"

/*****************************************************************************/

/* linked to by shares from giFT's share cache */
static FTSHost *local_shost = NULL;

/*****************************************************************************/

static int set_meta (Dataset *d, DatasetNode *node, FileShare *file)
{
	meta_set (file, node->key, node->value);
	return FALSE;
}

/* i dont think this is used, hmm. */
void ft_share_add (int verified, in_addr_t host, unsigned short port,
                   unsigned short http_port, char *alias, off_t size,
                   unsigned char *md5, char *mime, char *filename,
                   Dataset *meta)
{
	FileShare *file;
	FTSHost   *shost;

	if (!(shost = ft_shost_get (host)))
	{
		if (!(shost = ft_shost_new (verified, host, port, http_port, alias)))
			return;

		ft_shost_add (shost);
	}

	/* create the actual fileshare structure for insertion */
	file = ft_share_new (shost, size, md5, mime, filename);

	if (!ft_share_complete (file))
	{
		ft_share_unref (file);
		return;
	}

	dataset_foreach (meta, DATASET_FOREACH (set_meta), file);

	ft_shost_add_file (shost, file);
	ft_share_unref (file);
}

void ft_share_remove (in_addr_t host, unsigned char *md5)
{
	FTSHost *shost;

	if (!(shost = ft_shost_get (host)))
		return;

	ft_shost_remove_file (shost, md5);
}

/*****************************************************************************/

static int add_meta (Dataset *d, DatasetNode *node, FTPacket *packet)
{
	ft_packet_put_str (packet, node->key);
	ft_packet_put_str (packet, node->value);
	return FALSE;
}

static void send_packet (Connection *c, FTStream *stream, FTPacket *packet)
{
	if (stream)
		ft_stream_send (stream, packet);
	else
		ft_packet_send (c, packet);
}

static int submit_addshare (Connection *c, FTStream *stream, FileShare *file)
{
	FTPacket  *packet;
	ShareHash *sh;
	char      *path;

	if (!(packet = ft_packet_new (FT_ADDSHARE_REQUEST, 0)))
		return FALSE;

	if (!(sh = share_hash_get (file, "MD5")))
		return FALSE;

	/* this really shouldnt happen, but its not technically a fatal error
	 * so... */
	if (!(path = SHARE_DATA(file)->hpath))
	{
		path = SHARE_DATA(file)->path;
		TRACE (("exposing direct path: %s", path));
	}

	assert (path != NULL);

	ft_packet_put_uint32 (packet, (ft_uint32)file->size, TRUE);
	ft_packet_put_str    (packet, file->mime);
	ft_packet_put_ustr   (packet, sh->hash, sh->len);
	ft_packet_put_str    (packet, path);

	meta_foreach (file, DATASET_FOREACH (add_meta), packet);

	send_packet (c, stream, packet);
	return TRUE;
}

static int submit_remshare (Connection *c, FTStream *stream, FileShare *file)
{
	FTPacket  *packet;
	ShareHash *sh;

	if (!(packet = ft_packet_new (FT_REMSHARE_REQUEST, 0)))
		return FALSE;

	if (!(sh = share_hash_get (file, "MD5")))
		return FALSE;

	ft_packet_put_ustr (packet, sh->hash, sh->len);

	send_packet (c, stream, packet);
	return TRUE;
}

static void submit_sync (Connection *c, FTStream *stream)
{
	ft_stream_finish (stream);
	ft_packet_sendva (c, FT_MODSHARE_REQUEST, 0, "hl",
	                  TRUE, upload_availability ());
}

/*****************************************************************************/

static int local_flush (FTNode *node, void *udata)
{
	/* remove our shares from this node */
	ft_packet_sendva (FT_CONN(node), FT_REMSHARE_REQUEST, 0, NULL);
	return TRUE;
}

void ft_share_local_flush ()
{
	TRACE_FUNC ();

	ft_netorg_foreach (NODE_PARENT, NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(local_flush), NULL);
}

/*****************************************************************************/

#if 0
static int local_cleanup (Dataset *d, DatasetNode *node, void *udata)
{
	FileShare *file = node->value;
	FTShare  *share;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return TRUE;

	/* get rid of the extra data */
	ft_share_free_data (file);
	return TRUE;
}
#endif

/*
 * Forcefully cleanup all shares.
 *
 * NOTE:
 * This should be done by giFT, so it has been disabled here.  It currently
 * is not performed by giFT, however.
 */
void ft_share_local_cleanup ()
{
#if 0
	share_foreach (DATASET_FOREACH (local_cleanup), NULL);
#endif
}

/*****************************************************************************/

static int locate_future_parent (FTNode *node, FTNode **parent)
{
	if (node->klass & NODE_PARENT)
		return FALSE;

	*parent = node;
	return TRUE;
}

static void get_new_parent ()
{
	FTNode *node = NULL;

	/* bail on the first suitable non-parent search node we find, setting
	 * c's value */
	ft_netorg_foreach (NODE_SEARCH, NODE_CONNECTED, 1,
	                   FT_NETORG_FOREACH(locate_future_parent), &node);

	/* request this parent authorize us as a child */
	if (node)
		ft_packet_sendva (FT_CONN(node), FT_CHILD_REQUEST, 0, NULL);
}

/*****************************************************************************/

static int submit_write (Dataset *d, DatasetNode *node, void **args)
{
	Connection *c      = args[0];
	FTStream   *stream = args[1];
	FileShare  *file   = node->value;

	if (!(submit_addshare (c, stream, file)))
		TRACE_SOCK (("unable to submit %p", file));

	return FALSE;
}

void ft_share_local_submit (Connection *c)
{
	Dataset  *shares;
	FTStream *stream;
	void     *args[2];

	if (!(shares = share_index (NULL, NULL)))
		return;

	if (!c)
	{
		get_new_parent ();
		return;
	}

	TRACE_SOCK (("submitting shares..."));

	/* we don't require a compression stream here, but it sure would be
	 * nice :) */
	if (!(stream = ft_stream_get (c, FT_STREAM_SEND, NULL)))
		TRACE_SOCK (("unable to fetch a new stream, proceeding without"));

	/*
	 * Loop through all local shares issueing a direct stream write, this
	 * will require an excess blocking compression stage and additional
	 * buffering than would normally required, but it's much better than
	 * requiring a dataset_flatten here to use the queue subsystem.
	 * Likewise, we would need to loop the structure anyway in order to
	 * increment the reference counts while we access our local shares.
	 */
	args[0] = c; args[1] = stream;
	dataset_foreach (shares, DATASET_FOREACH (submit_write), args);

	/* cleanup the stream buffers, we are still holding buffered packets in
	 * the queue subsystem through ft_packet_send, so don't worry about a
	 * race condition here */
	submit_sync (c, stream);
}

/*****************************************************************************/

static FTSHost *openft_shost (FTNode *node)
{
	if (local_shost)
		return local_shost;

	local_shost = ft_shost_new (TRUE, node->ip, node->port, node->http_port,
	                            node->alias);

	return local_shost;
}

void *openft_share_new (Protocol *p, FileShare *file)
{
	FTSHost *shost;

	if (!(shost = openft_shost (FT_SELF)))
		return NULL;

	return ft_share_new_data (file, shost);
}

void openft_share_free (Protocol *p, FileShare *file, void *data)
{
	ft_share_free_data (file, data);
}

/*****************************************************************************/

static int share_add (FTNode *node, FileShare *file)
{
	if (node->session->submit)
		submit_addshare (FT_CONN(node), node->session->submit, file);

	return TRUE;
}

int openft_share_add (Protocol *p, FileShare *file, void *data)
{
	ft_netorg_foreach (NODE_PARENT, NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(share_add), file);
	return TRUE;
}

static int share_remove (FTNode *node, FileShare *file)
{
	if (node->session->submit)
		submit_remshare (FT_CONN(node), node->session->submit_del, file);

	return TRUE;
}

int openft_share_remove (Protocol *p, FileShare *file, void *data)
{
	ft_netorg_foreach (NODE_PARENT, NODE_CONNECTED, 0,
	                   FT_NETORG_FOREACH(share_remove), file);
	return TRUE;
}

/*****************************************************************************/

static int share_sync (FTNode *node, int *begin)
{
	if (*begin)
	{
		node->session->submit =
			ft_stream_get (FT_CONN(node), FT_STREAM_SEND, NULL);
		node->session->submit_del =
			ft_stream_get (FT_CONN(node), FT_STREAM_SEND, NULL);
	}
	else if (node->session->submit)
	{
		submit_sync (FT_CONN(node), node->session->submit);
		ft_stream_finish (node->session->submit_del);

		node->session->submit = NULL;
		node->session->submit_del = NULL;
	}

	return TRUE;
}

void openft_share_sync (Protocol *p, int start)
{
	TRACE (("%s share sync...", (start ? "beginning" : "finishing")));

	ft_netorg_foreach (NODE_PARENT, NODE_CONNECTED, 0,
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
