/*
 * $Id: fst_fasttrack.c,v 1.19 2003/07/14 20:17:44 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"

/*****************************************************************************/

Protocol *fst_proto = NULL;

/*****************************************************************************/

static int fst_plugin_session_callback (FSTSession *session, FSTSessionMsg msg_type, FSTPacket *msg_data);

/*****************************************************************************/

static int fst_plugin_connect_next()
{
	FSTNode *node;

	do
	{
		if (FST_PLUGIN->session)
		{
			/* remove old node from node cache */
			if (FST_PLUGIN->session->node)
				fst_nodecache_remove (FST_PLUGIN->nodecache, FST_PLUGIN->session->node->host);

			/* free old session */
			fst_session_free (FST_PLUGIN->session);
			FST_PLUGIN->session = NULL;
		}

		/* fetch next node */
		node = fst_nodecache_get_front (FST_PLUGIN->nodecache);

		if (node == NULL)
		{
			FST_WARN ("Ran out of nodes. Trying some static hosts");

			fst_nodecache_add (FST_PLUGIN->nodecache, NodeKlassIndex, "fm2.imesh.com", 1214, 0, 0);
			node = fst_nodecache_get_front (FST_PLUGIN->nodecache);
		}

		/* remove new node from cache so restarting can be used to force use of another node */
		fst_nodecache_remove (FST_PLUGIN->nodecache, node->host);

		/* create session */
		FST_PLUGIN->session = fst_session_create (fst_plugin_session_callback);

	} while (!fst_session_connect (FST_PLUGIN->session, node));

	return TRUE;
}

/*****************************************************************************/

static int fst_plugin_session_callback (FSTSession *session, FSTSessionMsg msg_type, FSTPacket *msg_data)
{
	switch (msg_type)
	{
	/* session management messages */
	case SessMsgConnected:
	{
		break;
	}

	case SessMsgEstablished:
	{
		FST_DBG_3 ("supernode connection established to %s:%d, load: %d%%",
				   session->node->host, session->node->port, session->node->load);
		/* resent queries for all running searches */
		/* TODO: greet supernode first! */
/*
*		fst_searchlist_send_queries (FST_PLUGIN->searches, session, TRUE);
*/
		break;
	}

	case SessMsgDisconnected:
	{
		/* zero stats */
		FST_PLUGIN->stats->users = 0;
		FST_PLUGIN->stats->files = 0;
		FST_PLUGIN->stats->size = 0;

		fst_plugin_connect_next ();
		return FALSE;
	}

	/* FastTrack messages */
	case SessMsgNodeList:	/* supernode sent ip list */
	{
		int i;
		time_t now = time (NULL); 

		for(i=0; fst_packet_remaining(msg_data) >= 8; i++)
		{
			unsigned long ip		= fst_packet_get_uint32 (msg_data);			
			unsigned short port		= ntohs (fst_packet_get_uint16 (msg_data));	
			unsigned int last_seen		= fst_packet_get_uint8 (msg_data);			
			unsigned int load		= fst_packet_get_uint8 (msg_data);		

//			FST_DBG_4 ("node: %s:%d   load: %d%% last_seen: %d mins ago", net_ip_str(ip), port, load, last_seen);
			fst_nodecache_add (FST_PLUGIN->nodecache, NodeKlassSuper, net_ip_str (ip), port, load, now - last_seen * 60);
		}
		FST_DBG_1 ("added %d received supernode IPs to nodes list", i);

		/* if we got this from an index node disconnect now and use a supernode */
		if (session->node->klass == NodeKlassIndex)
		{
			FST_DBG ("disconnecting from index node");
			fst_session_disconnect (session);	/* this calls us back with SessMsgDisconnected */
			return FALSE;
		}
		break;
	}

	case SessMsgNetworkStats:	/* network statistics */
	{
		unsigned int mantissa, exponent;

		if (fst_packet_remaining (msg_data) < 12)	/* 97 bytes total now? was 60? */
			break;

		FST_PLUGIN->stats->users = ntohl (fst_packet_get_uint32 (msg_data));	/* number of users */
		FST_PLUGIN->stats->files = ntohl(fst_packet_get_uint32 (msg_data));	/* number of files */

		mantissa = ntohs(fst_packet_get_uint16 (msg_data));	/* mantissa of size */
		exponent = ntohs(fst_packet_get_uint16 (msg_data));	/* exponent of size */

    	if (exponent >= 30)
			FST_PLUGIN->stats->size = mantissa << (exponent - 30);
    	else
			FST_PLUGIN->stats->size = mantissa >> (30 - exponent);

		/* what follows in the packet is the number of files and their size per media type (6 times)
		 * we do not currently care for those
		 * something else with a size of 37 byte follows, dunno what it is */

		FST_DBG_3 ("received network stats: %d users, %d files, %d GB",
				   FST_PLUGIN->stats->users,
				   FST_PLUGIN->stats->files,
				   FST_PLUGIN->stats->size);
		break;
	}

	case SessMsgNetworkName:		/* remote network name */
	{
		FSTPacket *packet;
		char *net_name = STRDUP_N (msg_data->data, fst_packet_size(msg_data));

		FST_DBG_2 ("received network name: \"%s\", sending ours: \"%s\"", net_name ,FST_NETWORK_NAME);
		free (net_name);

		packet = fst_packet_create();
		fst_packet_put_ustr (packet, FST_NETWORK_NAME, strlen(FST_NETWORK_NAME));

		if (fst_session_send_message (session, SessMsgNetworkName, packet) == FALSE)
		{
			fst_packet_free (packet);
			fst_session_disconnect (session);
			return FALSE;
		}

		fst_packet_free(packet);
		break;
	}

	case SessMsgQueryReply:
	case SessMsgQueryEnd:
	{
//		save_bin_data(msg_data->data, fst_packet_remaining(msg_data));
		fst_searchlist_process_reply (FST_PLUGIN->searches, msg_type, msg_data);
		break;
	}

	default:
/*
*		FST_DBG_2 ("unhandled message: type = 0x%02x, length = %d", msg_type, fst_packet_size(msg_data));
*		printf("\nunhandled message: type = 0x%02x, length = %d", msg_type, fst_packet_size(msg_data));
*		print_bin_data(msg_data->data, fst_packet_remaining(msg_data));
*/
		break;
	}

	return TRUE;
}

