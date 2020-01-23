/*
 * if_search.c
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

#include "gift.h"

#include "share_file.h"
#include "meta.h"
#include "md5.h"

#include "if_event.h"
#include "if_search.h"

/*****************************************************************************/

static int send_cancel (Dataset *d, DatasetNode *node, IFEvent *event)
{
	Protocol *p = node->value;

	p->search_cancel (p, event);

	return TRUE;
}

static void search_finished (Connection *c, IFEvent *event, void *udata)
{
	TRACE_FUNC ();
	protocol_foreach (DATASET_FOREACH (send_cancel), event);
}

IFEvent *if_search_new (Connection *c, IFEventID requested, IFSearchType type,
                        char *query, char *exclude, char *realm, Dataset *meta)
{
	IFSearch *search;
	IFEvent  *event;

	if (!(search = malloc (sizeof (IFSearch))))
		return NULL;

	memset (search, 0, sizeof (IFSearch));

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

static int add_meta (Dataset *d, DatasetNode *node, Interface *cmd)
{
	interface_put (cmd, stringf ("META/%s", node->key), node->value);
	return FALSE;
}

void append_meta_data (Interface *cmd, FileShare *file)
{
	if (!SHARE_DATA(file)->meta)
		return;

	interface_put (cmd, "META", NULL);
	meta_foreach (file, DATASET_FOREACH (add_meta), cmd);
}

void if_search_result (IFEvent *event, char *user, char *node,
                       char *href, unsigned long avail, FileShare *file)
{
	Interface *cmd;
	ShareHash *sh;
	char      *hash;

	if (!(sh = share_hash_get (file, "MD5")))
		return;

	if (!(hash = md5_string (sh->hash)))
		return;

	if (!(cmd = interface_new ("ITEM", NULL)))
	{
		free (hash);
		return;
	}

	interface_put   (cmd, "user", user);
	interface_put   (cmd, "node", node);
	INTERFACE_PUTLU (cmd, "availability", avail);
	INTERFACE_PUTI  (cmd, "size", file->size);
	interface_put   (cmd, "hash", hash);
	interface_put   (cmd, "url", href);
	interface_put   (cmd, "file", SHARE_DATA(file)->path);

	free (hash);

	if (file->mime)
		interface_put (cmd, "mime", file->mime);

	append_meta_data (cmd, file);

	if_event_send (event, cmd);
	interface_free (cmd);
}
