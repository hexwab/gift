/*
 * $Id: ft_node_cache.c,v 1.9 2003/05/05 09:49:09 jasta Exp $
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

/*
 * Last known modification time of the ~/.giFT/OpenFT/nodes cache file.  This
 * is used to autodetect changes to the file (every few minutes) and reload
 * all new nodes.  It will also detect removal of the file for the purpose of
 * re-reading from the globally installed nodes file.
 */
static time_t nodes_mtime = 0;

/*****************************************************************************/

static int import_cache (FILE *f, char *path)
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

		/* register this ip, just to add it to the netorg list, but don't
		 * connect here */
		if (!(node = ft_node_register_full (net_ip (host), port, http_port,
		                                    (FTNodeClass)klass, vitality,
		                                    uptime, version)))
			continue;

#if 0
		ft_node_set_port (node, port);
		ft_node_set_http_port (node, http_port);
		ft_node_set_class (node, (FTNodeClass)klass);

		/* manually assign a few cached members */
		node->last_session = vitality;
		node->uptime       = uptime;
		node->version      = version;
#endif

		nodes++;
	}

	return nodes;
}

static int read_cache ()
{
	FILE *f;
	char *path;
	int   nodes;

	if (!(path = gift_conf_path ("OpenFT/nodes")))
		return 0;

	FT->DBGFN (FT, "opening nodes cache from %s...", path);

	/* attempt to open the locally installed nodes file, if this fails we
	 * should try looking up the global */
	if (!(f = fopen (path, "r")))
	{
		path = stringf ("%s/OpenFT/nodes", platform_data_dir());
		FT->DBGFN (FT, "falling back to %s...", path);

		f = fopen (path, "r");
	}

	if (!f)
	{
		FT->warn (FT, "unable to locate a nodes file...this is very bad, "
		              "consult the documentation");
		return 0;
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

static int write_node_cache (FTNode *node, FILE *f)
{
	time_t      vitality;
	time_t      uptime;
	FTNodeClass klass;

	/* no reason to write out firewalled nodes */
	if (ft_node_fw (node) || !node->version)
		return FALSE;

	/* determine the time of the last established session */
	if (node->state == NODE_CONNECTED)
		vitality = time (NULL);
	else
		vitality = node->last_session;

	/* previously known uptime + current, as node->uptime is not updated
	 * until after the connection has closed
	 * NOTE: c may be NULL if no connection is currently established, but
	 * ft_session_uptime will just return 0 */
	uptime = node->uptime + ft_session_uptime (FT_CONN(node));

	/* get the class without session-specific class modifiers (such as child
	 * and parent relationships) */
	klass = ft_node_class (node, FALSE);

	fprintf (f, "%li %li %s %hu %hu %hu %u\n",
	         (long)vitality, (long)uptime, net_ip_str (node->ip),
	         node->port, node->http_port,
	         (unsigned short)klass, (unsigned int)node->version);

	return TRUE;
}

static int rewrite_cache (char *path)
{
	FILE *f;
	char *pathtmp;
	int   nodes;

	/* use a temp path so we dont clobber the original if something goes
	 * haywire */
	if (!(pathtmp = stringf ("%s.tmp", path)))
		return 0;

	if (!(f = fopen (pathtmp, "w")))
	{
		FT->err (FT, "can't create %s: %s", pathtmp, GIFT_STRERROR());
		return 0;
	}

	/* loop through all nodes and write them out to the nodes cache */
	nodes = ft_netorg_foreach (0x00, 0x00, 0,
	                           FT_NETORG_FOREACH(write_node_cache), f);

	fclose (f);

	/* get rid of the temp path */
	file_mv (pathtmp, path);

	return nodes;
}

/*
 * Make sure that the nodes cache file has been read properly.  Also, attempt
 * to sense changes to the file (see mtime above).  This code also ensures
 * that the local nodes cache reflects the current state in memory.
 */
int ft_node_cache_update ()
{
	char       *path;
	int         path_exists;
	struct stat st;
	int         nodes;

	if (!(path = gift_conf_path ("OpenFT/nodes")))
		return 0;

	/* check the nodes file current mtime */
	path_exists = file_stat (path, &st);

	/* this will only add to the state in memory, truncating the nodes
	 * file would have no effect */
	if ((!path_exists && nodes_mtime) || nodes_mtime != st.st_mtime)
		read_cache ();

	if (!path_exists)
		FT->warn (FT, "*** creating new nodes file: %s", path);

	/* now go ahead and rewrite a new nodes cache from the state in memory */
	nodes = rewrite_cache (path);

	/* store the new mtime (after we wrote to the file) for subsequent calls
	 * to this function */
	if ((file_stat (path, &st)))
		nodes_mtime = st.st_mtime;

	return nodes;
}
