/*
 * $Id: daemon.c,v 1.85 2003/06/06 04:06:35 jasta Exp $
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

#include "lib/event.h"
#include "lib/network.h"
#include "lib/parse.h"

#include "if_event.h"

#include "daemon.h"

#include "download.h"
#include "upload.h"

#include "share_cache.h"
#include "share_hash.h"

#include "if_search.h"
#include "if_share.h"
#include "if_stats.h"

#ifdef WIN32
#include <io.h>
#endif

/*****************************************************************************/

typedef void (*DCommand) (TCPC *c, Interface *cmd);
#define DCOMMAND_FUNC(func) dcmd_##func
#define DCOMMAND(func) static void DCOMMAND_FUNC(func) (TCPC *c, Interface *cmd)

DCOMMAND (attach);
DCOMMAND (detach);
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

static struct dcmd
{
	char     *name;
	DCommand  cb;
}
dcmds[] =
{
	{ "attach",    DCOMMAND_FUNC(attach)    },
	{ "detach",    DCOMMAND_FUNC(detach)    },
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

static Dataset *func_tbl = NULL;

static void build_ftbl (struct dcmd *ptr)
{
	assert (func_tbl == NULL);

	func_tbl = dataset_new (DATASET_HASH);
	assert (func_tbl != NULL);

	for (; ptr->name; ptr++)
	{
		dataset_insert (&func_tbl, ptr->name, STRLEN_0 (ptr->name),
		                ptr->cb, 0);
	}
}

static DCommand lookup_ftbl (char *name)
{
	string_lower (name);
	return dataset_lookup (func_tbl, name, STRLEN_0 (name));
}

/*****************************************************************************/

int daemon_command_handle (TCPC *c, Interface *cmd)
{
	DCommand cmdfn;

	if (!cmd || !cmd->command)
		return FALSE;

	if (!func_tbl)
		build_ftbl (dcmds);

	if ((cmdfn = lookup_ftbl (cmd->command)))
	{
		(*cmdfn) (c, cmd);
		return TRUE;
	}

	GIFT_TRACE (("%s: no handler for interface command %s",
	             net_ip_str (c->host), cmd->command));

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
	tcp_close (c);
}

/*****************************************************************************/

static void add_search (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	if_search_add (event, value->data);
}

static void send_search (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	Protocol *p = value->data;
	IFSearch *search;
	int       ret;

	search = if_event_data (event, "Search");
	assert (search != NULL);

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
		{
			char *algo = hashstr_algo (search->query);
			char *data = hashstr_data (search->query);

			ret = p->locate (p, event, algo, data);
		}
		break;
	 default:
		abort ();
		break;
	}

	if (!ret)
		if_search_remove (event, p);
}

