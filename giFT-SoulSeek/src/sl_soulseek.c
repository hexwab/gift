/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#include <signal.h>
#include <stdio.h>
#include <sys/times.h>

#include "sl_soulseek.h"

/*****************************************************************************/

Protocol *sl_proto = NULL;

/*****************************************************************************/

// alloc and init plugin
static int sl_gift_cb_start(Protocol *p)
{
	// initialize configuration
	if(!(SL_PLUGIN->conf = gift_config_new("SoulSeek")))
	{
		sl_proto->err(sl_proto, "Can't load SoulSeek configuration!");
		return FALSE;
	}

	// initialize session
	if(!(SL_PLUGIN->session = sl_session_create()))
	{
		sl_proto->err(sl_proto, "Can't create SoulSeek session!");
		return FALSE;
	}

	// initialize statistics
	if(!(SL_PLUGIN->stats = sl_stats_create()))
	{
		sl_proto->err(sl_proto, "Can't initialize SoulSeek stats!");
		return FALSE;
	}

	// initialize searches
	if(!(SL_PLUGIN->searches = sl_search_create()))
	{
		sl_proto->err(sl_proto, "Can't initialize SoulSeek searches!");
		return FALSE;
	}

	// initialize peer list
	if(!(SL_PLUGIN->peerlist = sl_peer_list_create()))
	{
		sl_proto->err(sl_proto, "Can't initialize Peer list!");
		return FALSE;
	}

	// set server settings
	SL_PLUGIN->server = config_get_str(SL_PLUGIN->conf, "server/ip=" SL_SERVER_ADDR);
	SL_PLUGIN->port = config_get_int(SL_PLUGIN->conf, "server/port=" SL_SERVER_PORT);

	SL_PLUGIN->username = sl_string_create_with_contents(config_get_str(SL_PLUGIN->conf, "main/alias"));

	// connect
	sl_session_connect(SL_PLUGIN->session);

	return TRUE;
}

// destroy plugin
static void sl_gift_cb_destroy(Protocol *p)
{
	config_free(SL_PLUGIN->conf);
	sl_session_disconnect(SL_PLUGIN->session);
	sl_session_destroy(SL_PLUGIN->session);
	sl_stats_destroy(SL_PLUGIN->stats);
	sl_search_destroy(SL_PLUGIN->searches);
	sl_peer_list_destroy(SL_PLUGIN->peerlist);
	sl_string_destroy(SL_PLUGIN->username);
	SL_PLUGIN->session = NULL;
}

/*****************************************************************************/

static void sl_plugin_setup_functbl (Protocol *p)
{
	// communicate special properties of this protocol which will modify
	// giFT's behaviour
	p->support(p, "static-chunks", FALSE);
	p->support(p, "range-get",     TRUE);
	p->support(p, "user-browse",   TRUE);
	p->support(p, "hash-unique",   FALSE);
	p->support(p, "chat-user",     FALSE); // TODO: should be true when actually supported
	p->support(p, "chat-group",    FALSE); // TODO: should be true when actually supported

	// Finally, assign the support communication structure.

	// sl_soulseek.c:
	p->start          = sl_gift_cb_start;
	p->destroy        = sl_gift_cb_destroy;

	// sl_search.c:
	p->search         = sl_gift_cb_search;
	p->browse         = sl_gift_cb_browse;
	p->locate         = sl_gift_cb_locate;
	p->search_cancel  = sl_gift_cb_search_cancel;

	// sl_download.c:
	p->download_start = sl_gift_cb_download_start;
	p->download_stop  = sl_gift_cb_download_stop;
	p->source_remove  = sl_gift_cb_source_remove;

	// sl_upload.c:
/*
	p->upload_stop    = sl_gift_cb_upload_stop;
	p->upload_avail   = sl_gift_cb_upload_avail;
	p->chunk_suspend  = sl_gift_cb_chunk_suspend;
	p->chunk_resume   = sl_gift_cb_chunk_resume;
	p->source_cmp     = sl_gift_cb_source_cmp;
	p->user_cmp       = sl_gift_cb_user_cmp;
*/
	// sl_share.c:
	p->share_new      = sl_gift_cb_share_new;
	p->share_free     = sl_gift_cb_share_free;
	p->share_add      = sl_gift_cb_share_add;
	p->share_remove   = sl_gift_cb_share_remove;
	p->share_sync     = sl_gift_cb_share_sync;
	p->share_hide     = sl_gift_cb_share_hide;
	p->share_show     = sl_gift_cb_share_show;

	// sl_stats.c:
	p->stats          = sl_gift_cb_stats;
}

int SoulSeek_init(Protocol *p)
{
	SLPlugin *plugin;

	// initialize random number generator
	struct tms _tms;
	srand(times(&_tms));

	// make sure we're loaded with the correct plugin interface version
	if(protocol_compat(p, LIBGIFTPROTO_MKVERSION(0, 11, 4)))
		return FALSE;

	plugin = MALLOC(sizeof(SLPlugin));

	// put protocol in global variable so we always have access to it
	sl_proto = p;

	memset(plugin, 0, sizeof(SLPlugin));

	p->udata = plugin;
	p->version_str = strdup(VERSION);

	// setup giFT callbacks
	sl_plugin_setup_functbl(p);

	return TRUE;
}

/*****************************************************************************/

