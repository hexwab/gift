/*
 * share.h
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

#ifndef __SHARE_H
#define __SHARE_H

/**************************************************************************/

/*
 * Information about a shared file ... username, file, filesize, ...
 */
typedef struct _shared_file
{
	char *user;
	char *network;   /* ptr into user ... */

	char *location;  /* href location to this file */
	char *filename;  /* displayed filename ... */
	size_t filesize; /* total file size (in bytes) */

	char *hash;      /* unique hash for identifying this share */
} SharedFile;

#define SHARE(obj) (&((obj)->shr))

/**************************************************************************/

void share_fill_data (SharedFile *share, GData *ft_data);
void share_dup (SharedFile *in, SharedFile *out);
void share_free (SharedFile *share);

/**************************************************************************/

#endif /* __SHARE_H */
