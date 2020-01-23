/*
 * sharing.h
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

#ifndef __SHARING_H
#define __SHARING_H

/*****************************************************************************/

typedef struct _file_share
{
	char         *root;   /* /data/mp3s                             */
	char         *path;   /* /data/mp3s/file.mp3                    */
	char         *hpath;  /*           /file.mp3 (memory from path) */
	char         *ext;    /*                .mp3 (memory from path) */
	char         *type;   /* audio/mpeg                             */

	List         *meta;
	char         *md5;

	unsigned long size;
	time_t        mtime;

	/* protocol-specific data */
	Dataset      *data;
} FileShare;

/*****************************************************************************/

FileShare *share_new          (char *root, char *path, char *md5,
                               unsigned long size, time_t mtime);
void       share_free         (FileShare *file);

Dataset   *share_build_index  ();
Dataset   *share_update_index ();
void       share_clear_index  ();
Dataset   *share_index        (unsigned long *files, double *size);
List      *share_index_sorted ();
void       share_foreach      (HashFunc foreach_func, void *data);

/*****************************************************************************/

#endif /* __SHARING_H */