/*****************************************************************************/

/* allocate and init plugin */
static int gift_cb_start (Protocol *p)
{
	FSTPlugin *plugin = malloc (sizeof (FSTPlugin));
	int i;
	char *nodesfile;
	char *conf_path, *default_conf_path;

	FST_DBG ("starting up");

	/* init config and copy to local config if missing */
	conf_path = gift_conf_path ("FastTrack/FastTrack.conf");

	if (!file_exists (conf_path))
	{
		FST_WARN ("Local config does not exist, copying default config.");

		default_conf_path = stringf ("%s/%s", platform_data_dir(), "FastTrack/FastTrack.conf");

		if (!file_cp (default_conf_path, conf_path))
		{		
			free (plugin);
			FST_ERR ("Unable to copy default fasttrack configuration, exiting plugin.");
			return FALSE;
		}
	}

	if (!(plugin->conf = gift_config_new ("FastTrack")))	/* this only fails on low mem */
	{
		free (plugin);
		FST_ERR ("Unable to open fasttrack configuration, exiting plugin.");
		return FALSE;
	}

	/* set protocol pointer */
	p->udata = (void*)plugin;

	/* cache user name */
	FST_PLUGIN->username = strdup (config_get_str (FST_PLUGIN->conf, "main/alias=giFTed"));

	/* set session to NULL */
	FST_PLUGIN->session = NULL;

	/* init node cache */
	FST_PLUGIN->nodecache = fst_nodecache_create ();

	/* Attempt to open the locally installed nodes file; if this fails we
	 * should try the global cache. */

	nodesfile = gift_conf_path ("FastTrack/nodes");
	i = fst_nodecache_load (plugin->nodecache, nodesfile);

	if (i < 0)
	{
		FST_WARN_1 ("Couldn't find any nodes in local \"%s\". Trying global list", nodesfile);

		nodesfile = stringf ("%s/FastTrack/nodes", platform_data_dir());
		i = fst_nodecache_load (plugin->nodecache, nodesfile);

		if (i < 0)
			FST_WARN_1 ("Couldn't find any nodes in global \"%s\".", nodesfile);
		else
			FST_DBG_2 ("Loaded %d supernode addresses from global nodes file \"%s\"", i, nodesfile);
	}
	else
	{
		FST_DBG_2 ("Loaded %d supernode addresses from local nodes file \"%s\"", i, nodesfile);
	}

	/* init searches */
	FST_PLUGIN->searches = fst_searchlist_create();

	/* init stats */
	FST_PLUGIN->stats = fst_stats_create ();

	/* temporary, until we have a way to find usefull nodes faster */
	FST_DBG ("adding fm2.imesh.com:1214 as temporary index node");
	fst_nodecache_add (FST_PLUGIN->nodecache, NodeKlassIndex, "fm2.imesh.com", 1214, 0, time(NULL));

	/* start first connection */
	fst_plugin_connect_next ();

	return TRUE;
}

