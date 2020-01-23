/*
 * $Id: fst_push.c,v 1.2 2003/12/24 22:41:40 mkern Exp $
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
#include "fst_push.h"

/*****************************************************************************/

/* called by http server for every received reply */
int fst_push_process_reply (FSTHttpServer *server, TCPC *tcpcon,
							unsigned int push_id)
{
	FSTPush *push;

	FST_HEAVY_DBG_1 ("received push reply with id %d, requesting download",
	                 push_id);

	if (! (push = fst_pushlist_lookup_id (FST_PLUGIN->pushlist, push_id)))
	{
		FST_DBG_1 ("push with id %d not in push list", push_id);
		return FALSE;
	}

	fst_pushlist_remove (FST_PLUGIN->pushlist, push);
	
	assert (push->source->chunk);

	if (!fst_download_start (push->source, tcpcon))
	{
		FST_DBG ("fst_download_start failed");
		fst_push_free (push);
		return FALSE;
	}

	fst_push_free (push);

	return TRUE;
}

/*****************************************************************************/

/* alloc and init push */
FSTPush *fst_push_create (Source *source, unsigned int push_id)
{
	FSTPush *push;

	if (! (push = malloc (sizeof (FSTPush))))
		return NULL;

	push->source = source;
	push->push_id = push_id;

	return push;
}

/* free push */
void fst_push_free (FSTPush *push)
{
	if (!push)
		return;

	free (push);
}

/* send push request to supernode */
int fst_push_send_request (FSTPush *push, FSTSession *session)
{
	FSTPacket *packet;
	unsigned char *hash;
	char *username;
	in_addr_t ip, shost;
	in_port_t port, sport;
	Dataset *params;

	if (!push || !session)
		return FALSE;

	if (!FST_PLUGIN->server)
		return FALSE;

	if (FST_PLUGIN->external_ip != FST_PLUGIN->local_ip &&
		!FST_PLUGIN->forwarding)
	{
		FST_DBG_1 ("NAT detected but port is not forwarded, not requesting push for %s",
				   push->source->url);
		return FALSE;
	}

	/* get everything we need from url */
	if (! (hash = fst_download_parse_url (push->source->url, &ip, &port, &params)))
	{
		FST_WARN_1 ("malformed url %s", push->source->url);
		return FALSE;
	}

	shost = net_ip (dataset_lookupstr (params, "shost"));
	sport = ATOI (dataset_lookupstr (params, "sport"));
	username = gift_strdup (dataset_lookupstr (params, "uname"));

	free (hash);
	dataset_clear (params);

	if (!shost || !sport || !username)
	{
		FST_WARN_1 ("malformed url %s", push->source->url);
		free (username);
		return FALSE;	
	}

	if (! (packet = fst_packet_create()))
	{
		free (username);
		return FALSE;
	}

	/* push id */
	fst_packet_put_uint32 (packet, htonl (push->push_id));
	/* ip and port the push shall be sent to */
	fst_packet_put_uint32 (packet, FST_PLUGIN->external_ip);
	fst_packet_put_uint16 (packet, htons(FST_PLUGIN->server->port));
	/* ip and port of pushing user */
	fst_packet_put_uint32 (packet, ip);
	fst_packet_put_uint16 (packet, htons(port));
	/* ip and port of pushing user's supernode */
	fst_packet_put_uint32 (packet, shost);
	fst_packet_put_uint16 (packet, htons(sport));
	/* pushing user's name */
	fst_packet_put_ustr (packet, username, strlen (username));

	free (username);

	/* now send it */
	if (!fst_session_send_message (session, SessMsgPushRequest, packet))
	{
		fst_packet_free (packet);
		return FALSE;
	}

	fst_packet_free (packet);

	FST_HEAVY_DBG_2 ("sent push request for source %s with id %d",
					 push->source->url, push->push_id);

	return TRUE;
}

/*****************************************************************************/

/* alloc and init push list */
FSTPushList *fst_pushlist_create ()
{
	FSTPushList *pushlist;

	if (! (pushlist = malloc (sizeof (FSTPushList))))
		return NULL;

	pushlist->list = NULL;
	pushlist->next_push_id = 1;

	return pushlist;
}

/* remove push */
static int pushlist_free_push (FSTPush *push, void *udata)
{
	fst_push_free (push);
	return TRUE;
}

/* free push list, frees all pushes */
void fst_pushlist_free (FSTPushList *pushlist)
{
	if (!pushlist)
		return;

	list_foreach_remove (pushlist->list, (ListForeachFunc)pushlist_free_push,
						 NULL);

	free (pushlist);
}

/*****************************************************************************/

/* add push for source to pushlist if it's not already added,
 * returns already added or new push */
FSTPush *fst_pushlist_add (FSTPushList *pushlist, Source *source)
{
	FSTPush *push;

	if (!pushlist || !source)
		return NULL;

	if ((push = fst_pushlist_lookup_source (pushlist, source)))
		return push;

	if (! (push = fst_push_create (source, pushlist->next_push_id++)))
		return NULL;

	pushlist->list = list_prepend (pushlist->list, push);

	return push;
}

/* remove push from list, does not free push, return removed push */
FSTPush *fst_pushlist_remove (FSTPushList *pushlist, FSTPush *push)
{
	if (!pushlist || !push)
		return NULL;

	pushlist->list = list_remove (pushlist->list, push);
	
	return push;
}

/*****************************************************************************/

static int pushlist_cmp_source (FSTPush *a, Source *source)
{
	return a->source != source;
}

static int pushlist_cmp_id (FSTPush *a, unsigned int push_id)
{
	return a->push_id != push_id;
}

/* lookup push by source */
FSTPush *fst_pushlist_lookup_source (FSTPushList *pushlist, Source *source)
{
	List *item;

	if (!pushlist || !source)
		return NULL;

	item = list_find_custom (pushlist->list, (void*) source,
							 (CompareFunc) pushlist_cmp_source);

	if (!item)
		return NULL;

	return (FSTPush*) item->data;
}

/* lookup push by push id */
FSTPush *fst_pushlist_lookup_id (FSTPushList *pushlist, unsigned int push_id)
{
	List *item;

	if (!pushlist || !push_id)
		return NULL;

	item = list_find_custom (pushlist->list, (void*) push_id,
							 (CompareFunc) pushlist_cmp_id);

	if (!item)
		return NULL;

	return (FSTPush*) item->data;
}

/*****************************************************************************/
