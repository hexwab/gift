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
#include "main.h"

#include "event.h"
#include "daemon.h"

#include "download.h"
#include "upload.h"

#include "parse.h"

#include "share_cache.h"

#ifdef WIN32
#include <io.h>
#endif

/*****************************************************************************/

typedef void (*EventCallback) (Protocol *p, Connection *c, Dataset *event);
#define EV_HANDLER(func) static void dc_callback_##func (Protocol *p, Connection *c, Dataset *event)

EV_HANDLER (attach);
EV_HANDLER (opt);
EV_HANDLER (search);
EV_HANDLER (transfer);
EV_HANDLER (share);
EV_HANDLER (stats);
EV_HANDLER (quit);

struct _dc_event
{
	char          *name;
	EventCallback  cb;
}
dc_events[] =
{
	{ "attach",   dc_callback_attach   },
	{ "opt",      dc_callback_opt      },
	{ "search",   dc_callback_search   },
	{ "transfer", dc_callback_transfer },
	{ "share",    dc_callback_share    },
	{ "stats",    dc_callback_stats    },
	{ "quit",     dc_callback_quit     },
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

	for (list_ptr = protocol_list (); list_ptr;
	     list_ptr = list_next (list_ptr))
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

	/* report transfers */
	download_report_attached (c);
	upload_report_attached (c);
}

/*****************************************************************************/
/* configuration option manipulation */

static void opt_show_modules (IFEventID id)
{
	List *modules = NULL;
	List *ptr;

	modules = list_append (modules, "giFT");
	modules = list_append (modules, "ui");

	/* add all protocol modules */
	for (ptr = protocol_list (); ptr; ptr = list_next (ptr))
	{
		Protocol *p = ptr->data;

		modules = list_append (modules, p->name);
	}

	/* actually display them now */
	for (ptr = modules; ptr; ptr = list_next (ptr))
	{
		if_event_reply (id, "opt",
		                "id=i",   id,
		                "conf=s", ptr->data,
		                NULL);
	}
}

static int show_keys (char *key, char *value, Dataset *dataset)
{
	IFEventID     id;
	char         *module;
	ConfigHeader *header;
	char         *full_key;

	if (!key || !value || !dataset)
		return TRUE;

	id     = (IFEventID) P_INT (dataset_lookup (dataset, "id"));
	module = dataset_lookup (dataset, "module");
	header = dataset_lookup (dataset, "header");

	full_key = malloc (strlen (header->name) + strlen (key) + 2);

	sprintf (full_key, "%s/%s", header->name, key);

	if_event_reply (id, "opt",
					"id=i",    id,
	                "conf=s",  module,
	                "key=s",   full_key,
	                "value=s", value,
	                NULL);

	free (full_key);

	return TRUE;
}

static int show_headers (ConfigHeader *header, Dataset *dataset)
{
	dataset_insert (dataset, "header", header);

	hash_table_foreach (header->keys, (HashFunc) show_keys, dataset);

	return TRUE;
}

static void opt_show_module (IFEventID id, char *module)
{
	Dataset *dataset = NULL;
	Config  *conf;

	if (!(conf = gift_config_new (module)))
	{
		TRACE (("unable to load conf module '%s'", module));
		return;
	}

	dataset_insert (dataset, "id",     I_PTR (id));
	dataset_insert (dataset, "module", module);
	dataset_insert (dataset, "header", NULL);

	list_foreach (conf->headers, (ListForeachFunc) show_headers, dataset);

	dataset_clear (dataset);
}

static void opt_finish (IFEvent *event)
{
	if (event->c && event->c->data)
		if_event_reply (event->id, "opt", "id=i", event->id, NULL);
}

