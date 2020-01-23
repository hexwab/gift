/*
 * if_share.c
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

#include "file.h"
#include "share_cache.h"
#include "meta.h"
#include "md5.h"

#include "if_event.h"
#include "if_search.h"
#include "if_share.h"

/*****************************************************************************/

static void share_attached (Connection *c, IFEvent *event, void *udata)
{
	Interface *cmd;

	if (!share_indexing ())
		return;

	if (!(cmd = interface_new ("SHARE", NULL)))
		return;

	interface_put (cmd, "action", "sync");
	interface_put (cmd, "status", "0");
	interface_send (cmd, c);
	interface_free (cmd);
}

IFEvent *if_share_new (Connection *c, IFEventID requested)
{
	IFEventFlags flags = IFEVENT_NONE;

	/* this is a tricky event because the same name has two types of
	 * responses that act very differently.  we will use the requested logic
	 * to figure out which it is we should be using */
	if (requested == 0)
		flags |= IFEVENT_BROADCAST | IFEVENT_NOID;

	return if_event_new (c, requested, flags,
	                     (IFEventCB) share_attached, NULL,
	                     NULL,                       NULL,
	                     NULL,                       NULL);
}

void if_share_file (IFEvent *event, FileShare *file)
{
	Interface *cmd;

	if (!event->initiate)
	{
		GIFT_WARN (("nowhere to go :("));
		return;
	}

	if (!(cmd = interface_new ("ITEM", NULL)))
		return;

	if (file)
	{
		ShareHash *sh;

		char *path;
		char *md5;

		sh = share_hash_get (file, "MD5");

		path = file_host_path (SHARE_DATA(file)->path);
		md5  = md5_string     (sh->hash);

		interface_put  (cmd, "path", path);
		INTERFACE_PUTI (cmd, "size", file->size);
		interface_put  (cmd, "mime", file->mime);
		interface_put  (cmd, "hash", md5);

		append_meta_data (cmd, file);

		free (path);
		free (md5);
	}

	if_event_send (event, cmd);
	interface_free (cmd);
}

void if_share_action (IFEvent *event, char *action, char *status)
{
	Interface *cmd;

	if (!(cmd = interface_new ("SHARE", NULL)))
		return;

	interface_put (cmd, "action", action);

	if (status)
		interface_put (cmd, "status", status);

	if_event_send (event, cmd);
	interface_free (cmd);
}

void if_share_finish (IFEvent *event)
{
	if_event_finish (event);
}
