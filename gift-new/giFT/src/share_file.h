/*
 * share_file.h
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

#ifndef __SHARE_FILE_H
#define __SHARE_FILE_H

/*****************************************************************************/

/* the ShareData structure is used so that FileShare structures may flush
 * the data needed to store root, path, etc etc when moved to disk
 *
 * NOTE: this saves ~24 bytes per flushed entry at the cost of 4 per
 * unflushed */
typedef struct
{
	char         *root;     /* /data/mp3s                               */
	char         *path;     /* /data/mp3s/file.mp3                      */
	char         *hpath;    /*           /file.mp3 (memory from path)   */

	/* TODO -- this shouldn't be a string */
	char         *md5;
} ShareData;

typedef struct _file_share
{
	off_t          flushed;  /* if this data was flushed to disk, this will be
	                          * > 0 to indicate what it's position is */

	unsigned short ref;

	ShareData     *sdata;    /* hold a pointer to the actual data unflushed
	                          * from disk */

	/* this data is more efficient to be kept here */
	unsigned long  size;

	/* TEMPORARY -- these will move to sdata soon */
	char          *type;     /* audio/mpeg          (memory from mime.c) */
	time_t         mtime;

	/* The protocol that created this share.
	 * NOTE: this may be NULL, in which case giFT created it */
	Protocol      *p;
	void          *data;     /* see share_free */
} FileShare;

/*****************************************************************************/

FileShare *share_new          (Protocol *p, int p_notify,
                               char *root, size_t root_len, char *path,
                               char *md5, unsigned long size, time_t mtime);
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
