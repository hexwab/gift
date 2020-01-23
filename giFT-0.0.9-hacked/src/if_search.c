/*
 * $Id: if_search.c,v 1.13 2003/05/30 21:13:45 jasta Exp $
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

#include "giftd.h"

#include "plugin/protocol.h"
#include "plugin/share.h"

#include "share_hash.h"

#include "if_event.h"
#include "if_search.h"

/*****************************************************************************/

static void send_cancel (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	Protocol *p = value->data;

	p->search_cancel (p, event);
}

static void search_finished (TCPC *c, IFEvent *event, void *udata)
{
	protocol_foreach (DS_FOREACH(send_cancel), event);
}

IFEvent *if_search_new (TCPC *c, if_event_id requested, IFSearchType type,
                        char *query, char *exclude, char *realm, Dataset *meta)
{
	IFSearch *search;
	IFEvent  *event;

	if (!(search = MALLOC (sizeof (IFSearch))))
		return NULL;

	event = if_event_new (c, requested, IFEVENT_NONE,
	                      NULL,                        NULL,
	                      (IFEventCB) search_finished, NULL,
	                      "Search",                    search);

	if (!event)
	{
		free (search);
		return NULL;
	}

	search->type    = type;
	search->query   = STRDUP (query);
	search->exclude = STRDUP (exclude);
	search->realm   = STRDUP (realm);
	search->meta    = meta;

	return event;
}

void if_search_finish (IFEvent *event)
{
	Interface *cmd;
	IFSearch  *search;

	if (!event || !(search = if_event_data (event, "Search")))
		return;

	list_free (search->protocols);
	free (search->query);
	free (search->exclude);
	free (search->realm);
	free (search);

	/* send the sentinel */
	if ((cmd = interface_new ("ITEM", NULL)))
	{
		if_event_send (event, cmd);
		interface_free (cmd);
	}

	if_event_finish (event);
}

void if_search_add (IFEvent *event, Protocol *p)
{
	IFSearch *search;

	if (!event || !(search = if_event_data (event, "Search")))
		return;

	if (!list_find (search->protocols, p))
		search->protocols = list_prepend (search->protocols, p);
}

void if_search_remove (IFEvent *event, Protocol *p)
{
	IFSearch *search;

	if (!event || !(search = if_event_data (event, "Search")))
		return;

	if (!(search->protocols = list_remove (search->protocols, p)))
		if_search_finish (event);
}

static void add_meta (ds_data_t *key, ds_data_t *value, Interface *cmd)
{
	interface_put (cmd, stringf ("META/%s", (char *)key->data), value->data);
}

void append_meta_data (Interface *cmd, FileShare *file)
{
	if (!SHARE_DATA(file)->meta)
		return;

	interface_put (cmd, "META", NULL);
	share_foreach_meta (file, DS_FOREACH(add_meta), cmd);
}

void if_search_result (IFEvent *event, char *user, char *node,
                       char *href, unsigned long avail, FileShare *file)
{
	Interface *cmd;
	char      *hash;

	if (!(cmd = interface_new ("ITEM", NULL)))
		return;

	interface_put   (cmd, "user", user);
	interface_put   (cmd, "node", node);
	INTERFACE_PUTLU (cmd, "availability", avail);
	INTERFACE_PUTI  (cmd, "size", file->size);
	interface_put   (cmd, "url", href);
	interface_put   (cmd, "file", SHARE_DATA(file)->path);

	if (file->mime)
		interface_put (cmd, "mime", file->mime);

	if ((hash = share_dsp_hash (file, NULL)))
	{
		interface_put (cmd, "hash", hash);
		free (hash);
	}

	append_meta_data (cmd, file);

	if_event_send (event, cmd);
	interface_free (cmd);
}
