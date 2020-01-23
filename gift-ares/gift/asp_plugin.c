/*
 * $Id: asp_plugin.c,v 1.9 2004/12/24 12:06:25 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#include "asp_plugin.h"

/*****************************************************************************/

/* Set to actual proto struct in Ares_init */
Protocol *gift_proto = NULL;
Config  *gift_config = NULL;

/*****************************************************************************/

static int asp_giftcb_stats (Protocol *p, unsigned long *users,
                             unsigned long *files, double *size,
                             Dataset **extra)
{
	*users = AS->netinfo->users;
	*files = AS->netinfo->files;
	*size  = AS->netinfo->size;

	return AS->netinfo->conn_have;
}

static int asp_giftcb_source_cmp (Protocol *p, Source *a, Source *b)
{
	ASSource *sa, *sb;
	int ret;

	if (!(sa = as_source_unserialize (a->url)))
	{
		AS_ERR_1 ("Invalid source url '%s'", a->url);
		return -1;
	}

	if (!(sb = as_source_unserialize (b->url)))
	{
		AS_ERR_1 ("Invalid source url '%s'", b->url);
		as_source_free (sa);
		return -1;
	}

	/* !as_source_equal implies strcmp != 0 so this is safe. */
	if (as_source_equal (sa, sb))
		ret = 0;
	else
		ret = strcmp (a->url, b->url);

	as_source_free (sa);
	as_source_free (sb);

	return ret;
}

static int asp_giftcb_user_cmp (Protocol *p, const char *a, const char *b)
{
	const char *aa = strrchr (a, '@'), *bb = strrchr (b, '@');

	/* Only compare the user's ip after the '@' to prevent upload queue
	 * circumvention by spoofing user name.
	 */
	return strcmp ((aa ? aa+1 : a), (bb ? bb+1 : b));
}

static int asp_giftcb_chunk_suspend (Protocol *p, Transfer *transfer,
                                     Chunk *chunk, Source *source)
{
	as_bool ret;

	if (transfer_direction (transfer) == TRANSFER_UPLOAD)
	{
		assert (chunk->udata);
		ret = as_upload_suspend (chunk->udata);
		assert (ret);
	}
	else
	{
		assert (source->udata);
		ret = as_downconn_suspend (source->udata);
		assert (ret);
	}
	
	return TRUE;
}

static int asp_giftcb_chunk_resume (Protocol *p, Transfer *transfer,
                                    Chunk *chunk, Source *source)
{
	as_bool ret;

	if (transfer_direction (transfer) == TRANSFER_UPLOAD)
	{
		assert (chunk->udata);
		ret = as_upload_resume (chunk->udata);
		assert (ret);
	}
	else
	{
		assert (source->udata);
		ret = as_downconn_resume (source->udata);
		assert (ret);
	}

	return TRUE;
}

/*****************************************************************************/

static int copy_default_file (const char *srcfile, const char *dstfile)
{
	char *local_path, *default_path, *target_dir;

	target_dir = stringf_dup ("%s/Ares", platform_local_dir());
	local_path = stringf_dup ("%s/Ares/%s", platform_local_dir(), dstfile);
	default_path = stringf_dup ("%s/Ares/%s", platform_data_dir(), srcfile);

	if (!file_exists (local_path))
	{
		AS_WARN_2 ("Local file \"%s\" does not exist, copying default from \"%s\"",
		           local_path, default_path);

		/* make sure the target directory exists */
		if (!file_mkdir (target_dir, 0777))
		{
			AS_ERR_1 ("Unable to create directory \"%s\"", target_dir);

			free (target_dir);
			free (local_path);
			free (default_path);
			return FALSE;
		}


		if (!file_cp (default_path, local_path))
		{		
			AS_ERR_1 ("Unable to copy default file \"%s\"", default_path);

			free (target_dir);
			free (local_path);
			free (default_path);
			return FALSE;
		}
	}

	free (target_dir);
	free (local_path);
	free (default_path);
	return TRUE;
}

/*****************************************************************************/

static BOOL config_refresh (Config *conf)
{
	int i;

	for (i = 0; i < AS_CONF_VAL_ID_MAX - 1; i++)
	{
		char *name;
		if ((name = (char *)as_config_get_name (AS->config, i)))
		{
			char *val = (char *)config_get_str (conf, name);

			if (val)
			{
				switch (as_config_get_type (AS->config, i))
				{
				case AS_CONF_STR:
					as_config_set_str (AS->config, i, val);
					break;
				case AS_CONF_INT:
					as_config_set_int (AS->config, i,
					                   config_get_int (conf, name));
					break;
				default:
					assert (0);
				}
			}
		}
	}

	return TRUE; /* reset */
}

static timer_id conf_timer = INVALID_TIMER;

static void config_init (void)
{
	if (!(gift_config = gift_config_new ("Ares")))
	{
		AS_WARN ("Couldn't open config file. Using hard coded defaults.");
		return;
	}

	config_refresh (gift_config);

	conf_timer = timer_add (2 * MINUTES, (TimerCallback)config_refresh,
	                        gift_config);
}

