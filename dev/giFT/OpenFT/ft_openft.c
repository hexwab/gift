/*
 * ft_openft.c
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

#include "ft_openft.h"

#include "ft_netorg.h"
#include "ft_conn.h"
#include "ft_node_cache.h"
#include "ft_search.h"

#include "ft_xfer.h"
#include "ft_html.h"

#include "file.h"
#include "parse.h"

#include "md5.h"

/*****************************************************************************/

#define MAIN_TIMER (2 * MINUTES)
#define NODE_ALIAS config_get_str (OPENFT->conf, "main/alias")

/*****************************************************************************/

static Protocol *openft = NULL;

/*****************************************************************************/

static OpenFT *create_udata (Protocol *p)
{
	OpenFT *udata;

	if (!(udata = MALLOC (sizeof (OpenFT))))
		return NULL;

	return udata;
}

static void free_udata (Protocol *p)
{
	if (!p)
		return;

	free (p->udata);
}

/*****************************************************************************/

static Connection *http_start (unsigned short port)
{
	Connection *c;

	if (!(c = connection_bind (openft_p, port, FALSE)))
		return NULL;

	input_add (openft_p, c, INPUT_READ,
	           (InputCallback)http_server_incoming, FALSE);

	/* set the http port on the local node structure, hopefully this is sort
	 * of temporary code pending a redesign of the FTNode structure */
	ft_node_set_http_port (FT_SELF, port);

	return c;
}

static Connection *ft_start (FTNodeClass klass, unsigned short port)
{
	Connection    *c;
	FTNode        *node;
	unsigned short bind_port;

	/* firewalled users can not go beyond the user class */
	if (port == 0)
		klass = NODE_USER;

#ifndef USE_LIBDB
	/* search status requires libdb support built in at compile time...if they
	 * have selected this class in error, we will fail here in order to
	 * ensure that the user sees this error message */
	if (klass & NODE_SEARCH)
	{
		GIFT_ERROR (("dropping NODE_SEARCH status due to lacking libdb "
		             "support"));
		return NULL;
	}
#endif /* !USE_LIBDB */

	/*
	 * If port is configured as 0, openft is expected to treat this as a
	 * firewalled node, that is one which cannot directly accept connections
	 * on the core openft (or http) ports.  We will still create a local node
	 * structure (FT_SELF), but we will not attempt to associate the the
	 * actual listening port (default 1215) so that it does not report this
	 * to other nodes.
	 */
	bind_port = (port ? port : 1215);
	if (!(c = connection_bind (openft_p, bind_port, FALSE)))
		return NULL;

	if (!(node = ft_node_new (0)))
	{
		connection_close (c);
		return NULL;
	}

	node->session = MALLOC (sizeof (FTSession));
	node->session->cap = dataset_new (DATASET_LIST);
	node->session->c = c;

	ft_node_set_state (node, NODE_CONNECTED);
	ft_node_set_class (node, klass);
	ft_node_set_port  (node, port);
	ft_node_set_alias (node, NODE_ALIAS);
	c->data = node;

	input_add (openft_p, c, INPUT_READ,
	           (InputCallback)ft_session_incoming, FALSE);

	return c;
}

/*****************************************************************************/

static unsigned char *openft_md5 (char *path, char *type, int *len)
{
	*len = 16;
	return md5_digest (path, 0);
}

static char *openft_md5_human (unsigned char *md5, int *len)
{
	*len = 32;
	return md5_string (md5);
}

static int openft_start (Protocol *p)
{
	unsigned short ft_port;
	unsigned short http_port;
	unsigned short klass;

	/* open OpenFT specific configuration data and assign it as a global
	 * for use all over this plugin */
	if (!(OPENFT->conf = gift_config_new ("OpenFT")))
	{
		GIFT_ERROR (("unable to load OpenFT configuration"));
		return FALSE;
	}

	/* gather basic information about our nodes connectivity and role in
	 * the network */
	ft_port   = config_get_int (OPENFT->conf, "main/port=1215");
	http_port = config_get_int (OPENFT->conf, "main/http_port=1216");
	klass     = config_get_int (OPENFT->conf, "main/class=1");

#if 0
	TRACE (("info: ft_port = %hu (%sfirewalled), http_port = %hu, klass = %s",
	        ft_port, (ft_port > 0) ? "non-" : "", http_port,
	        node_class_str (klass)));
#endif

	/* bind the appropriate listening ports */
	if ((OPENFT->ft = ft_start ((FTNodeClass)klass, ft_port)))
		OPENFT->http = http_start (http_port);

	/* make sure we didnt encounter any strange errors here, eventually giFT
	 * will automatically call p->destroy for us and then startup a timer for
	 * retrying p->start, but for now I think it just assert's that we not
	 * return FALSE here ;) */
	if (!OPENFT->ft || !OPENFT->http)
	{
		GIFT_ERROR (("unable to successfully bind listening ports, aborting "
		             "OpenFT startup"));
		return FALSE;
	}

    /* add this nodes capabilities */
#ifdef USE_ZLIB
	dataset_insertstr (&FT_SELF->session->cap, "ZLIB", "ZLIB");
#endif /* USE_ZLIB */
	dataset_insertstr (&FT_SELF->session->cap, "MD5-FULL", "MD5-FULL");

	/* setup the connection maintenance timer (2 minute intervals) */
	OPENFT->main_timer =
	    timer_add (MAIN_TIMER, (TimerCallback)ft_conn_maintain, NULL);

	/* delete any host shares left over from previous search node sessions */
	file_rmdir (gift_conf_path ("OpenFT/db"));

	/* OpenFT, here we come :) */
	return ft_conn_initial ();
}

