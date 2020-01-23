/*
 * $Id: ft_openft.c,v 1.63 2005/01/25 04:47:18 hexwab Exp $
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

#define __FT_OPENFT_C
#include "ft_openft.h"

#include <libgift/file.h>
#include <libgift/parse.h>

#include "ft_netorg.h"
#include "ft_conn.h"
#include "ft_node_cache.h"
#include "ft_search.h"
#include "ft_search_db.h"
#include "ft_routing.h"

#include "ft_transfer.h"
#include "ft_http_server.h"
#include "ft_http.h"

#include "md5.h"

/*****************************************************************************/

/* sigh, declare our globals */
Protocol *FT = NULL;
struct openft_data *openft = NULL;

/*****************************************************************************/

static struct openft_data *alloc_udata (Protocol *p)
{
	struct openft_data *udata;

	if (!(udata = MALLOC (sizeof (struct openft_data))))
		return NULL;

	return udata;
}

static void free_udata (struct openft_data *openft)
{
	free (openft);
}

/*****************************************************************************/

static TCPC *openft_bind (in_port_t port)
{
	TCPC *bindc;

	/*
	 * If port is configured as 0, openft is expected to treat this as a
	 * firewalled node, that is one which cannot directly accept connections
	 * on the core openft (or http) ports.  We will still create a local node
	 * structure (FT_SELF), but we will not attempt to associate the the
	 * actual listening port (default 1215) so that it does not report this
	 * to other nodes.
	 */
	if (!(bindc = tcp_bind (((port) ? (port) : (1215)), FALSE)))
		return NULL;

	/* begin accepting incoming connections on this port */
	input_add (bindc->fd, bindc, INPUT_READ,
	           (InputCallback)ft_session_incoming, FALSE);

	return bindc;
}

static TCPC *http_bind (in_port_t port)
{
	TCPC *bindc;

	if (!(bindc = tcp_bind (port, FALSE)))
		return NULL;

	input_add (bindc->fd, bindc, INPUT_READ,
	           (InputCallback)ft_http_server_incoming, FALSE);

	return bindc;
}

/*****************************************************************************/

static unsigned char *openft_md5 (const char *path, size_t *len)
{
	*len = 16;
	return md5_digest (path, 0);
}

/*****************************************************************************/

/*
 * Often times it is necessary to isolate certain key systems in this module
 * for specialized testing and/or benchmarking.  This is accomplished by
 * defining OPENFT_TEST_SUITE to the necessary test module name.  For
 * example, to benchmark the ft_search_db.c code you would add the following
 * to CFLAGS:
 *
 *  -DOPENFT_TEST_SUITE=test_suite_search_db
 *
 * Please note that you must know the symbol name you wish to call ahead of
 * time before you can take advantage of this feature.
 */
#ifdef OPENFT_TEST_SUITE

static BOOL handle_test_suite (Protocol *p)
{
	BOOL ret = OPENFT_TEST_SUITE(p);

	FT->DBGFN (FT, "test concluded with ret=%d", ret);

	exit ((ret) ? (EXIT_SUCCESS) : (EXIT_FAILURE));

	return ret;
}

static BOOL openft_start (Protocol *p)
{
	return handle_test_suite (p);
}

#else /* !OPENFT_TEST_SUITE */

