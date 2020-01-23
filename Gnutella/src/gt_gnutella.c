/*
 * $Id: gt_gnutella.c,v 1.74 2006/08/06 16:53:36 hexwab Exp $
 *
 * Copyright (C) 2001-2004 giFT project (gift.sourceforge.net)
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
#include "xml.h"

#include "gt_share.h"
#include "gt_share_state.h"

#include "gt_accept.h"
#include "gt_ban.h"
#include "gt_bind.h"

#include "gt_node.h"
#include "gt_node_list.h"
#include "gt_netorg.h"

#include "gt_xfer_obj.h"
#include "gt_xfer.h"

#include "gt_search.h"
#include "gt_search_exec.h"

#include "gt_web_cache.h"
#include "gt_stats.h"
#include "gt_query_route.h"
#include "transfer/source.h"           /* gnutella_source_{add,remove,cmp} */

/*****************************************************************************/

/* giFT protocol pointer */
Protocol *GT;

/*****************************************************************************/

/* The ip address is local if the address is local and the source from which
 * it was discovered was not local */
BOOL gt_is_local_ip (in_addr_t ip, in_addr_t src)
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

/* shutdown */
static void gnutella_destroy (Protocol *p)
{
	GT->DBGFN (GT, "entered");

	/* cleanup any network maintenance data */
	gt_netorg_cleanup ();

	/* save the node list to disk */
	gt_node_list_save ();

	/* cleanup any information about banned nodes */
	gt_ban_cleanup ();

	/* destroy query_router tables */
	gt_query_router_self_destroy ();

	/* cleanup remote search data structures */
	gt_search_cleanup ();

	/* cleanup local share state */
	gt_share_state_local_cleanup ();

	/* cleanup local search data structures */
	gt_search_exec_cleanup ();

	/* cleanup XML structures */
	gt_xml_cleanup ();

	/* free and disconnect all nodes */
	gt_node_remove_all ();

	/* destroy web cache information */
	gt_web_cache_cleanup ();

	/* stop binding to the local port */
	gt_bind_cleanup ();

	/* free configuration information */
	gt_config_cleanup ();

	/* free this client's GUID */
	gt_guid_self_cleanup ();
}

/*****************************************************************************/

static unsigned char *gnutella_sha1_hash (const char *path, size_t *len)
{
	*len = SHA1_BINSIZE;
	return sha1_digest (path, 0);
}

static char *gnutella_sha1_dsp (unsigned char *hash, size_t len)
{
	return sha1_string (hash);
}

/*****************************************************************************/

static BOOL self_is_too_old (void)
{
	return FALSE;
}

static void too_old_error_msg (void)
{
	GIFT_ERROR (("\nYour version of the Gnutella plugin is more than 1 year\n"
	             "old.  In order to protect the Gnutella network from \n"
	             "older programs, this plugin has deactivated itself.\n\n"
	             "Please update the plugin with a new version from \n"
	             "http://www.giftproject.org/, or stop running the \n"
	             "plugin by runnning gift-setup or removing \"Gnutella\"\n"
	             "from the /main/plugins line in $HOME/.giFT/giftd.conf\n"
	             "manually.\n\n"
	             "Thanks, and sorry for the inconvenience.\n"));
}

static BOOL gnutella_start (Protocol *p)
{
	p->DBGFN (p, "Starting Gnutella plugin");

	/*
	 * If this node is extremely old, deactivate the plugin, but print a
	 * message to the log file.
	 *
	 * This is a temporary hack to clean out old nodes in the future, just
	 * until ultrapeer support is implemented, but we don't want to bother
	 * people that are using other plugins and don't care...
	 */
	if (self_is_too_old ())
	{
		too_old_error_msg ();
		return TRUE;
	}

	if (!gt_config_init ())
	{
		GIFT_ERROR (("Unable to load config file. Please copy it to "
		             "~/.giFT/Gnutella/Gnutella.conf"));
		return FALSE;
	}

	if (!gt_web_cache_init ())
	{
		GIFT_ERROR (("Unable to load gwebcaches file. Please copy it to "
		             "~/.giFT/Gnutella/gwebcaches"));
		return FALSE;
	}

	/* load any banned ip addresses */
	gt_ban_init ();

	/* initialize the GUID for this node */
	gt_guid_self_init ();

	/* listen for connections */
	gt_bind_init ();

	/* load the list of all previously contacted nodes */
	gt_node_list_load ();

	/* initialize query router tables */
	gt_query_router_self_init ();

	/* initialize the local search data structures */
	gt_search_exec_init ();

	/* initialize the local sharing state */
	gt_share_state_local_init ();

	/* initialize the remote search data structures */
	gt_search_init ();

	/* initialize support for xml metadata */
	gt_xml_init ();

	/* startup network maintenance */
	gt_netorg_init ();

	return TRUE;
}

/*
 * The entry-point for the giFT daemon
 */
BOOL Gnutella_init (Protocol *p)
{
	if (protocol_compat (p, LIBGIFTPROTO_MKVERSION (0, 11, 4)) != 0)
		return FALSE;

	p->version_str = STRDUP (GT_VERSION);
	GT = p;

	/* gt_gnutella.c: */
	p->start = gnutella_start;

	/* skip initializing if too old. Note that we still need to provide a
	 * start function since gift's default handler will return FALSE. The
	 * actual error message describing the 'too old' problem is also generated
	 * by our start function.
	 */
	if (self_is_too_old ())
		return TRUE;

	/* describe the hash algo */
	p->hash_handler (p, "SHA1", HASH_PRIMARY, gnutella_sha1_hash,
	                 gnutella_sha1_dsp);

	/* gt_gnutella.c: */
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

	/* transfer/source.c: */
	p->source_cmp     = gnutella_source_cmp;
	p->source_add     = gnutella_source_add;
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

	/* gt_share_state.c: */
	p->share_hide     = gnutella_share_hide;
	p->share_show     = gnutella_share_show;

	/* gt_stats.c: */
	p->stats          = gnutella_stats;

	return TRUE;
}