static int cleanup_conn (FTNode *node, void *data)
{
	ft_session_stop (FT_CONN(node));
	return TRUE;
}

static void openft_destroy (Protocol *p)
{
	int nodes;

	TRACE_FUNC ();

	/* boolean used by node_disconnect to disable certain behavior */
	OPENFT->shutdown = TRUE;

	/* write nodes cache */
	nodes = ft_node_cache_update ();
	TRACE (("flushed %d nodes", nodes));

	/* disconnect everyone for cleanliness */
	ft_netorg_clear (FT_NETORG_FOREACH(cleanup_conn), NULL);

	/* cleanup */
	config_free (OPENFT->conf);
	timer_remove (OPENFT->main_timer);
	connection_close (OPENFT->http);
	connection_close (OPENFT->ft);

	/* get rid of our protocol specific data */
	ft_share_local_cleanup ();

	free_udata (p);
}

/*****************************************************************************/

static void setup_functbl (Protocol *p)
{
	/* inform giFT what kind of hashing algorithm OpenFT uses and how to
	 * express this hash in a human readable form (ASCII) */
	p->hash_set (p, "MD5", openft_md5, openft_md5_human);

	/* communicate special properties of this protocol which will modify
	 * giFT's behaviour
	 * NOTE: most of these dont do anything yet */
	p->support (p, "range-get", TRUE);
	p->support (p, "user-browse", TRUE);
	p->support (p, "hash-unique", TRUE);
	p->support (p, "chat-user", FALSE);
	p->support (p, "chat-group", FALSE);

	/*
	 * Finally, assign the support communication structure.
	 */

	/* ft_openft.c: */
	p->start          = openft_start;
	p->destroy        = openft_destroy;

	/* ft_search.c: */
	p->search         = openft_search;
	p->browse         = openft_browse;
	p->locate         = openft_locate;
	p->search_cancel  = openft_search_cancel;

	/* ft_xfer.c: */
	p->download_start = openft_download_start;
	p->download_stop  = openft_download_stop;
	p->upload_stop    = openft_upload_stop;
	p->upload_avail   = openft_upload_avail;
	p->chunk_suspend  = openft_chunk_suspend;
	p->chunk_resume   = openft_chunk_resume;
	p->source_remove  = openft_source_remove;
	p->source_cmp     = openft_source_cmp;
	p->user_cmp       = openft_user_cmp;

	/* ft_share.c: */
	p->share_new      = openft_share_new;
	p->share_free     = openft_share_free;
	p->share_add      = openft_share_add;
	p->share_remove   = openft_share_remove;
	p->share_sync     = openft_share_sync;
	p->share_hide     = openft_share_hide;
	p->share_show     = openft_share_show;

	/* ft_stats.c: */
	p->stats          = openft_stats;
}

int OpenFT_init (Protocol *p)
{
	/*
	 * Just a sanity check to make sure this protocol was initialized properly.
	 * I don't expect other protocol developers to do this, as bugs in giFT
	 * are not really their problem...
	 */
	assert (p->init == OpenFT_init);

	/*
	 * Attempt to setup OpenFT's protocol-specific data that giFT will keep
	 * track of for us.
	 */
	p->udata = create_udata (p);

	/*
	 * Set this so that OpenFT may track it when the protocol pointer may not
	 * be available.  I think it's pretty safe to assume OpenFT's isn't going
	 * to be implemented using multiple intercommunicating plugins ;)
	 */
	openft = p;

	/*
	 * Setup the giFT callback/function table found in protocol.h so that
	 * giFT understands how to communicate with this plugin.  Please note
	 * that not all features that OpenFT assigns are required.  Please consult
	 * the documentation in protocol.h for information.
	 */
	setup_functbl (p);

	return TRUE;
}

/*****************************************************************************/

/* simple accessor for the openft protocol information */
Protocol *openft_get_proto (void)
{
	return openft;
}
