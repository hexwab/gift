/*
 * $Id: meta.c,v 1.18 2003/02/10 12:57:38 jasta Exp $
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

#include "gift.h"

#include "share_file.h"
#include "mime.h"
#include "file.h"

#include "meta.h"
#include "meta_mp3.h"
#include "meta_ogg.h"
#include "meta_image.h"
#include "meta_avi.h"

#include <ctype.h>

/*****************************************************************************/

void meta_set (FileShare *file, char *key, char *value)
{
	Dataset **ds;
	char     *key_low;

	if (!file || !key || !(key_low = string_lower (STRDUP (key))))
		return;

	ds = &SHARE_DATA(file)->meta;

	/* remove any data already present here */
	dataset_removestr (*ds, key_low);

	if (value)
		dataset_insertstr (ds, key_low, value);

	free (key_low);
}

void meta_clear (FileShare *file)
{
	if (!file || !SHARE_DATA(file))
		return;

	dataset_clear (SHARE_DATA(file)->meta);
	SHARE_DATA(file)->meta = NULL;
}

char *meta_lookup (FileShare *file, char *key)
{
	if (!file || !key)
		return NULL;

	return dataset_lookup (SHARE_DATA(file)->meta, key, STRLEN_0 (key));
}

void meta_foreach (FileShare *file, DatasetForeach func, void *udata)
{
	dataset_foreach (SHARE_DATA(file)->meta, func, udata);
}

/*****************************************************************************/

/* fills the supplied structure with as much meta data as can be found */
int meta_run (FileShare *file)
{
	char *host_path;

	if (!file)
		return FALSE;

	/* if file->mime already exists it means that we loaded it from the cache,
	 * avoid recalculating it when not necessary */
	if (!file->mime)
	{
		/* HEx: you do not need to attach into mime.c at all anymore.  Figure
		 * the mime type out right here any way that you want. */
		if (!(file->mime = mime_type (SHARE_DATA(file)->path)))
			return FALSE;
	}

	if (!(host_path = file_host_path (SHARE_DATA(file)->path)))
		return FALSE;

	if (!strcmp (file->mime, "audio/mpeg"))
		meta_mp3_run (file, host_path);
	else if (!strcmp (file->mime, "audio/x-vorbis"))
		meta_ogg_run (file, host_path);
	else if (!strncmp (file->mime, "image", 5))
		meta_image_run (file, host_path);
	else if (!strcmp (file->mime, "video/x-msvideo"))
		meta_avi_run (file, host_path);

	free (host_path);

	return TRUE;
}
