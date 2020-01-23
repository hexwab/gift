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
#include "network.h"

#include "share_file.h"
#include "download.h"
#include "upload.h"

#include "parse.h"

#include "share_cache.h"

#include "if_search.h"
#include "if_share.h"
#include "if_stats.h"

#ifdef WIN32
#include <io.h>
#endif

/*****************************************************************************/

typedef void (*DCommand) (Connection *c, Interface *cmd);
#define DCOMMAND_FUNC(func) dcmd_##func
#define DCOMMAND(func) static void DCOMMAND_FUNC(func) (Connection *c, Interface *cmd)

DCOMMAND (attach);
DCOMMAND (detach);
#if 0
DCOMMAND (opt);
#endif
DCOMMAND (search);
DCOMMAND (browse);
DCOMMAND (locate);

DCOMMAND (transfer);
DCOMMAND (addsource);
DCOMMAND (delsource);

DCOMMAND (share);
DCOMMAND (shares);
DCOMMAND (stats);

DCOMMAND (quit);

struct dcmd
{
	char     *name;
	DCommand  cb;
}
dcmds[] =
{
	{ "attach",    DCOMMAND_FUNC(attach)    },
	{ "detach",    DCOMMAND_FUNC(detach)    },
#if 0
	{ "opt",       DCOMMAND_FUNC(opt)       },
#endif
	{ "search",    DCOMMAND_FUNC(search)    },
	{ "browse",    DCOMMAND_FUNC(browse)    },
	{ "locate",    DCOMMAND_FUNC(locate)    },

	{ "transfer",  DCOMMAND_FUNC(transfer)  },
	{ "addsource", DCOMMAND_FUNC(addsource) },
	{ "delsource", DCOMMAND_FUNC(delsource) },

	{ "share",     DCOMMAND_FUNC(share)     },
	{ "shares",    DCOMMAND_FUNC(shares)    },
	{ "stats",     DCOMMAND_FUNC(stats)     },

	{ "quit",      DCOMMAND_FUNC(quit)      },
	{ NULL,        NULL                     },
};

/*****************************************************************************/

int daemon_command_handle (Connection *c, Interface *cmd)
{
	struct dcmd *ptr;

	if (!cmd || !cmd->command)
		return FALSE;

	for (ptr = dcmds; ptr->name; ptr++)
	{
		if (!strcasecmp (ptr->name, cmd->command))
		{
			(*ptr->cb) (c, cmd);
			return TRUE;
		}
	}

	TRACE_SOCK (("unable to handle interface command %s: no handler found.",
	             cmd->command));

	return FALSE;
}

/*****************************************************************************/

/* ATTACH
 *   profile      username (giFT specific, not FastTrack/OpenFT)
 *   client       giFT-fe
 *   version      0.1.0
 */
DCOMMAND (attach)
{
	Interface *p;

	if (!(p = interface_new ("ATTACH", NULL)))
		return;

	interface_put (p, "server", PACKAGE);
	interface_put (p, "version", VERSION);
	interface_send (p, c);
	interface_free (p);

	if_event_attach (c);
}

DCOMMAND (detach)
{
	if_event_detach (c);
	connection_close (c);
}

/*****************************************************************************/
/* configuration option manipulation */

#if 0
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

DCOMMAND (opt)
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
#endif

/*****************************************************************************/

static int add_search (Dataset *d, DatasetNode *node, IFEvent *event)
{
	if_search_add (event, node->value);
	return TRUE;
}

static int send_search (Dataset *d, DatasetNode *node, IFEvent *event)
{
	Protocol *p = node->value;
	IFSearch *search;
	int       ret;

	if (!(search = if_event_data (event, "Search")))
		return FALSE;

	switch (search->type)
	{
	 case IF_SEARCH_QUERY:
		ret = p->search (p, event, search->query, search->exclude,
		                 search->realm, search->meta);
		break;
	 case IF_SEARCH_BROWSE:
		ret = p->browse (p, event, search->query, NULL);
		break;
	 case IF_SEARCH_LOCATE:
		ret = p->locate (p, event, search->query);
		break;
	}

	if (!ret)
		if_search_remove (event, p);

	return TRUE;
}

