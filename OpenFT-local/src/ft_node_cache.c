/*
 * $Id: ft_node_cache.c,v 1.24 2004/11/02 22:10:10 hexwab Exp $
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

#include "ft_openft.h"

#include "ft_netorg.h"
#include "ft_node_cache.h"

/*****************************************************************************/

struct cache_state
{
	FILE      *f;
	BOOL       error;
	BOOL       unconfirmed;
	ft_class_t klass;
};

static ft_class_t class_priority[] =
{
	FT_NODE_INDEX,
	FT_NODE_SEARCH,
	FT_NODE_USER
};

#define PRIO_LEN (sizeof(class_priority) / sizeof(class_priority[0]))

/*****************************************************************************/

/*
 * Last known modification time of the ~/.giFT/OpenFT/nodes cache file.  This
 * is used to autodetect changes to the file (every few minutes) and reload
 * all new nodes.  It will also detect removal of the file for the purpose of
 * re-reading from the globally installed nodes file.
 */
static time_t nodes_mtime = 0;

/*****************************************************************************/

static int import_cache (FILE *f, const char *path)
{
	FTNode        *node;
	int            nodes = 0;
	char          *buf = NULL;
	char          *buf_ptr;
	time_t         vitality;
	time_t         uptime;
	char          *host;
	in_port_t      port;
	in_port_t      http_port;
	unsigned short klass;
	uint32_t       version;

	while (file_read_line (f, &buf))
	{
		in_addr_t ip;
		buf_ptr = buf;

		/* parse out all the required fields destructively */
		vitality  = (time_t)        ATOUL(string_sep (&buf_ptr, " "));
		uptime    = (time_t)        ATOUL(string_sep (&buf_ptr, " "));
		host      =                       string_sep (&buf_ptr, " ");
		port      = (in_port_t)      ATOI(string_sep (&buf_ptr, " "));
		http_port = (in_port_t)      ATOI(string_sep (&buf_ptr, " "));
		klass     = (unsigned short) ATOI(string_sep (&buf_ptr, " "));
		version   = (uint32_t)     ATOUL(string_sep (&buf_ptr, " "));

		/* incomplete line */
		if (!host || !version)
		{
			FT->warn (FT, "ignoring incomplete line in %s (%i)", path, nodes);
			continue;
		}

		if ((ip = net_ip (host)) != INADDR_NONE)
		{
			/* register this ip, just to add it to the netorg list, but don't
			 * connect here */
			if (!(node = ft_node_register_full (ip, port, http_port,
		                                    (ft_class_t)klass, vitality,
		                                    uptime, version)))
				continue;
		}
		else
		{
			/* Yes this blocks, but is only used on
			 * initial connection. */
			struct hostent *he = gethostbyname (host);
			in_addr_t **ptr;

			if (!he	|| he->h_addrtype != AF_INET ||
			    he->h_length != sizeof (in_addr_t))
				continue;

			for (ptr = (in_addr_t **)he->h_addr_list; *ptr; ptr++)
				ft_node_register_full (**ptr, port, http_port,
						       (ft_class_t)klass, vitality,
						       uptime, version);
		}

		nodes++;
	}

	return nodes;
}

static int read_cache (void)
{
	FILE *f;
	char *path;
	int   nodes;

	if (!(path = gift_conf_path ("OpenFT/nodes")))
		return 0;

	FT->DBGFN (FT, "opening nodes cache from %s...", path);

	/* attempt to open the locally installed nodes file, if this fails we
	 * should try looking up the global */
	if ((f = fopen (path, "r")) == NULL)
	{
		path = stringf ("%s/OpenFT/nodes", platform_data_dir());
		FT->DBGFN (FT, "falling back to %s...", path);

		if ((f = fopen (path, "r")) == NULL)
		{
			FT->warn (FT, "unable to locate a nodes file...this is very bad, "
			              "consult the documentation");
			return 0;
		}
	}

	/* actually execute the read_line's on the file and register each node
	 * we come across */
	nodes = import_cache (f, path);
	fclose (f);

	if (nodes)
		FT->DBGFN (FT, "successfully read %i nodes", nodes);
	else
	{
        FT->err (FT, "No nodes loaded.  If you believe this is in error, try "
		             "removing your local nodes file, causing giFT to "
		             "re-read from the global.  If you are still having "
		             "troubles, try consulting the installation guide.");
	}

	return nodes;
}

/*****************************************************************************/

