/*
 * share.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#include "gift-fe.h"

#include "share.h"

void share_fill_data (SharedFile *share, GData *ft_data)
{
	if (!share || !ft_data)
		return;

	/* user = 'user@network', network = 'network' */
	share->user = format_user_disp (g_datalist_get_data (&ft_data, "user"));

	if (share->user)
	{
		if ((share->network = strrchr (share->user, '@')))
			share->network++;
	}
	else
		share->network = NULL;

	/*
	 * location = 'http://bla/bla/file%5bthis%5d+that.bla'
	 * filename = 'file[this] that.bla'
	 */
	share->location = STRDUP (g_datalist_get_data (&ft_data, "href"));
	share->filename = format_href_disp (share->location);

	share->filesize = ATOI (g_datalist_get_data (&ft_data, "size"));

	share->hash = STRDUP (g_datalist_get_data (&ft_data, "hash"));
}

/* dup for object movement through the list structures ... favored over a
 * reference count system ... */
void share_dup (SharedFile *out, SharedFile *in)
{
	out->user = STRDUP (in->user);

	if (out->user)
	{
		if ((out->network = strrchr (out->user, '@')))
			out->network++;
	}
	else
		out->user = NULL;

	out->location = STRDUP (in->location);
	out->filename = STRDUP (in->filename);

	out->filesize = in->filesize;

	out->hash = STRDUP (in->hash);
}

void share_free (SharedFile *share)
{
	free (share->user);
	share->network = NULL;

	free (share->location);
	free (share->filename);

	share->filesize = 0;

	free (share->hash);

	/* don't free share, it wasn't dynamically allocated ... */
}
