/*
 * daemon.c
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

#include "event.h"
#include "daemon.h"
#include "download.h"
#include "parse.h"

#include "watch.h"

#include "sharing.h"

/*****************************************************************************/

/**/extern List *protocols;

/*****************************************************************************/

typedef void (*EventCallback) (Protocol *p, Connection *c, Dataset *event);
#define EV_HANDLER(func) static void dc_callback_##func (Protocol *p, Connection *c, Dataset *event)

EV_HANDLER (attach);
EV_HANDLER (search);
EV_HANDLER (transfer);
EV_HANDLER (share);
EV_HANDLER (stats);

struct _dc_event
{
	char          *name;
	EventCallback  cb;
}
dc_events[] =
{
	{ "attach",   dc_callback_attach   },
	{ "search",   dc_callback_search   },
	{ "transfer", dc_callback_transfer },
	{ "share",    dc_callback_share    },
	{ "stats",    dc_callback_stats    },
	{ NULL,       NULL                 },
};

/*****************************************************************************/

void dc_handle_event (Protocol *p, Connection *c, Dataset *event)
{
	struct _dc_event *ptr;
	List *list_ptr;
	char *ev_name;

	ev_name = dataset_lookup (event, "head");

	for (ptr = dc_events; ptr->name; ptr++)
	{
		if (!strcasecmp (ptr->name, ev_name))
		{
			(*ptr->cb) (p, c, event);
			return;
		}
	}

	for (list_ptr = protocols; list_ptr; list_ptr = list_next (list_ptr))
	{
		p = list_ptr->data;

		if (p->callback)
			p->callback (p, c, event);
	}
}

/*****************************************************************************/

#if 0
EV_HANDLER (connect)
{
	Protocol *protocol;
	char     *proto;

	if (!(proto = dataset_lookup(event, "protocol")))
	{
		interface_send_err (c, "Bad arguments to connect: no protocol");
		return;
	}

	/* locate the protocol */
	if (!(protocol = protocol_find (proto)))
	{
		interface_send_err (c, "Bad arguments to connect: "
		                    "unknown protocol");
		return;
	}

	/* assign the data */
	if (c->protocol)
	{
		GIFT_WARN (("re-connecting %p as %s", c, proto));
	}

	c->protocol = protocol;
}

/*****************************************************************************/

EV_HANDLER (watch)
{
	TRACE_FUNC ();

	watch_activate (c);
}
#endif

/*****************************************************************************/
/* ATTACH
 *   profile      username (giFT specific, not FastTrack/OpenFT)
 *   client       giFT-fe
 *   version      0.1.0
 */

EV_HANDLER (attach)
{
	char *profile;
	char *client;
	char *version;

	TRACE_FUNC ();

	profile = dataset_lookup (event, "profile");
	client  = dataset_lookup (event, "client");
	version = dataset_lookup (event, "version");

	/* append this connection to all events broadcast list */
	if_event_attach (c);
}

/*****************************************************************************/

static void search_finish (IFEvent *event)
{
	TRACE_FUNC ();

	/* if attached, return the termination, otherwise the logic that called
	 * this function will close the endpoint */
	if (event->c && event->c->data)
		if_event_reply (event->id, "search", "id=i", event->id, NULL);
}

/* SEARCH
 *   protocol     optional single-protocol search
 *   query        search string
 *   exclude      exclusion list
 *   realm        audio[/mpeg]
 *   csize        size constraint ("min-max")
 *   ckbps        bits per second constraint ("min-max")
 */

EV_HANDLER (search)
{
	Protocol  *proto;
	IFEventID  id;
	List *ptr;
	char *protocol;
	char *query;
	char *exclude;
	char *realm;
	char *csize;
	char *ckbps;

	TRACE_FUNC ();

	protocol = dataset_lookup (event, "protocol");
	query    = dataset_lookup (event, "query");
	exclude  = dataset_lookup (event, "exclude");
	realm    = dataset_lookup (event, "realm");
	csize    = dataset_lookup (event, "csize");
	ckbps    = dataset_lookup (event, "ckbps");

	/* before we pass to the protocol, insert the local event id */
	id = if_event_new (c, IFEVENT_SEARCH, (IFEventFunc) search_finish, NULL);
	dataset_insert (event, "id", (void *) id);

	/* format realm */
	if (realm)
	{
		if (!strcasecmp (realm, "everything"))
			realm = NULL;

		string_lower (realm);
	}

	/* proto was supplied, use it */
	if ((proto = protocol_find (protocol)))
	{
		if (proto->callback)
			proto->callback (proto, c, event);

		return;
	}

	/* otherwise, we need to loop all protocols and deliver this request */
	for (ptr = protocols; ptr; ptr = list_next (ptr))
	{
		proto = ptr->data;

		if (proto->callback)
			proto->callback (proto, c, event);

	}

	/* dataset_clear_free will be called after this function exits, "id"'s
	 * value is not allocated... */
	dataset_remove (event, "id");
}

