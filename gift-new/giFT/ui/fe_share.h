/*
 * fe_share.h
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

#ifndef __FE_SHARE_H
#define __FE_SHARE_H

#include "fe_obj.h"

/**************************************************************************/

/*
 * Information about a shared file ... username, file, filesize, ...
 */
typedef struct _shared_file
{
	char *user;
	char *network;   /* ptr into user ... */

	char *mime_type; /* mime type of file until implemented this
			          * is guessed by share_mime_type (char* filename) */

	char *location;  /* href location to this file */
	char *filename;  /* displayed filename ... */
	size_t filesize; /* total file size (in bytes) */

	char *hash;      /* unique hash for identifying this share */
} SharedFile;

#define SHARE(obj) (&((obj)->shr))

/**************************************************************************/

int share_get_pixmap	(SharedFile *share, GdkColormap *colormap,
						 GdkPixmap **pixmap, GdkBitmap **bitmap);
void share_fill_data	(SharedFile *share, GData *ft_data);
void share_dup			(SharedFile *in, SharedFile *out);
void fe_share_free		(SharedFile *share);

/**************************************************************************/

#endif /* __FE_SHARE_H */
