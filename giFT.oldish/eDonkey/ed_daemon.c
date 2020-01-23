/*
 * ft_daemon.c
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

#include "ed_main.h"

#include "reply.h"
#include "share_file.h"

/*****************************************************************************/
#if 0
extern Connection   *ft_self;
extern unsigned long search_unique_id;
#endif

/*****************************************************************************/

typedef void (*EventCallback) (Protocol *p, Connection *c, Tree *cmd, IFEvent *event);
#define EV_HANDLER(func) static void ed_##func (Protocol *p, Connection *c, Tree *cmd, IFEvent *event)

EV_HANDLER (daemon_search);
EV_HANDLER (daemon_browse);
EV_HANDLER (daemon_locate);
EV_HANDLER (daemon_share);
EV_HANDLER (daemon_stats);

/*****************************************************************************/

struct _cmd
{
	char *name;
	EventCallback cb;
}
edonkey_cmds[] =
{
	{ "search",     ed_daemon_search  },
	{ "browse",     ed_daemon_browse  },
	{ "locate",     ed_daemon_locate  },
	{ "share",      ed_daemon_share   },
	{ "stats",      ed_daemon_stats   },
	{ NULL,         NULL              },
};

/*****************************************************************************/

void ed_daemon_callback (Protocol *p, Connection *c, Tree *cmd,
                         IFEvent *event)
{
	struct _cmd   *ptr;
	char                 *command;

	if (!(command = interface_lookup (cmd, NULL)))
		return;

	for (ptr = edonkey_cmds; ptr->name; ptr++)
	{
		if (strcasecmp (ptr->name, command))
			continue;

		/* found the appropriate event */
		(*ptr->cb) (p, c, cmd, event);

		return;
	}

	GIFT_ERROR (("no handler found for %s", command));
}

/*****************************************************************************/
#if 0
static Connection *send_search (Connection *c, Node *node, FTSearch *search)
{
	int type;

	assert (search != NULL);
	assert (search->event != NULL);

	type = search->type;

#ifdef FT_SEARCH_PARANOID
	if (type & FT_SEARCH_FILENAME)
	{
		type |= FT_SEARCH_HIDDEN;

		ft_packet_sendva (c, FT_SEARCH_REQUEST, 0, "lhLLs",
		                  search->event->id, type,
						  search->qtokens, search->etokens, search->realm);
	}
	else
#endif /* FT_SEARCH_PARANOID */
		ft_packet_sendva (c, FT_SEARCH_REQUEST, 0, "lhsss",
		                  search->event->id, type,
						  search->query, search->exclude, search->realm);

	ft_search_need_reply (search, c);
	return NULL;
}

static int result_reply (FileShare *file, FTSearch *search)
{
	FT_Share *share;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return TRUE;

	assert (share->shost != NULL);

	if (share->shost->host)
		ft_search_reply (search, ft_self, file);

	/* this was done to keep track of all search results when used with
	 * a different api...oh well, senselessly wasting resources here */
	share->shost->files = list_remove (share->shost->files, file);
	share_unref (file);
	return TRUE;
}

static void search_children (FTSearch *search)
{
	List *r;

	/* fill r with search results */
	r = ft_search (NULL, search->type, search->realm,
	               search->query, search->exclude);

	/* reply and remove */
	r = list_foreach_remove (r, (ListForeachFunc) result_reply, search);
	list_free (r);
}

static void search_parents (FTSearch *search)
{
	conn_foreach ((ConnForeachFunc) send_search, search,
	              NODE_SEARCH, NODE_CONNECTED, 0);

	TRACE (("bounced to %i node(s)", dataset_length (search->ref)));
}

static void exec_search (IFEvent *event, SearchType type, char *q, char *e,
                         char *realm)
{
	FTSearch *search;

	if (!(search = ft_search_new (event, type, q, e, realm)))
		return;

	/* search all of our children's shares as if a remote node executed
	 * the search */
	if (NODE(ft_self)->class & NODE_SEARCH)
		search_children (search);

	assert (search->ref == NULL);

	/* this searches all search nodes you are connected to ... obviously
	 * this is not a blocking operation...the rest of this code moves over
	 * to ft_protocol.c:ft_search_response */
	search_parents (search);

	/* if search_parents found no nodes to send to, this will actually
	 * unregister the search...otherwise it should have no effect */
	ft_search_reply (search, ft_self, NULL);
}

