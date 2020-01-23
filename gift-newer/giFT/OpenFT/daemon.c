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

#include "openft.h"

#include "netorg.h"
#include "search.h"

/*****************************************************************************/

/**/extern Connection *ft_self;
/**/extern unsigned long search_unique_id;
/**/extern HashTable *ft_shares;

/*****************************************************************************/

typedef void (*EventCallback) (Protocol *p, Connection *c, Dataset *event);
#define EV_HANDLER(func) static void ft_##func (Protocol *p, Connection *c, Dataset *event)

EV_HANDLER (daemon_promote);
EV_HANDLER (daemon_search);
EV_HANDLER (daemon_share);
EV_HANDLER (daemon_stats);
#ifdef DEBUG
EV_HANDLER (daemon_debug);
#endif /* DEBUG */

/*****************************************************************************/

struct _daemon_event
{
	char *name;
	EventCallback cb;
}
daemon_events[] =
{
	{ "promote",    ft_daemon_promote },
	{ "search",     ft_daemon_search  },
	{ "share",      ft_daemon_share   },
	{ "stats",      ft_daemon_stats   },
#ifdef DEBUG
	{ "debug",      ft_daemon_debug   },
#endif /* DEBUG */
	{ NULL,         NULL              },
};

/*****************************************************************************/

void ft_daemon_callback (Protocol *p, Connection *c, Dataset *event)
{
	struct _daemon_event *ptr;
	char *ev_name;

	ev_name = dataset_lookup (event, "head");

	for (ptr = daemon_events; ptr->name; ptr++)
	{
		if (strcasecmp (ptr->name, ev_name))
			continue;

		/* found the appropriate event */
		(*ptr->cb) (p, c, event);

		return;
	}

	printf ("no handler found for %s\n", ev_name);
}

/*****************************************************************************/

static Connection *broadcast_status (Connection *c, Node *node, void *data)
{
	ft_packet_send (c, FT_CLASS_RESPONSE, "%hu", NODE (ft_self)->class);

	return NULL;
}

EV_HANDLER (daemon_promote)
{
	node_class_add (ft_self, NODE_SEARCH);

	conn_foreach ((ConnForeachFunc) broadcast_status, NULL,
	              NODE_USER, NODE_CONNECTED, 0);
}

/*****************************************************************************/

#if 0
EV_HANDLER (daemon_connect)
{
	char *ip, *port;

	ip   = dataset_lookup (event, "ip");
	port = dataset_lookup (event, "port");

	if (!ip || !port)
		return;

	ft_connect (p, ip, atoi (port));
}
#endif

/*****************************************************************************/

static Connection *broadcast_search (Connection *c, Node *node, Search *search)
{
	int type;

	type = search->type;

	/* trying to catch a bug :) */
	assert (type < 100);

#ifdef SEARCH_PARANOID
	if (type & SEARCH_FILENAME)
	{
		type |= SEARCH_HIDDEN;

		ft_packet_send (c, FT_SEARCH_REQUEST, "%lu%hu*lu*lu%s%lu%lu%lu%lu",
		                search->id, type, search->qtokens, search->etokens,
		                search->realm, search->size_min, search->size_max,
		                search->kbps_min, search->kbps_max);
	}
	else
#else /* !SEARCH_PARANOID */
		ft_packet_send (c, FT_SEARCH_REQUEST, "%lu%hu%s%s%s%lu%lu%lu%lu",
		                search->id, type, search->query, search->exclude,
		                search->realm, search->size_min, search->size_max,
		                search->kbps_min, search->kbps_max);
#endif /* SEARCH_PARANOID */

	search->ref++;

	return NULL;
}

static void constraint_parse (char *str, size_t *min, size_t *max)
{
	char *str0;
	char *min_str;

	*min = 0;
	*max = 0;

	if (!str)
		return;

	str0 = str = STRDUP (str);

	min_str = string_sep (&str, "-");
	*min    = ATOUL (min_str);

	if (str && *str)
		*max = ATOUL (str);

	free (str0);
}

EV_HANDLER (daemon_search)
{
	Search *search;
	int     search_type;
	char   *query;
	char   *exclude;
	char   *realm;
	char   *type;
	size_t  size_min, size_max;
	size_t  kbps_min, kbps_max;

	query   = dataset_lookup (event, "query");
	exclude = dataset_lookup (event, "exclude");
	type    = dataset_lookup (event, "type");
	realm   = dataset_lookup (event, "realm");

	if (!query)
		return;

	constraint_parse (dataset_lookup (event, "csize"), &size_min, &size_max);
	constraint_parse (dataset_lookup (event, "ckbps"), &kbps_min, &kbps_max);

	if (!type)
		search_type = SEARCH_FILENAME;
	else
	{
		if (!strcasecmp (type, "hash"))
			search_type = SEARCH_MD5;
		else if (!strcasecmp (type, "user"))
			search_type = SEARCH_HOST;
	}

	/* create the local OpenFT search object */
	search = search_new ((IFEventID) P_INT (dataset_lookup (event, "id")),
	                     search_type, query, exclude, realm,
						 size_min, size_max, kbps_min, kbps_max);

	if (NODE (ft_self)->class & NODE_SEARCH)
	{
		List *results;
		List *ptr;

		results = ft_search (NULL, search_type, query, exclude, realm,
							 size_min, size_max, kbps_min, kbps_max);

		for (ptr = results; ptr; ptr = list_next (ptr))
		{
			FileShare *file  = ptr->data;
			FT_Share  *share = share_lookup_data (file, "OpenFT");

			/* ignore local shares */
			if (share->host_share->host != 0)
				search_reply (search->id, ft_self, file);

			ft_share_unref (file);
		}

		list_free (results);
	}

	/* local connections are sort of an exception.  they do not count as a
	 * reference as it completes in the loop above */
	search->ref = 0;

	conn_foreach ((ConnForeachFunc) broadcast_search, search,
	              NODE_SEARCH, NODE_CONNECTED, 0);

	printf ("search->ref = %i\n", search->ref);

	if (!search->ref)
	{
		/* basically call search_remove, but handle it more naturally */
		search->ref = 1;
		search_reply (search->id, ft_self, NULL);
	}
}

