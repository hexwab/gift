/*
 * $Id: meta.c,v 1.21 2003/05/26 11:47:41 jasta Exp $
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

#include "giftd.h"

#include "plugin/share.h"

#include "lib/file.h"

#include "mime.h"

#include "meta.h"
#include "meta_mp3.h"
#include "meta_ogg.h"
#include "meta_image.h"
#include "meta_avi.h"

#include <ctype.h>

/*****************************************************************************/

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
