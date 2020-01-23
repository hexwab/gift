/*
 * $Id: gt_gnutella.c,v 1.37 2003/06/27 10:49:40 jasta Exp $
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

#include "gt_gnutella.h"

#include "gt_conf.h"

#include "sha1.h"

#include "gt_share.h"

#include "gt_accept.h"

#include "gt_node.h"
#include "gt_netorg.h"

#include "ft_xfer.h"
#include "gt_xfer.h"

#include "gt_search.h"

#include "gt_web_cache.h"

#include "gt_stats.h"

#include "gt_query_route.h"

/*****************************************************************************/

/* node pointer for this machine */
GtNode         *gt_self;

/* giFT protocol pointer */
Protocol       *gt_proto;

/* HTTP Protocol information */
HTTP_Protocol  *gt_http;

/* the static guid identifying this client -- this should remain the
 * same across restarts to give pushes some (probably very slim) chance
 * to operate across them */
gt_guid_t      *gt_client_guid;

/*****************************************************************************/

static time_t   start_time;

/*****************************************************************************/

char *gt_version (void)
{
	return "giFT-Gnutella/" GT_VERSION;
}

time_t gt_uptime (void)
{
	return time (NULL) - start_time;
}

/* The ip address is local if the address is local and
 * the source from which it was discovered was not local */
int gt_is_local_ip (in_addr_t ip, in_addr_t src)
{
	if (ip == 0)
		return TRUE;

	if (net_match_host (ip, "LOCAL") &&
	    (src == 0 || !net_match_host (src, "LOCAL")))
	{
		return TRUE;
	}

	return FALSE;
}

/*****************************************************************************/

static char *fw_file (void)
{
	return gift_conf_path ("Gnutella/fwstatus");
}

static void save_fw_status (void)
{
	FILE  *f;

	if (!(f = fopen (fw_file (), "w")))
		return;

	/* Store the last successful time of connect.
	 * But don't do that yet, store 1 or 0 as appropriate for now. */
	fprintf (f, "%d\n", GT_SELF->firewalled ? 0 : 1);
	fclose (f);
}