static void handle_search (Connection *c, Interface *cmd, IFSearchType type)
{
	IFEvent   *event;
	IFEventID  requested;
	char      *query;
	char      *exclude;
	char      *realm;
	char      *action;

	requested = ATOUL (cmd->value);

	/* cancel a search.  basically remove the id and hope that the protocol
	 * is compitent enough to stop replying on this [now dead] id */
	if ((action = interface_get (cmd, "action")))
	{
		if (!(event = if_connection_get_event (c->data, requested)))
			return;

		/* this will notify all protocols of the search cancellation because
		 * of this event's finished callback */
		if (!strcasecmp (action, "stop") || !strcasecmp (action, "cancel"))
			if_search_finish (event);

		return;
	}

	if ((realm = interface_get (cmd, "realm")))
	{
		if (!strcasecmp (realm, "everything"))
			realm = "";

		string_lower (realm);
	}

	query = interface_get (cmd, "query");
	exclude = interface_get (cmd, "exclude");

	/* create the search object */
	if (!(event = if_search_new (c, requested, type, query, exclude, realm, NULL)))
		return;

	protocol_foreach (DATASET_FOREACH (add_search), event);
	protocol_foreach (DATASET_FOREACH (send_search), event);
}

DCOMMAND (search) { handle_search (c, cmd, IF_SEARCH_QUERY); }
DCOMMAND (browse) { handle_search (c, cmd, IF_SEARCH_BROWSE); }
DCOMMAND (locate) { handle_search (c, cmd, IF_SEARCH_LOCATE); }

/*****************************************************************************/

DCOMMAND (transfer)
{
	TransferType type;
	Transfer    *transfer;
	IFEventID    tid;
	IFEvent     *event;
	char        *action;

	tid = ATOUL (cmd->value);

	if (!(action = interface_get (cmd, "action")))
		return;

	if (!(event = if_connection_get_event (c->data, tid)) ||
		!(transfer = if_event_data (event, "Transfer")))
		return;

	type = transfer->type;

	if (!strcasecmp (action, "cancel"))
		transfer_stop (transfer, TRUE);
	else if (!strcasecmp (action, "pause"))
		transfer_pause (transfer);
	else if (!strcasecmp (action, "unpause"))
		transfer_unpause (transfer);
	else
	{
		TRACE (("action %s unsupported", action));
		return;
	}
}

/*****************************************************************************/

static int cmp_hash (Transfer *transfer, void **cmp)
{
	char         *hash =              cmp[0];
	off_t         size = (*((off_t *) cmp[1]));
	int           ret;

	if ((ret = strcmp (transfer->hash, hash)) != 0)
		return ret;

	if (transfer->total > size)
		return 1;
	else if (transfer->total < size)
		return -1;

	/* transfer matched */
	return 0;
}

static Transfer *lookup_download (char *hash, off_t size)
{
	List *link;
	void *cmp[] = { hash, &size };

	if (!hash || size <= 0)
		return NULL;

	link = list_find_custom (download_list (), cmp, (CompareFunc) cmp_hash);

	return (link ? link->data : NULL);
}

static Transfer *start_download (Connection *c, off_t size,
                                 char *hash, char *save)
{
	if (!hash || !save || size <= 0)
		return NULL;

	TRACE (("new download (%s) (%i)", save, (int) size));

	return download_new (c, 0, NULL, save, hash, size);
}

DCOMMAND (addsource)
{
	Transfer     *transfer;
	char         *user;
	char         *hash;
	off_t         size;
	char         *url;
	char         *save;

	user = STRING_NULL (interface_get  (cmd, "user"));
	hash = STRING_NULL (interface_get  (cmd, "hash"));
	size =       (off_t)INTERFACE_GETL (cmd, "size");
	url  = STRING_NULL (interface_get  (cmd, "url"));
	save = STRING_NULL (interface_get  (cmd, "save"));

	if (!(transfer = lookup_download (hash, size)))
		transfer = start_download (c, size, hash, save);

	download_add_source (transfer, user, hash, url);
}

