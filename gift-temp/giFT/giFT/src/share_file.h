/*
 * $Id: share_file.h,v 1.11 2003/05/04 06:55:49 jasta Exp $
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

#ifndef __SHARE_FILE_H
#define __SHARE_FILE_H

/*****************************************************************************/

#include "protocol.h"
#include "share_hash.h"

/*****************************************************************************/

/*
 * Deprecated design, use SHARE_DATA(file) to access to keep compatability
 * when it is removed.
 */
typedef struct
{
	char          *root;               /*       /data/mp3s          */
	char          *path;               /*       /data/mp3s/file.mp3 */
	char          *hpath;              /* path:           /file.mp3 */

	time_t         mtime;

	Dataset       *meta;

	/*
	 * if FileShare.p is NULL, we will be using the hashes dataset so
	 * that we may support all protocols loaded by giFT
	 *
	 * NOTE: protocol plugins should still go through hash_algo.c to fetch
	 * the appropriate hash algorithm
	 */
	union
	{
		Dataset   *hashes;
		Hash      *hash;
	} hash;
} ShareData;

#define SHARE_DATA(file) (&(file->sdata))

/*
 * Main share structure for both giFT and protocols alike.
 */
typedef struct _file_share
{
	ShareData      sdata;              /* this is split for legacy purposes...
	                                    * and it may go away soon */

	unsigned short ref;                /* see share_ref */

	char          *mime;               /* mime type, memory from mime.c */
	off_t          size;               /* file size */

	Protocol      *p;                  /* protocol responsible for this
	                                    * file share object */
	void          *data;               /* see share_lookup_data */
} FileShare;

/*****************************************************************************/

FileShare *share_new          (Protocol *p, char *root, size_t root_len,
                               char *path, char *mime, off_t size,
                               time_t mtime);
void       share_free         (FileShare *file);

/*****************************************************************************/

unsigned short share_ref      (FileShare *file);
unsigned short share_unref    (FileShare *file);

/*****************************************************************************/

void      *share_lookup_data  (FileShare *file, char *protocol);
void       share_insert_data  (FileShare *file, char *protocol, void *data);
void       share_remove_data  (FileShare *file, char *protocol);

/*****************************************************************************/

int        share_complete     (FileShare *file);

/*****************************************************************************/

#endif /* __SHARE_FILE_H */