static BOOL clamp_openft_params (Protocol *p, struct openft_data *openft)
{
	assert (openft != NULL);

	/* make sure the user didn't supply a value we're not expecting */
	openft->ninfo.klass &= FT_NODE_CLASSPRI_MASK;
	openft->ninfo.klass |= FT_NODE_USER;

	/* firewalled users can not go beyond the user class */
	if (openft->ninfo.port_openft == 0)
	{
		if (openft->ninfo.klass != FT_NODE_USER)
		{
			p->err (p,
			        "Current connection configuration does not allow "
			        "extended class setups.  Please rethink your class "
			        "choice.");

			return FALSE;
		}

		/* lets not make a big fuss out of this setting */
		openft->klass_alw &= ~FT_NODE_SEARCH;
	}

#ifndef USE_LIBDB
	/*
	 * Search status requires libdb support built in at compile time.  If
	 * they have selected this class without support, we will fail here in
	 * order to ensure that the user sees this error message.  Hopefully it
	 * will convince them to rebuild with libdb support.
	 */
	if (openft->ninfo.klass & FT_NODE_SEARCH)
	{
		p->err (p,
		        "Configured as a search node, but this build has no "
		        "Berkeley Database support present!");

		return FALSE;
	}

	openft->klass_alw &= ~FT_NODE_SEARCH;
#endif /* !USE_LIBDB */

#ifdef CHECK_FOR_EVIL_BEHAVIOUR
	if (strcmp (eyes, "shifty") == 0)
	{
		p->err (p, "I don't like the way you're looking at me.");
		return FALSE;
	}
#endif /* CHECK_FOR_EVIL_BEHAVIOUR */

	return TRUE;
}

static BOOL init_openft_obj (Protocol *p, struct openft_data *openft)
{
	assert (openft != NULL);

	/*
	 * Access the OpenFT configuration module.  Please note that in the
	 * next major release of libgift and libgiftproto, this will be abstracted
	 * into giFT space to provide a more structured and flexible interface.
	 */
	if (!(openft->cfg = gift_config_new ("OpenFT")))
	{
		p->err (p, "Unable to load OpenFT configuration: %s", GIFT_STRERROR());
		return FALSE;
	}

	/* gather basic information about our node */
	openft->ninfo.klass       = (ft_class_t)FT_CFG_NODE_CLASS;
	openft->ninfo.alias       =      STRDUP(FT_CFG_NODE_ALIAS);
	openft->ninfo.port_openft =  (in_port_t)FT_CFG_NODE_PORT;
	openft->ninfo.port_http   =  (in_port_t)FT_CFG_NODE_HTTP_PORT;
	openft->klass_alw         = (ft_class_t)FT_CFG_NODE_CLASS_ALLOW;

	/* hmm, maybe ninfo.indirect is a bad idea? */
	openft->ninfo.indirect = BOOL_EXPR (openft->ninfo.port_openft == 0);

	/* make sure that the parameters supplied above dont conflict in some way */
	if (!(clamp_openft_params (p, openft)))
		return FALSE;

	/* bind the OpenFT port and begin accepting connections */
	if (!(openft->bind_openft = openft_bind (openft->ninfo.port_openft)))
	{
		p->err (p, "Unable to successfully bind the OpenFT port, aborting...");
		return FALSE;
	}

	/* bind to the HTTP service OpenFT uses for uploading files */
	if (!(openft->bind_http = http_bind (openft->ninfo.port_http)))
	{
		p->err (p, "Unable to successfully bind the OpenFT HTTP port, aborting...");
		return FALSE;
	}

	/* setup the main connection maintenance timer, which will make sure
	 * we continue to exist on the network when things get stale */
	openft->cmaintain_timer =
	    timer_add (FT_CFG_MAINT_TIMER, (TimerCallback)ft_conn_maintain, NULL);
	assert (openft->cmaintain_timer != 0);

	return TRUE;
}

static BOOL openft_start (Protocol *p)
{
	BOOL ret;

	/* i wrote giFT, and therefore i do not trust it */
	assert (openft == p->udata);
	assert (openft != NULL);

	/*
	 * Announce our presence to the world.  This is a test of the new logging
	 * facilities really.
	 */
	p->DBGFN (p, "Booya! %s in the house!", p->name);

	/* initialize the "global" OpenFT object, which includes opening the
	 * local configuration and binding listening ports */
	if (!(ret = init_openft_obj (p, openft)))
		return ret;

	/* initialize the database environment for search nodes */
	if (openft->ninfo.klass & FT_NODE_SEARCH)
	{
		if (!(ret = ft_routing_init ()))
			return ret;

		if (!(ret = ft_search_db_init (FT_CFG_SEARCH_ENV_PATH, FT_CFG_SEARCH_ENV_CACHE)))
			return ret;
	}

	/* OpenFT, here we come :) */
	return ft_conn_initial ();
}