/*****************************************************************************/

static void start_download (Connection *c, Dataset *event)
{
	Transfer *transfer;
	unsigned long size;
	char *save;

	/* gather arguments */
	size   = ATOI (dataset_lookup (event, "size"));
	save   =       dataset_lookup (event, "save");

	if (!save || !size)
	{
		TRACE (("invalid transfer tag"));
		return;
	}

	TRACE (("new download (%s) (%lu)", save, size));

	transfer = download_new (c, save, size);
}

EV_HANDLER (transfer)
{
	IFEventID id;
	Transfer *transfer;
	char *action;
	char *user;
	char *hash;
	char *addsource;
	char *delsource;

	TRACE_FUNC ();

	if (!c->data)
	{
		GIFT_WARN (("transfer must be attached!"));
		return;
	}

	id     = ATOI (dataset_lookup (event, "id"));
	action =       dataset_lookup (event, "action");

	transfer = if_event_data (id);

	if (action)
	{
		if (!strcasecmp (action, "download"))
		{
			start_download (c, event);
			return;
		}
		else if (id && !strcasecmp (action, "cancel"))
		{
			transfer_stop (transfer, TRUE);
			return;
		}
		else if (id && !strcasecmp (action, "pause"))
		{
			transfer_stop (transfer, FALSE);
			return;
		}
	}

	user      =       dataset_lookup (event, "user");
	hash      =       dataset_lookup (event, "hash");
	addsource =       dataset_lookup (event, "addsource");
	delsource =       dataset_lookup (event, "delsource");

	if (!transfer)
		return;

	if (addsource)
		download_add_source (transfer, source_new (user, hash, addsource));
	else if (delsource)
		download_remove_source (transfer, delsource);
}

/*****************************************************************************/

EV_HANDLER (share)
{
	Protocol *proto;
	List     *ptr;

	TRACE_FUNC ();

	share_update_index ();

	/* push this to all protocols as well, they will need to update */
	for (ptr = protocols; ptr; ptr = list_next (ptr))
	{
		proto = ptr->data;

		if (proto->callback)
			proto->callback (proto, c, event);
	}

	/* if we aren't attached, close after this request is evaluated */
	if (!c->data)
		interface_close (c);
}

/*****************************************************************************/

static void stats_finish (IFEvent *event)
{
	/* if attached, return the termination, otherwise the logic that called
	 * this function will close the endpoint */
	if (event->c && event->c->data)
		if_event_reply (event->id, "stats", "id=i", event->id, NULL);
}

EV_HANDLER (stats)
{
	Protocol *proto;
	List     *ptr;
	char     *protocol;
	IFEventID id;
	unsigned long files;
	double        size;

	protocol = dataset_lookup (event, "protocol");

	/* before we pass to the protocol, insert the local event id */
	id = if_event_new (c, IFEVENT_STATS, (IFEventFunc) stats_finish, NULL);
	dataset_insert (event, "id", (void *) id);

	/* send the local share information */
	share_index (&files, &size);
	interface_send (c, "stats",
					"id=i",       id,
					"protocol=s", "local",
					"files=i",    files,
					"size=i",     (unsigned long) size,
					NULL);

	/* proto was supplied, use it */
	if ((proto = protocol_find (protocol)))
	{
		if (proto->callback)
			proto->callback (proto, c, event);

		return;
	}

	/* otherwise, we need to loop all protocols and deliver this request */
	for (ptr = protocols; ptr; ptr = list_next (ptr))
	{
		proto = ptr->data;

		if (proto->callback)
			proto->callback (proto, c, event);
	}

	/* dataset_clear_free will be called after this function exits, "id"'s
	 * value is not allocated... */
	dataset_remove (event, "id");

	if_event_remove (id);
}
