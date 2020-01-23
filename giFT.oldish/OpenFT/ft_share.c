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

	if (stream)
		ft_stream_send (stream, packet);
	else
		ft_packet_send (c, packet);

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

	if (stream)
		ft_stream_send (stream, packet);
	else
		ft_packet_send (c, packet);

	return TRUE;
}

static void submit_sync (Connection *c, FTStream *stream)
{
	ft_stream_finish (stream);
	ft_packet_sendva (c, FT_MODSHARE_REQUEST, 0, "hl",
	                  TRUE, upload_availability ());
}

/*****************************************************************************/

static Connection *local_flush (Connection *c, Node *node, void *udata)
{
	/* remove our shares from this node */
	ft_packet_sendva (c, FT_REMSHARE_REQUEST, 0, NULL);
	return NULL;
}

void ft_share_local_flush ()
{
	TRACE_FUNC ();

	conn_foreach ((ConnForeachFunc) local_flush, NULL,
	              NODE_PARENT, NODE_CONNECTED, 0);
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

static Connection *locate_future_parent (Connection *c, Node *node,
                                         void *udata)
{
	if (node->class & NODE_PARENT)
		return NULL;

	return c;
}

static void get_new_parent ()
{
	Connection *c;

	c = conn_foreach ((ConnForeachFunc) locate_future_parent, NULL,
	                  NODE_SEARCH, NODE_CONNECTED, 0);

	/* request this parent authorize us as a child */
	if (c)
		ft_packet_sendva (c, FT_CHILD_REQUEST, 0, NULL);
}

/*****************************************************************************/

static int submit_write (Connection *c, FileShare *file, FTStream *stream)
{
	if (!file)
	{
		submit_sync (c, stream);
		return FALSE;
	}

	if (!submit_addshare (c, stream, file))
		return TRUE;

	return FALSE;
}

static int submit_destroy (Connection *c, FileShare *file, FTStream *stream)
{
	return FALSE;
}

void ft_share_local_submit (Connection *c)
{
	Dataset  *shares;
	FTStream *stream;

	if (!(shares = share_index (NULL, NULL)))
		return;

	if (!c)
	{
		get_new_parent ();
		return;
	}

	TRACE_SOCK (("submitting shares..."));

	if (!(stream = ft_stream_get (c, FT_STREAM_SEND, NULL)))
		TRACE_SOCK (("unable to fetch a new stream, proceeding without"));

	queue_add (c,
	           (QueueWriteFunc) submit_write,
	           (QueueWriteFunc) submit_destroy,
	           dataset_flatten (shares), stream);
}

/*****************************************************************************/

static FTSHost *openft_shost (Node *node)
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

static Connection *share_add (Connection *c, Node *node, FileShare *file)
{
	if (FT_SESSION(c)->submit)
		submit_addshare (c, FT_SESSION(c)->submit, file);

	return NULL;
}

int openft_share_add (Protocol *p, FileShare *file, void *data)
{
	conn_foreach ((ConnForeachFunc)share_add, file,
	              NODE_PARENT, NODE_CONNECTED, 0);

	return TRUE;
}

static Connection *share_remove (Connection *c, Node *node, FileShare *file)
{
	if (FT_SESSION(c)->submit)
		submit_remshare (c, FT_SESSION(c)->submit_del, file);

	return NULL;
}

int openft_share_remove (Protocol *p, FileShare *file, void *data)
{
	conn_foreach ((ConnForeachFunc)share_remove, file,
	              NODE_PARENT, NODE_CONNECTED, 0);

	return TRUE;
}

/*****************************************************************************/

static Connection *share_sync (Connection *c, Node *node, int *begin)
{
	if (*begin)
	{
		FT_SESSION(c)->submit = ft_stream_get (c, FT_STREAM_SEND, NULL);
		FT_SESSION(c)->submit_del = ft_stream_get (c, FT_STREAM_SEND, NULL);
	}
	else if (FT_SESSION(c)->submit)
	{
		submit_sync (c, FT_SESSION(c)->submit);
		ft_stream_finish (FT_SESSION(c)->submit_del);

		FT_SESSION(c)->submit = NULL;
		FT_SESSION(c)->submit_del = NULL;
	}

	return NULL;
}

void openft_share_sync (Protocol *p, int start)
{
	TRACE (("%s share sync...", (start ? "beginning" : "finishing")));

	conn_foreach ((ConnForeachFunc)share_sync, &start,
	              NODE_PARENT, NODE_CONNECTED, 0);

	html_cache_flush ("shares");
}

/*****************************************************************************/

static Connection *send_avail (Connection *c, Node *node, unsigned long *av)
{
	ft_packet_sendva (c, FT_MODSHARE_REQUEST, 0, "l", *av);
	return NULL;
}

static void broadcast_avail (unsigned long avail)
{
	conn_foreach ((ConnForeachFunc)send_avail, &avail,
	              NODE_PARENT, NODE_CONNECTED, 0);
}

void openft_share_hide (Protocol *p)
{
	assert (upload_availability () == 0);
	broadcast_avail (0);
}

void openft_share_show (Protocol *p, unsigned long avail)
{
	broadcast_avail (avail);
}