#endif /* OPENFT_TEST_SUITE */

static BOOL cleanup_conn (FTNode *node, void *data)
{
	if (node->session)
		ft_session_stop (FT_CONN(node));

	return TRUE;
}

static void openft_destroy (Protocol *p)
{
	int nodes;

	assert (p != NULL);
	assert (p->udata == openft);
	assert (p->udata != NULL);

	/* boolean used by node_disconnect to disable certain behavior */
	openft->shutdown = TRUE;

	/* remove the timer first to eliminate a race condition when
	 * ft_netorg_clear is running */
	timer_remove (openft->cmaintain_timer);

	/* write nodes cache */
	nodes = ft_node_cache_update ();
	p->DBGFN (p, "flushed %d nodes", nodes);

	/* disconnect everyone for cleanliness */
	ft_netorg_clear (FT_NETORG_FOREACH(cleanup_conn), NULL);

	ft_search_db_destroy ();

	ft_routing_free ();

	/* cleanup */
	config_free (openft->cfg);
	tcp_close (openft->bind_openft);
	tcp_close (openft->bind_http);

	/* cleanup our protocol-specific "globals" */
	free_udata (openft);
	openft = NULL;
}

/*****************************************************************************/

static void setup_functbl (Protocol *p)
{
	/* inform giFT what kind of hashing algorithm OpenFT uses */
	p->hash_handler (p, "MD5", HASH_PRIMARY, openft_md5, NULL);

	/*
	 * Communicate special properties of this protocol which will modify
	 * giFT's behaviour.
	 *
	 * NOTE: Most of these have no effect just yet, but are merely provided
	 * here as an example of what could be :)
	 */
#if 0
	p->support (p, "static-chunks", TRUE);
#endif
	p->support (p, "range-get",     TRUE);
	p->support (p, "user-browse",   TRUE);
	p->support (p, "hash-unique",   TRUE);
	p->support (p, "chat-user",     FALSE);
	p->support (p, "chat-group",    FALSE);

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
	p->source_add     = openft_source_add;
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

BOOL OpenFT_init (Protocol *p)
{
	/*
	 * Check that this plugin is linked against a compatible runtime version.
	 */
	if (protocol_compat (p, LIBGIFTPROTO_MKVERSION (0, 11, 4)) != 0)
		return FALSE;

#if 0
	/*
	 * Just a sanity check to make sure this protocol was initialized properly.
	 * I don't expect other protocol developers to do this, as bugs in giFT
	 * are not really their problem...
	 */
	assert (p->init == OpenFT_init);
#endif

	/*
	 * Inform giFT of our protocol version.
	 */
	p->version_str = stringf_dup ("%i.%i.%i.%i",
	                              OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO,
	                              OPENFT_REV);

	/*
	 * Attempt to setup OpenFT's protocol-specific data that giFT will keep
	 * track of for us.
	 */
	openft = alloc_udata (p);
	assert (openft != NULL);

	p->udata = openft;

	/*
	 * Setup the giFT callback/function table found in protocol.h so that
	 * giFT understands how to communicate with this plugin.  Please note
	 * that not all features that OpenFT assigns are required.  Please consult
	 * the documentation in protocol.h for information.
	 */
	setup_functbl (p);

	/*
	 * Set this so that OpenFT may track it when the protocol pointer may not
	 * be available.  I think it's pretty safe to assume OpenFT's isn't going
	 * to be implemented using multiple intercommunicating plugins ;)
	 */
	FT = p;

	/*
	 * Yes, initialization succeeded.
	 */
	return TRUE;
}