/* destroy plugin */
static void gift_cb_destroy (Protocol *p)
{
	char *nodesfile;
	int i;

	FST_DBG ("shutting down");

	if (!FST_PLUGIN)
		return;

	/* free stats */
	fst_stats_free (FST_PLUGIN->stats);

	/* free searches */
	fst_searchlist_free (FST_PLUGIN->searches);

	/* free session */
	fst_session_free (FST_PLUGIN->session);

	/* save and free nodes */
	nodesfile = gift_conf_path ("FastTrack/nodes");
	i = fst_nodecache_save (FST_PLUGIN->nodecache, nodesfile);
	if (i < 0)
		FST_WARN_1 ("couldn't save nodes file \"%s\"", nodesfile);
	else
		FST_DBG_2 ("saved %d supernode addresses to nodes file \"%s\"", i, nodesfile);
	fst_nodecache_free (FST_PLUGIN->nodecache);

	/* free cached user name */
	free (FST_PLUGIN->username);

	/* free config */
	config_free (FST_PLUGIN->conf);

	free (FST_PLUGIN);
}

/*****************************************************************************/

int gift_cb_source_cmp (Protocol *p, Source *a, Source *b)
{
	return strcmp (a->url, b->url);
}

/*****************************************************************************/

static void fst_plugin_setup_functbl (Protocol *p)
{
	/*
	 * communicate special properties of this protocol which will modify
	 * giFT's behaviour
 	 * NOTE: most of these dont do anything yet
	 */

	p->support (p, "range-get", TRUE);
	p->support (p, "hash-unique", TRUE);

	/*
	 * Finally, assign the support communication structure.
	 */

	/* fst_hash.c */
	p->hash_handler (p, (const char*)FST_HASH_NAME, HASH_PRIMARY, (HashFn)gift_cb_FTH, (HashDspFn)gift_cb_FTH_human);

	/* fst_openft.c: */
	p->start          = gift_cb_start;
	p->destroy        = gift_cb_destroy;

	/* fst_search.c: */
	p->search         = gift_cb_search;
	p->browse         = gift_cb_browse;
	p->locate         = gift_cb_locate;
	p->search_cancel  = gift_cb_search_cancel;

	/* fst_xfer.c: */
	p->download_start = gift_cb_download_start;
	p->download_stop  = gift_cb_download_stop;
	p->source_remove  = gift_cb_source_remove;
/*
*	p->upload_stop    = openft_upload_stop;
*	p->upload_avail   = openft_upload_avail;
*	p->chunk_suspend  = openft_chunk_suspend;
*	p->chunk_resume   = openft_chunk_resume;
*/
	p->source_cmp     = gift_cb_source_cmp;
/*	
	p->user_cmp       = openft_user_cmp;
*/	

	/* fst_share.c: */
/*
*	p->share_new      = openft_share_new;
*	p->share_free     = openft_share_free;
*	p->share_add      = openft_share_add;
*	p->share_remove   = openft_share_remove;
*	p->share_sync     = openft_share_sync;
*	p->share_hide     = openft_share_hide;
*	p->share_show     = openft_share_show;
*/
	/* fst_stats.c: */
	p->stats          = gift_cb_stats;
}

int FastTrack_init (Protocol *p)
{
	/* make sure we're loaded with the correct plugin interface version */
	if (protocol_compat (p, LIBGIFTPROTO_MKVERSION (0, 11, 1)) != 0)
		return FALSE;
	
	/* tell giFT about our version
	 * VERSION is defined in config.h. e.g. "0.8.2"
	 */ 
	p->version_str = strdup ("foo");
	
	/* put protocol in global variable so we always have access to it */
	fst_proto = p;

	/* setup giFT callbacks */
	fst_plugin_setup_functbl (p);

	return TRUE;
}

/*****************************************************************************/