static int load_fw_status (void)
{
	FILE   *f;
	char    buf[RW_BUFFER];
	int     i;

	if (!(f = fopen (fw_file (), "r")))
		return TRUE;

	if (fgets (buf, sizeof (buf) - 1, f) == NULL)
	{
		fclose (f);
		return TRUE;
	}

	/* Store the time of the last successful connect, so
	 * > 0 means _not_ firewalled */
	if (sscanf (buf, "%d", &i) == 1 && i > 0)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/

/* shutdown */
static void gnutella_destroy (Protocol *p)
{
	GT->DBGFN (GT, "entered");

	/* cleanup any network maintenance data */
	gt_netorg_cleanup ();

	/* destroy query_router tables */
	gt_query_router_self_destroy ();

	/* save firewalled status */
	save_fw_status ();

	/* free ourself */
	gt_node_free (gt_self);
	gt_self = NULL;

	/* free and disconnect all nodes */
	gt_node_remove_all ();

	/* destroy web cache information */
	gt_web_cache_cleanup ();

	/* free the client guid */
	free (gt_client_guid);
	gt_client_guid = NULL;

	/* unregister http */
	http_protocol_unregister (gt_http);
	free (gt_http);
	gt_http = NULL;

	/* free configuration information */
	gt_config_cleanup ();

	/* set time we were running back to 0 */
	start_time = 0;
}

static unsigned char *gnutella_hash_sha1 (const char *path, size_t *len)
{
	*len = SHA1_BINSIZE;
	return sha1_digest (path, 0);
}

static char *gnutella_sha1_human (unsigned char *hash)
{
	return sha1_string (hash);
}

static GtNodeClass read_class (void)
{
	char *klass;

	klass = gt_config_get_str ("main/class");

	if (klass && strstr (klass, "ultra"))
		return GT_NODE_ULTRA;

	return GT_NODE_LEAF;
}

static GtNode *bind_gnutella_port (in_port_t port)
{
	GtNode  *node;
	TCPC    *c;

	GT->DBGFN (GT, "entered");

	if (!(node = gt_node_new ()))
		return NULL;

	node->gt_port    = 0;
	node->firewalled = load_fw_status ();

	if (!port || !(c = tcp_bind (port, FALSE)))
	{
		GT->warn (GT, "Failed binding port %d, setting firewalled", port);
		node->firewalled = TRUE;
		return node;
	}

	GT->dbg (GT, "bound to port %d", port);

	/* attach the connection to this node */
	gt_node_connect (node, c);
	node->gt_port = port;

	/* load the class for this node */
	node->klass = read_class();

	input_add (c->fd, c, INPUT_READ,
	           (InputCallback)gnutella_handle_incoming, FALSE);

	return node;
}

static gt_guid_t *get_client_id (char *conf_path)
{
	FILE      *f;
	gt_guid_t *client_id = NULL;
	char      *buf       = NULL;

	if ((f = fopen (gift_conf_path (conf_path), "r")))
	{
		while (file_read_line (f, &buf))
		{
			char *id;
			char *line;

			free (client_id);
			client_id = NULL;

			line = buf;
			id = string_sep_set (&line, "\r\n");

			if (string_isempty (id))
				continue;

			client_id = gt_guid_bin (id);
		}

		fclose (f);
	}

	/* create the id */
	if (!client_id)
	{
		client_id = gt_guid_new ();
		assert (client_id != NULL);
	}

	/* store the id in ~/.giFT/Gnutella/clientid */
	if (!(f = fopen (gift_conf_path (conf_path), "w")))
	{
		GIFT_ERROR (("clientid storage file: %s", GIFT_STRERROR ()));
		return client_id;
	}

	fprintf (f, "%s\n", gt_guid_str (client_id));
	fclose (f);

	return client_id;
}

static void load_nodes_file (char *conf_path, in_port_t klass)
{
	GtNode *node;
	FILE *f;
	char *buf = NULL;
	char *ptr;

	f = fopen (gift_conf_path (conf_path), "r");

	/* try the global nodes file */
	if (!f)
	{
		char *filename;

		if (!(filename = malloc (strlen (platform_data_dir ()) + 50)))
			return;

		sprintf (filename, "%s/%s", platform_data_dir (), conf_path);

		f = fopen (filename, "r");

		free (filename);
	}

	if (!f)
		return;

	while (file_read_line (f, &buf))
	{
		unsigned long  vitality;
		char          *ip;
		in_port_t      port;

		ptr = buf;

		/* [vitality] [ip]:[port] */

		vitality  = ATOUL (string_sep (&ptr, " "));
		ip        =        string_sep (&ptr, ":");
		port      = ATOI  (string_sep (&ptr, " "));

		if (!ip)
			continue;

		node = gt_node_register (inet_addr (ip), port, klass, 0, 0);

		if (!node)
			continue;

		node->vitality = vitality;
	}

	fclose (f);
}

static int gnutella_start (Protocol *p)
{
	int port;

	/* set the last time we were running to now */
	start_time = time (NULL);

	if (!gt_config_init ())
	{
		GIFT_ERROR (("Unable to load config file. Please copy it to "
		             "~/.giFT/Gnutella/Gnutella.conf"));
		return FALSE;
	}

	port = gt_config_get_int ("main/port=6346");

	/* register Gnutella's variant of HTTP with the giFT daemon */
	gt_http = http_protocol_new (gt_proto);
	gt_http->localize_cb = gt_localize_request;

	http_protocol_register (gt_http);

	/* read the client id from ~/.giFT/Gnutella/clientid or create it */
	gt_client_guid = get_client_id ("Gnutella/clientid");

	/* listen for connections */
	gt_self = bind_gnutella_port (port);

	load_nodes_file ("Gnutella/index_nodes", GT_NODE_INDEX);
	load_nodes_file ("Gnutella/nodes",       GT_NODE_NONE);

	gt_conn_sort ((CompareFunc) gt_conn_sort_vit);

	/* initialize query router tables */
	gt_query_router_self_init ();

	if (!gt_web_cache_init ())
	{
		GIFT_ERROR (("Unable to load gwebcaches file. Please copy it to "
		             "~/.giFT/Gnutella/gwebcaches"));
		return FALSE;
	}

	/* startup network maintenance */
	gt_netorg_init ();

	return TRUE;
}

/*
 * The entry-point for the giFT daemon
 */
int Gnutella_init (Protocol *p)
{
	if (protocol_compat (LIBGIFTPROTO_MKVERSION (0, 10, 1)) != 0)
		return FALSE;

	p->DBGFN (p, "Don't Panic!");

	gt_proto = p;

	/* describe the hash algo */
	p->hash_handler (p, "SHA1", HASH_PRIMARY, gnutella_hash_sha1,
	                 gnutella_sha1_human);

	/* gt_gnutella.c: */
	p->start          = gnutella_start;
	p->destroy        = gnutella_destroy;

	/* gt_search.c: */
	p->search         = gnutella_search;
#if 0
	p->browse         = gnutella_browse;
#endif
	p->locate         = gnutella_locate;
	p->search_cancel  = gnutella_search_cancel;

	/* gt_xfer.c: */
	p->download_start = gnutella_download_start;
	p->download_stop  = gnutella_download_stop;
	p->upload_stop    = gnutella_upload_stop;
	p->chunk_suspend  = gnutella_chunk_suspend;
	p->chunk_resume   = gnutella_chunk_resume;
	p->source_cmp     = gnutella_source_cmp;
	p->source_remove  = gnutella_source_remove;
#if 0
	p->upload_avail   = gnutella_upload_avail;
	p->user_cmp       = gnutella_user_cmp;
#endif

	/* gt_share.c: */
	p->share_new      = gnutella_share_new;
	p->share_free     = gnutella_share_free;
	p->share_add      = gnutella_share_add;
	p->share_remove   = gnutella_share_remove;
	p->share_sync     = gnutella_share_sync;

	/* gt_stats.c: */
	p->stats          = gnutella_stats;

	return TRUE;
}