static void handle_search (TCPC *c, Interface *cmd, IFSearchType type)
{
	IFEvent    *event;
	if_event_id requested;
	char       *query;
	char       *exclude;
	char       *realm;
	char       *action;

	requested = ATOUL (cmd->value);

	/* cancel a search.  basically remove the id and hope that the protocol
	 * is compitent enough to stop replying on this [now dead] id */
	if ((action = interface_get (cmd, "action")))
	{
		if (!(event = if_connection_get_event (c->udata, requested)))
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

	protocol_foreach (DS_FOREACH(add_search), event);
	protocol_foreach (DS_FOREACH(send_search), event);
}

DCOMMAND (search) { handle_search (c, cmd, IF_SEARCH_QUERY); }
DCOMMAND (browse) { handle_search (c, cmd, IF_SEARCH_BROWSE); }
DCOMMAND (locate) { handle_search (c, cmd, IF_SEARCH_LOCATE); }

/*****************************************************************************/

DCOMMAND (transfer)
{
	Transfer    *transfer;
	if_event_id  tid;
	IFEvent     *event;
	char        *action;

	tid = ATOUL (cmd->value);

	if (!(action = interface_get (cmd, "action")))
		return;

	if (!(event = if_connection_get_event (c->udata, tid)) ||
		!(transfer = if_event_data (event, "Transfer")))
		return;

	if (!strcasecmp (action, "cancel"))
		transfer_stop (transfer, TRUE);
	else if (!strcasecmp (action, "pause"))
		transfer_pause (transfer);
	else if (!strcasecmp (action, "unpause"))
		transfer_unpause (transfer);
	else
	{
		GIFT_TRACE (("action %s unsupported", action));
		return;
	}
}

/*****************************************************************************/

static int find_xfer (Transfer *transfer, Array **args)
{
	Source *source;
	off_t  *size;
	List   *link;
	int     ret;

	list (args, &source, &size, NULL);

	if ((ret = INTCMP (transfer->total, *size)))
		return ret;

	/* compare the hashes if the protocol installed them */
	if (source->hash && transfer->hash)
		return strcmp (source->hash, transfer->hash);

	/* otherwise look for an identical source */
	link = list_find_custom (transfer->sources, source,
	                         (CompareFunc)source_cmp);

	if (!link)
		return 1;

	/* transfer matched */
	return 0;
}

static Transfer *lookup_download (char *user, char *hash, char *url,
                                  off_t size)
{
	List   *link;
	Source *source;
	Array  *args = NULL;

	if (!url || size <= 0)
		return NULL;

	if (!(source = source_new (user, hash, url)))
		return NULL;

	push (&args, source);
	push (&args, &size);

	link = list_find_custom (download_list (), &args, (CompareFunc)find_xfer);

	source_free (source);
	unset (&args);

	return list_nth_data (link, 0);
}

static Transfer *start_download (TCPC *c, off_t size,
                                 char *hash, char *save)
{
	if (!save || size <= 0)
		return NULL;

	GIFT_TRACE (("new download (%s) (%i)", save, (int) size));
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

	if (!(transfer = lookup_download (user, hash, url, size)))
		transfer = start_download (c, size, hash, save);

	download_add_source (transfer, user, hash, url);
}

/*****************************************************************************/

DCOMMAND (delsource)
{
	Transfer   *transfer;
	IFEvent    *event;
	if_event_id id;
	char       *url;

	if (!(url = interface_get (cmd, "url")))
	{
		GIFT_TRACE (("no url specified"));
		return;
	}

	id = ATOUL (cmd->value);

	if (!(event = if_connection_get_event (c->udata, id)) ||
	    !(transfer = if_event_data (event, "Transfer")))
	{
		GIFT_TRACE (("no matched transfer found for %lu",
		             (unsigned long)id));
		return;
	}

	download_remove_source (transfer, url);
}

/*****************************************************************************/

static void send_hide (ds_data_t *key, ds_data_t *value, void *udata)
{
	Protocol *p = value->data;

	p->upload_avail (p, upload_availability ());
	p->share_hide (p);
}

static void send_show (ds_data_t *key, ds_data_t *value, void *udata)
{
	Protocol *p = value->data;

	p->upload_avail (p, upload_availability ());
	p->share_show (p);
}

DCOMMAND (share)
{
	IFEvent  *event;
	char     *action;

	GIFT_TRACEFN;

	if (!(event = if_share_new (c, 0)))
		return;

	if (!(action = interface_get (cmd, "action")))
		return;

	if (!strcasecmp (action, "sync"))
		share_update_index ();
	else if (!strcasecmp (action, "hide"))
	{
		upload_disable ();
		protocol_foreach (DS_FOREACH(send_hide), NULL);
	}
	else if (!strcasecmp (action, "show"))
	{
		upload_enable ();
		protocol_foreach (DS_FOREACH(send_show), NULL);
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

static int show_shares (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	FileShare *share = value->data;

	if_share_file (event, share);

	return DS_CONTINUE;
}

DCOMMAND (shares)
{
	IFEvent    *event;
	if_event_id requested;

	requested = ATOUL (cmd->value);

	/* create the event reply object */
	if (!(event = if_share_new (c, requested)))
		return;

	share_foreach (DS_FOREACH_EX(show_shares), event);
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

static void remote_stats (ds_data_t *key, ds_data_t *value, IFEvent *event)
{
	Protocol     *p = value->data;
	unsigned long users = 0;
	unsigned long files = 0;
	double        size  = 0.0;
	Dataset      *extra = NULL;

	/* gather stats and return them to the interface protocol */
	p->stats (p, &users, &files, &size, &extra);
	if_stats_reply (event, p->name, users, files, size);
	dataset_clear (extra);
}

DCOMMAND (stats)
{
	IFEvent *event;

	if (!(event = if_stats_new (c, 0)))
		return;

	local_stats (event);
	protocol_foreach (DS_FOREACH(remote_stats), event);

	if_stats_finish (event);
}

/*****************************************************************************/

DCOMMAND (quit)
{
	event_quit (0);
}
