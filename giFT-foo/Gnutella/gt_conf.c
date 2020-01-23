/*
 * $Id: gt_conf.c,v 1.1 2003/06/07 05:24:14 hipnod Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

/*****************************************************************************/

#define CHECK_CONFIG_INTERVAL       (1 * MINUTES)

/*****************************************************************************/

static Config     *gt_conf;

static char       *conf_path;
static time_t      conf_mtime;

static Dataset    *cache;

static timer_id    refresh_timer;

/*****************************************************************************/

static char *get_key (char *key_str)
{
	char *str;
	char *key;

	if (!(str = STRDUP (key_str)))
		return NULL;

	key = string_sep (&str, "=");
	return key;
}

static char *cache_lookup (char *key_str)
{
	char *key;
	char *data;

	if (!(key = get_key (key_str)))
		return NULL;

	data = dataset_lookupstr (cache, key);
	free (key);

	return data;
}

static void cache_insert (char *key_str, char *value)
{
	char *key;

	if (!(key = get_key (key_str)))
		return;

	dataset_insertstr (&cache, key, value);
	free (key);
}

char *gt_config_get_str (char *key)
{
	char *str;
	char *ret;

	if (!(str = cache_lookup (key)))
		str = config_get_str (gt_conf, key);

	ret = str;

	/* unset keys are marked specially by the empty string so
	 * we don't have to call config_get_xxx for them */
	if (string_isempty (str))
	{
		ret = NULL;
		str = "";
	}

	/* Hrm, the dataset doesn't handle inserting the same item you lookup
	 * yet, so we can't do the insert unconditionally here.. */
	if (str != cache_lookup (key))
		cache_insert (key, str);

	return ret;
}

int gt_config_get_int (char *key)
{
	return ATOI (gt_config_get_str (key));
}

static int refresh_conf (void *udata)
{
	/* check the mtime on the conf file. If it has changed, reload */
	struct stat  conf_st;
	char        *path;
	BOOL         ret;

	path = STRDUP (gift_conf_path (conf_path));
	ret = file_stat (path, &conf_st);

	if (!ret || conf_st.st_mtime != conf_mtime)
	{
		GT->DBGFN (GT, "Gnutella.conf changed on disk. flushing cached config");

		/* gt_config_get_xxx will reload the cache */
		dataset_clear (cache);
		cache = NULL;

		conf_mtime = conf_st.st_mtime;
	}

	free (path);
	return TRUE;
}

BOOL gt_config_load_file (char *relative_path, BOOL update, BOOL force)
{
	char        *src_path;
	char        *dst_path;
	BOOL         src_exists;
	BOOL         dst_exists;
	struct stat  src_st;
	struct stat  dst_st;
	BOOL         ret        = TRUE;

	src_path = STRDUP (stringf ("%s/%s", platform_data_dir(), relative_path));
	dst_path = STRDUP (gift_conf_path (relative_path));

	src_exists = file_stat (src_path, &src_st);
	dst_exists = file_stat (dst_path, &dst_st);

	/* NOTE: the user may modify the config files in ~/.giFT/Gnutella/.
	 * If so, the mtime there should be greater, so look for an mtime
	 * greater than instead of not equal to the those files. */
	if (force || (src_exists && 
	              (!dst_exists || src_st.st_mtime > dst_st.st_mtime)))
	{
		/* Copy the default configuration from the data dir
		 * (usually "/usr/local/share/giFT/Gnutella/") */
		GT->DBGFN (GT, "reloading configuration for %s (copying %s -> %s)", 
		           relative_path, src_path, dst_path);
		ret = file_cp (src_path, dst_path);
	}

	free (dst_path);
	free (src_path);

	return ret;
}

static Config *load_config (char *relative_path)
{
	Config  *conf;
	char    *full_path;

	full_path = STRDUP (gift_conf_path (relative_path));

	if (!(conf = config_new (full_path)))
	{
		/* copy the configuration from the data dir */
		gt_config_load_file (relative_path, TRUE, TRUE);

		/* retry loading the configuration */
		conf = config_new (full_path);
	}

	return conf;
}

/*****************************************************************************/

BOOL gt_config_init (void)
{
	struct stat st;

	refresh_timer = timer_add (CHECK_CONFIG_INTERVAL, 
	                           (TimerCallback)refresh_conf, NULL);

	conf_path = STRDUP (stringf ("%s/%s.conf", GT->name, GT->name));

	if (file_stat (gift_conf_path (conf_path), &st))
	    conf_mtime = st.st_mtime;

	gt_conf = load_config (conf_path);
	cache = dataset_new (DATASET_HASH);

	if (!refresh_timer || !conf_path || !gt_conf)
		return FALSE;

	return TRUE;
}

void gt_config_cleanup (void)
{
	dataset_clear (cache);
	cache = NULL;

	config_free (gt_conf);
	gt_conf = NULL;

	free (conf_path);
	conf_path = NULL;

	timer_remove_zero (&refresh_timer);
}
