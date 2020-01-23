/*
 * $Id: ar_misc.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "aresdll.h"
#include "as_ares.h"
#include "ar_callback.h"

/*****************************************************************************/

/* ASNetInfo stat callback which forwards state to user application */
void ar_stats_callback (ASNetInfo *info)
{
	ARStatusData data;

	data.connected  = (info->conn_have > 0);
	data.connecting = (info->conn_want > info->conn_have);
	data.users      = info->users;
	data.files      = info->files;
	data.size       = info->size;

	ar_raise_callback (AR_CB_STATUS, &data, NULL);
}

/*****************************************************************************/

/* Set meta data array from lib object */
void ar_export_meta (ARMetaTag meta[AR_MAX_META_TAGS], ASMeta *libmeta)
{
	List *l;
	int i;

	l = libmeta ? libmeta->tags : NULL;
	for (i = 0; i < AR_MAX_META_TAGS; i++)
	{
		if (l)
		{
			meta[i].name = ((ASMetaTag *)l->data)->name;
			meta[i].value = ((ASMetaTag *)l->data)->value;
			l = l->next;
		}
		else
		{
			meta[i].name = NULL;
			meta[i].value = NULL;
		}
	}
}

/* Create meta data object from external array */
ASMeta *ar_import_meta (ARMetaTag meta[AR_MAX_META_TAGS])
{
	ASMeta *libmeta;
	int i;

	if (!(libmeta = as_meta_create ()))
		return NULL;

	for (i = 0; i < AR_MAX_META_TAGS && meta[i].name; i++)
		as_meta_add_tag (libmeta, meta[i].name, meta[i].value);

	return libmeta;
}

/*****************************************************************************/