/*****************************************************************************/

#if 0
static Connection *share_sync (Connection *c, Node *node, void *data)
{
	/* sync w/ the remote search node by flushing then re-adding...
	 * very inefficient ... sigh */
	ft_packet_send (c, FT_MODSHARE_REQUEST, NULL);
	ft_share_local_submit (c);

	return NULL;
}
#endif

static Connection *share_hide (Connection *c, Node *node, void *data)
{
	ft_packet_send (c, FT_MODSHARE_REQUEST, "%hu",
	                1 /* HIDE SHARES */);

	return NULL;
}

static Connection *share_show (Connection *c, Node *node, void *data)
{
	ft_packet_send (c, FT_MODSHARE_REQUEST, "%hu",
	                2 /* SHOW SHARES */);

	return NULL;
}

EV_HANDLER (daemon_share)
{
	char *action;

	action = dataset_lookup (event, "action");

	if (!action || !strcmp (action, "sync"))
	{
		/* everything we would've wanted to do has already been taken care
		 * of by giFT */
	}
	else if (!strcmp (action, "hide"))
	{
		conn_foreach ((ConnForeachFunc) share_hide, NULL,
		              NODE_PARENT, NODE_CONNECTED, 0);
	}
	else if (!strcmp (action, "show"))
	{
		conn_foreach ((ConnForeachFunc) share_show, NULL,
		              NODE_PARENT, NODE_CONNECTED, 0);
	}
}

/*****************************************************************************/

struct _stats
{
	unsigned long users;
	unsigned long files;
	double        size; /* in gigs */
	int connections;
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

EV_HANDLER (daemon_stats)
{
	struct _stats stats;

	memset (&stats, 0, sizeof (stats));

	/* gather stats from all index nodes, use the longest living */
	conn_foreach ((ConnForeachFunc) gather_stats, &stats,
	              NODE_INDEX, NODE_CONNECTED, 0);

	/* just to keep Online/Offline happy */
	stats.connections +=
		conn_length (NODE_CHILD | NODE_SEARCH, NODE_CONNECTED);

	interface_send (c, "stats",
	                "id=i",       P_INT (dataset_lookup (event, "id")),
	                "protocol=s", openft_proto->name,
	                "status=s",   stats.connections ? "Online" : "Offline",
	                "users=i",    stats.users,
	                "files=i",    stats.files,
	                "size=i",     (unsigned long) stats.size,
	                NULL);
}

/*****************************************************************************/

#ifdef DEBUG
static int mem_usage_file (unsigned long key, FileShare *file,
                           size_t *mem_usage)
{
	FT_Share  *share;
	ft_uint32 *tokens;

	/* include the node */
	if (key)
		*mem_usage += sizeof (HashNode);
	else
		*mem_usage += sizeof (List);

	*mem_usage += sizeof (FileShare);

	if (file->sdata)
	{
		if (file->sdata->root)
			*mem_usage += strlen (file->sdata->root);

		if (file->sdata->md5)
			*mem_usage += strlen (file->sdata->md5);

		if (file->sdata->path)
			*mem_usage += strlen (file->sdata->path);
	}

	if (!(share = share_lookup_data (file, "OpenFT")))
		return TRUE;

	*mem_usage += sizeof (FT_Share);

	for (tokens = share->tokens; tokens && *tokens; tokens++)
		*mem_usage += sizeof (ft_uint32);

	/* include the sentinel */
	if (tokens)
		*mem_usage += sizeof (ft_uint32);

	return TRUE;
}

static int mem_usage_key_as_host (unsigned long key, FT_HostShare *host_share,
                                  size_t *mem_usage)
{
	hash_table_foreach (host_share->dataset,
	                    (HashFunc) mem_usage_file, mem_usage);

	return TRUE;
}

static Connection *mem_usage_node (Connection *c, Node *node,
                                   size_t *mem_usage)
{
	*mem_usage += sizeof (Connection);
	*mem_usage += sizeof (Node);

	return NULL;
}

EV_HANDLER (daemon_debug)
{
	size_t mem_usage_local  = 0;
	size_t mem_usage_shares = 0;
	size_t mem_usage_nodes  = 0;

	/* calculate rough memory usage from all structures that exist...as to
	 * guage how badly this shit leaks */
	share_foreach ((HashFunc) mem_usage_file, &mem_usage_local);

	TRACE (("local:  %i", mem_usage_local));

	hash_table_foreach (ft_shares, (HashFunc) mem_usage_key_as_host,
	                    &mem_usage_shares);

	TRACE (("shares: %i", mem_usage_shares));

	conn_foreach ((ConnForeachFunc) mem_usage_node, &mem_usage_nodes,
	              NODE_NONE, -1, 0);

	TRACE (("nodes:  %i", mem_usage_nodes));

	TRACE (("total:  %i",
	        mem_usage_local + mem_usage_shares + mem_usage_nodes));
}
#endif /* DEBUG */