static void config_destroy (void)
{
	timer_remove_zero (&conf_timer);

	config_free (gift_config);
}

/*****************************************************************************/

/* Initialize everyything and start network connections. */
static int asp_giftcb_start (Protocol *proto)
{
	char *path;
	int sessions;

	AS_DBG ("Starting up.");

	if (!as_init ())
	{
		AS_ERR ("Failed to initialize ares library.");
		return FALSE;
	}

	/* Copy default config file if it is missing and load config. */
	copy_default_file ("Ares.conf.template", "Ares.conf");
	config_init ();

	/* Load nodes file and copy default if necessary. */
	copy_default_file ("nodes", "nodes");
	path = gift_conf_path ("Ares/nodes");

	if (!as_nodeman_load (AS->nodeman, path))
	{
		AS_WARN_1 ("Couldn't load nodes file '%s'. Fix that!", path);
	}

	/* Now start the library. Oddly enough this doesn't do anything at the
	 * moment?
	 */
	if (!as_start (AS))
	{
		AS_ERR ("Failed to start ares library.");
		as_cleanup (AS);
		return FALSE;
	}

	/* Setup some library callbacks we need. */
	asp_upload_register_callbacks ();

	/* Initialize the evil hash map. */
	asp_hashmap_init ();
	
	/* And now start the connections. */
	if ((sessions = config_get_int (gift_config, "main/sessions=4")) >
	    ASP_MAX_SESSIONS)
	{
		AS_WARN_2 ("Requested number of sessions (%d) above hard limit. Reducing to %d.",
		           sessions, ASP_MAX_SESSIONS);
		//		sessions = ASP_MAX_SESSIONS;
	}

	as_sessman_connect (AS->sessman, sessions);

	return TRUE;
}

/* Shut everything down. */
static void asp_giftcb_destroy (Protocol *proto)
{
	AS_DBG ("Shutting down.");

	/* Save nodes file. */
	if (!as_nodeman_save (AS->nodeman, gift_conf_path ("Ares/nodes")))
	{
		AS_WARN_1 ("Failed to save nodes file to '%s'", 
		           gift_conf_path ("Ares/nodes"));
	}

	config_destroy ();

	/* Destroy the evil hash map. */
	asp_hashmap_destroy ();

	/* Stop library. */
	if (!as_stop (AS))
	{
		AS_WARN ("Couldn't stop ares library");
	}

	if (!as_cleanup (AS))
	{
		AS_WARN ("Couldn't cleanup ares library");
	}
}

/*****************************************************************************/

int Ares_init (Protocol *p)
{
	/* Make sure we're loaded with the correct plugin interface version. */
	if (protocol_compat (p, LIBGIFTPROTO_MKVERSION (0, 11, 8)) != 0)
	{
		AS_ERR ("libgift version mismatch. Need at least version 0.11.8.");
		return FALSE;
	}
	
	/* Tell giFT about our version.
	 * VERSION is defined in config.h. e.g. "0.1.1".
	 */ 
	p->version_str = strdup (VERSION);
	
	/* Put protocol in global variable so we always have access to it. */
	gift_proto = p;

	/* Tell giFT about all the cool things we can do. */
	p->support (p, "range-get", TRUE);
	p->support (p, "hash-unique", TRUE);

	/* Register our hash handling callbacks. */
	p->hash_handler (p, "SHA1", HASH_PRIMARY,
	                 (HashFn)asp_giftcb_hash,
	                 (HashDspFn)asp_giftcb_hash_encode);

	/* Now setup all other giFT callbacks. */

	/* asp_plugin.c */
	p->start          = asp_giftcb_start;
	p->destroy        = asp_giftcb_destroy;
	p->stats          = asp_giftcb_stats;
	p->source_cmp     = asp_giftcb_source_cmp;
	p->user_cmp       = asp_giftcb_user_cmp;
	p->chunk_suspend  = asp_giftcb_chunk_suspend;
	p->chunk_resume   = asp_giftcb_chunk_resume;

	/* asp_search.c */
	p->search         = asp_giftcb_search;
	p->browse         = asp_giftcb_browse;
	p->locate         = asp_giftcb_locate;
	p->search_cancel  = asp_giftcb_search_cancel;

	/* asp_download.c */
	p->download_start = asp_giftcb_download_start;
	p->download_stop  = asp_giftcb_download_stop;
	p->source_add     = asp_giftcb_source_add;
	p->source_remove  = asp_giftcb_source_remove;

	/* asp_upload.c */
	p->upload_stop    = asp_giftcb_upload_stop;

	/* asp_share.c */
	p->share_new      = asp_giftcb_share_new;
	p->share_free     = asp_giftcb_share_free;
	p->share_add      = asp_giftcb_share_add;
	p->share_remove   = asp_giftcb_share_remove;
	p->share_sync     = asp_giftcb_share_sync;
	p->share_hide     = asp_giftcb_share_hide;
	p->share_show     = asp_giftcb_share_show;

	return TRUE;
}

/*****************************************************************************/