/*****************************************************************************/

DCOMMAND (delsource)
{
	Transfer *transfer;
	IFEvent  *event;
	IFEventID id;
	char     *url;

	if (!(url = interface_get (cmd, "url")))
	{
		TRACE (("no url specified"));
		return;
	}

	id = ATOUL (cmd->value);

	if (!(event = if_connection_get_event (c->data, id)) ||
	    !(transfer = if_event_data (event, "Transfer")))
	{
		TRACE (("no matched transfer found for %lu",
		        (unsigned long)id));
		return;
	}

	download_remove_source (transfer, url);
}

/*****************************************************************************/

static int send_hide (Dataset *d, DatasetNode *node, void *udata)
{
	Protocol *p = node->value;

	p->upload_avail (p, upload_availability ());
	p->share_hide (p);

	return TRUE;
}

static int send_show (Dataset *d, DatasetNode *node, void *udata)
{
	Protocol *p = node->value;

	p->upload_avail (p, upload_availability ());
	p->share_show (p);

	return TRUE;
}

DCOMMAND (share)
{
	IFEvent  *event;
	char     *action;

	TRACE_FUNC ();

	if (!(event = if_share_new (c, 0)))
		return;

	if (!(action = interface_get (cmd, "action")))
		return;

	if (!strcasecmp (action, "sync"))
		share_update_index ();
	else if (!strcasecmp (action, "hide"))
	{
		upload_disable ();
		protocol_foreach (DATASET_FOREACH (send_hide), NULL);
	}
	else if (!strcasecmp (action, "show"))
	{
		upload_enable ();
		protocol_foreach (DATASET_FOREACH (send_show), NULL);
	}
	else if (!strcasecmp (action, ""))
	{
		/* dump the current action */
		if (upload_status ())
			action = "show";
		else
			action = "hide";
	}

	if (!strcasecmp (action, "hide") || !strcasecmp (action, "show"))
		if_share_action (event, action, NULL);

	if_share_finish (event);
}

/*****************************************************************************/

static int show_shares (Dataset *d, DatasetNode *node, IFEvent *event)
{
	FileShare *share = node->value;
	if_share_file (event, share);
	return TRUE;
}

DCOMMAND (shares)
{
	IFEvent  *event;
	IFEventID requested;

	requested = ATOUL (cmd->value);

	/* create the event reply object */
	if (!(event = if_share_new (c, requested)))
		return;

	share_foreach (DATASET_FOREACH (show_shares), event);
	if_share_file (event, NULL);

	if_share_finish (event);
}

/*****************************************************************************/

static void local_stats (IFEvent *event)
{
	unsigned long files = 0;
	double        size  = 0.0;

	/* if we arent eligible for upload, we should send 0 here */
	if (upload_status () != 0)
		share_index (&files, &size);

	if_stats_reply (event, "giFT", 0, files, size / 1024.0);
}

static int remote_stats (Dataset *d, DatasetNode *node, IFEvent *event)
{
	Protocol     *p = node->value;
	unsigned long users = 0;
	unsigned long files = 0;
	double        size  = 0.0;
	Dataset      *extra = NULL;

	/* gather stats and return them to the interface protocol */
	p->stats (p, &users, &files, &size, &extra);
	if_stats_reply (event, p->name, users, files, size);
	dataset_clear (extra);

	return TRUE;
}

DCOMMAND (stats)
{
	IFEvent *event;

	if (!(event = if_stats_new (c, 0)))
		return;

	local_stats (event);
	protocol_foreach (DATASET_FOREACH (remote_stats), event);

	if_stats_finish (event);
}

/*****************************************************************************/

DCOMMAND (quit)
{
	gift_exit ();
}