static void handle_search (Protocol *p, Connection *c, Tree *cmd,
                           IFEvent *event, char *command, SearchType type)
{
	char *action;
	char *query;
	char *exclude;
	char *realm;

	if ((action = interface_lookup (cmd, stringf ("%s/action", command))))
	{
		if (!strcasecmp (action, "stop") || !strcasecmp (action, "cancel"))
			ft_event_disable (event);

		return;
	}

	if (!(query = interface_lookup (cmd, stringf ("%s/query", command))))
	{
		if_search_remove (event, openft_proto);
		return;
	}

	exclude = interface_lookup (cmd, stringf ("%s/exclude", command));
	realm   = interface_lookup (cmd, stringf ("%s/realm", command));

	/* all the necessary data has been gathered from the interface protocol,
	 * so perform the search */
	exec_search (event, type, query, exclude, realm);

}
#endif /*  0 */

EV_HANDLER (daemon_search)
{
	char *action, *query;

	TRACE_FUNC();

	if (action = interface_lookup (cmd, "SEARCH/action"))
	{
		if (!strcasecmp (action, "stop") || !strcasecmp (action, "cancel"))
			/* kill search */
			return;
	}

	if (!(query = interface_lookup (cmd, "SEARCH/query")))
	{
		if_search_remove (event, edonkey_proto);
		return;
	}


	{
		/* return a fake result */
		char *user, *node, *url;
		unsigned long avail;
		FileShare file;
		
		
		user = "foo";
		node = "bar";
		url = "skel://foo/bar";
		
#if 0		
		file.root = "/";
		file.path = "/path/filename.ext";
		file.hpath = NULL;
		file.meta = NULL;
#endif

		file.mime = "text/plain";
		file.size = 123456;
		file.p = edonkey_proto;
		file.data = NULL;

		avail = 1;

		if_search_result (event, user, node, url,
                          avail, &file);
	}

	if_search_remove (event, edonkey_proto);
}

EV_HANDLER (daemon_locate)
{
	TRACE_FUNC();
//	handle_search (p, c, cmd, event, "LOCATE", FT_SEARCH_MD5);
}

/*****************************************************************************/

EV_HANDLER (daemon_browse)
{
	TRACE_FUNC();
#if 0
	Connection *user_c;
	char       *user, *ptr;
	char       *node;

	user = interface_lookup (cmd, "BROWSE/query");
	node = interface_lookup (cmd, "BROWSE/node");

	if (!user)
	{
		if_search_remove (event, openft_proto);
		return;
	}

	if ((ptr = strchr (user, '@')))
		user = ptr + 1;

	if (!(user_c = conn_lookup (net_ip (user))))
	{
		TRACE (("TODO: not connected to %s", user));
		return;
	}

	ft_packet_sendva (user_c, FT_BROWSE_REQUEST, 0, NULL);
#endif
}

/*****************************************************************************/
#if 0
static Connection *submit_avail (Connection *c, Node *node, void *udata)
{
	ft_packet_sendva (c, FT_MODSHARE_REQUEST, 0, "l", upload_availability ());
	return NULL;
}
#endif

EV_HANDLER (daemon_share)
{
	TRACE_FUNC();
#if 0
	char *action;

	if (!(action = interface_lookup (cmd, "SHARE/action")))
		return;

	if (!strcasecmp (action, "sync"))
	{
		/* everything we would've wanted to do has already been taken care
		 * of by giFT */
	}
	else
	{
		conn_foreach ((ConnForeachFunc) submit_avail, NULL,
		              NODE_PARENT, NODE_CONNECTED, 0);
	}
#endif
}

/*****************************************************************************/
#if 0
struct _stats
{
	unsigned long users;
	unsigned long files;
	double        size; /* in gigs */
	int           connections;
};

static Connection *gather_stats (Connection *c, Node *node,
                                 struct _stats *stats)
{
	/* favor the most positive (longest lived stats) */
	if (node->stats.users > stats->users)
		stats->users = node->stats.users;

	if (node->stats.shares > stats->files)
		stats->files = node->stats.shares;

	if (node->stats.size > stats->size)
		stats->size = node->stats.size;

	stats->connections++;

	return NULL;
}
#endif

EV_HANDLER (daemon_stats)
{
	unsigned long users, files;
	double size;

	TRACE_FUNC();

	users = 247;
	files = 31337;
	size = 123.456;

	if_stats_reply (event, edonkey_proto->name, users, files,
					size);
}