static int write_node (FTNode *node, struct cache_state *state)
{
	time_t     vitality;
	time_t     uptime;
	ft_class_t klass;
	int        wrote;

	/* make sure the class we're interested in is the most
	 * significant class this node has */
	if ((node->ninfo.klass & FT_NODE_CLASSPRI_MASK) >= (state->klass << 1))
		return FALSE;

	/* no reason to write out firewalled nodes or users which we would
	 * deny a connection to anyway... */
	if (ft_node_fw (node) || FT_VERSION_LT (node->version, FT_VERSION_LOCAL))
		return FALSE;

	/*
	 * Determine the time of the last established session.
	 *
	 * TODO: Avoid multiple time(2) calls by simply passing the result from
	 * the caller to this iterator function.
	 */
	if (node->state == FT_NODE_CONNECTED)
		vitality = time (NULL);
	else
		vitality = node->last_session;

	/* don't store nodes we've never connected to, unless we're
	 * desperate */
	if (state->unconfirmed ^ !vitality)
		return FALSE;

	/*
	 * Calculate the current uptime by determining the last known recorded
	 * uptime (set at session close or when reading from the nodes file) and
	 * adding the currently active session duration if there is one.
	 */
	uptime = node->uptime;
	uptime += ft_session_uptime (FT_CONN(node));

	/* get the class without session-specific class modifiers (such as child
	 * and parent relationships) */
	klass = ft_node_class (node, FALSE);

	wrote =
	  fprintf (state->f, "%li %li %s %hu %hu %hu %u\n",
	           (long)vitality, (long)uptime, net_ip_str (node->ninfo.host),
	           node->ninfo.port_openft, node->ninfo.port_http,
	           (unsigned short)klass, (unsigned int)node->version);

	if (wrote <= 0)
	{
		if (state->error == FALSE)
			FT->err (FT, "error writing nodes cache: %s", GIFT_STRERROR());

		state->error = TRUE;

		return FALSE;
	}

	return TRUE;
}

static int rewrite_cache (char *path)
{
	struct cache_state state;
	char *pathtmp;
	int nodes;
	int remaining;
	int i;

	/* use a temp path so we dont clobber the original if something goes
	 * haywire */
	if (!(pathtmp = stringf ("%s.tmp", path)))
		return 0;

	if (!(state.f = fopen (pathtmp, "w")))
	{
		FT->err (FT, "can't create %s: %s", pathtmp, GIFT_STRERROR());
		return 0;
	}

	nodes = 0;
	state.error = FALSE;

	for (i = 0; i < PRIO_LEN * 2; i++)
	{
		remaining = FT_CFG_NODES_CACHE_MAX - nodes;

		if (remaining <= 0)
			break;

		state.klass = class_priority[i % PRIO_LEN];
		state.unconfirmed = BOOL_EXPR (i >= PRIO_LEN);
		nodes += ft_netorg_foreach (state.klass, FT_NODE_STATEANY, remaining,
		                            FT_NETORG_FOREACH(write_node), &state);
	}

	if (fclose (state.f) != 0)
	{
		if (state.error == FALSE)
			FT->err (FT, "error writing nodes cache: %s", GIFT_STRERROR());

		state.error = TRUE;
	}

	/* get rid of the temp path */
	if (state.error == FALSE)
		file_mv (pathtmp, path);

	return nodes;
}

/*
 * Make sure that the nodes cache file has been read properly.  Also, attempt
 * to sense changes to the file (see mtime above).  This code also ensures
 * that the local nodes cache reflects the current state in memory.
 */
int ft_node_cache_update (void)
{
	char       *path;
	struct stat st;
	int         stret;
	int         nodes;

	if (!(path = gift_conf_path ("OpenFT/nodes")))
		return 0;

	/* check the nodes file current mtime */
	stret = stat (path, &st);

	/* this will only add to the state in memory, truncating the nodes
	 * file would have no effect */
	if ((nodes_mtime == 0) || (stret == 0 && nodes_mtime != st.st_mtime))
		read_cache();

	/* ugh */
	if (stret == -1)
		FT->warn (FT, "*** creating new nodes file: %s", path);

	/* now go ahead and rewrite a new nodes cache from the state in memory */
	nodes = rewrite_cache (path);

	/* store the new mtime (after we wrote to the file) for subsequent calls
	 * to this function */
	if (stat (path, &st) == 0)
		nodes_mtime = st.st_mtime;

	return nodes;
}