EV_HANDLER (opt)
{
	IFEventID id;
	char *conf;
	char *key;
	char *value;

	conf  = dataset_lookup (event, "conf");
	key   = dataset_lookup (event, "key");
	value = dataset_lookup (event, "value");

	id = if_event_new (c, IFEVENT_OPT, (IFEventFunc) opt_finish, NULL);

	if (!conf)       /* list all configuration "modules" */
		opt_show_modules (id);
	else if (!key)   /* show all keys/values for a given module */
		opt_show_module (id, conf);
	else if (!value) /* show the value for a given key */
		/* TODO */;
	else
	{
		/* set the value for a given key */
		/* TODO */
	}

	if_event_remove (id);
}

/*****************************************************************************/

static void search_finish (IFEvent *event)
{
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
 *   type         user or hash
 *   csize        size constraint ("min-max")
 *   ckbps        bits per second constraint ("min-max")
 */

EV_HANDLER (search)
{
	Protocol  *proto;
	IFEventID  id;
	List *ptr;
	char *protocol;
	char *realm;

	protocol = dataset_lookup (event, "protocol");
	realm    = dataset_lookup (event, "realm");

	/* before we pass to the protocol, insert the local event id */
	id = if_event_new (c, IFEVENT_SEARCH, (IFEventFunc) search_finish, NULL);
	dataset_insert (event, "id", I_PTR (id));

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
	for (ptr = protocol_list (); ptr; ptr = list_next (ptr))
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
	char *hash;
	char *save;

	/* gather arguments */
	size   = ATOI (dataset_lookup (event, "size"));
	hash   =       dataset_lookup (event, "hash");
	save   =       dataset_lookup (event, "save");

	if (!save || !hash || !size)
	{
		TRACE (("invalid transfer tag"));
		return;
	}

	TRACE (("new download (%s) (%lu)", save, size));

	transfer = download_new (c, NULL, save, hash, size);
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
		else if (id)
		{
			if (!strcasecmp (action, "cancel"))
			{
				transfer_stop (transfer, TRUE);
				return;
			}
			else if (!strcasecmp (action, "pause"))
			{
				transfer_pause (transfer);
				return;
			}
			else if (!strcasecmp (action, "unpause"))
			{
				transfer_unpause (transfer);
				return;
			}
		}
	}

	if (!transfer)
		return;

	user      =       dataset_lookup (event, "user");
	hash      =       dataset_lookup (event, "hash");
	addsource =       dataset_lookup (event, "addsource");
	delsource =       dataset_lookup (event, "delsource");

	if (addsource)
		download_add_source (transfer, user, hash, addsource);
	else if (delsource)
		download_remove_source (transfer, delsource);
}

/*****************************************************************************/

EV_HANDLER (share)
{
	Protocol *proto;
	List     *ptr;
	char     *action;

	TRACE_FUNC ();

	action = dataset_lookup (event, "action");

	if (!action || !strcmp (action, "sync"))
		share_update_index ();
	else if (!strcmp (action, "hide"))
		upload_disable ();
	else if (!strcmp (action, "show"))
		upload_enable ();

	/* push this to all protocols as well, they will need to update */
	for (ptr = protocol_list (); ptr; ptr = list_next (ptr))
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
	unsigned long files = 0;
	double        size  = 0.0;

	protocol = dataset_lookup (event, "protocol");

	/* before we pass to the protocol, insert the local event id */
	id = if_event_new (c, IFEVENT_STATS, (IFEventFunc) stats_finish, NULL);
	dataset_insert (event, "id", I_PTR (id));

	/* if max_uploads are at 0, local shares should be set at 0 as well */
	if (upload_status () != 0)
		share_index (&files, &size);

	interface_send (c, "stats",
	                "id=i",       id,
	                "protocol=s", "local",
	                "files=i",    files,
	                "size=i",     (unsigned long) (size / 1024.0),
	                NULL);

	/* proto was supplied, use it */
	if ((proto = protocol_find (protocol)))
	{
		if (proto->callback)
			proto->callback (proto, c, event);

		return;
	}

	/* otherwise, we need to loop all protocols and deliver this request */
	for (ptr = protocol_list (); ptr; ptr = list_next (ptr))
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

/*****************************************************************************/

/* if you really want giFT to exit normally */
EV_HANDLER (quit)
{
	gift_exit ();
}
