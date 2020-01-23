/*
 * $Id: meta_import.c,v 1.1 2006/09/12 14:50:32 mkern Exp $
 *
 * Copyright (C) 20003 giFT project (gift.sourceforge.net)
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

#include "giftd.h"

#include "plugin/share.h"

#include "lib/file.h"

/*****************************************************************************/

/*
 * This module reads meta data from a config file named ".<media file>.meta"
 * (note the leading dot) and adds all keys it finds under the "meta" section
 * to the share.
 *
 * Sample file:
 *
 * [meta]
 * comment=my great video clip
 * keywords=foo bar fred
 *
 */

/*****************************************************************************/

#if 0
# define PARSE_DEBUG(x) GIFT_TRACE(x)
#else
# define PARSE_DEBUG(x)
#endif

/*****************************************************************************/

typedef struct
{
	Share *share;
	int count;
} AddMetaParams;

static void add_meta_keys (ds_data_t *key, ds_data_t *value, AddMetaParams *p)
{
	share_set_meta (p->share, key->data, value->data);
	p->count++;
	PARSE_DEBUG (("\tAdded %s: '%s'", key->data, value->data));
}

BOOL meta_import_run (Share *share, const char *path)
{
	char *meta_file, *directory;
	Config *conf;
	ConfigHeader *header;
	List *l;
	AddMetaParams p;

#if 0
	GIFT_DBG (("meta_import: %s", path));
#endif

	directory = file_dirname (path);
	meta_file = stringf_dup ("%s/.%s.meta", directory, file_basename (path));
	free (directory);

	if (!(conf = config_new (meta_file)))
	{
		PARSE_DEBUG (("meta_import: Failed to open '%s'", meta_file));
		free (meta_file);
		return FALSE;
	}

	PARSE_DEBUG (("Reading meta tags from '%s'", meta_file));

	/* find 'meta' section */
	for (l = conf->headers; l; l = l->next)
	{
		header = (ConfigHeader*) l->data;
		if (gift_strcasecmp(header->name, "meta") == 0)
			break;
	}

	if (!l)
	{
		PARSE_DEBUG (("No meta section found in '%s'", meta_file));
		free (meta_file);
		return FALSE;
	}

	/* add all keys to share */
	p.share = share;
	p.count = 0;
	dataset_foreach (header->keys, DS_FOREACH(add_meta_keys), &p);

	GIFT_TRACE (("Added %d meta data tags from '%s'", p.count, meta_file));

	config_free (conf);
	free (meta_file);

	return TRUE;
}
