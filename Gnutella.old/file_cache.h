/*
 * $Id: file_cache.h,v 1.3 2003/07/01 16:15:12 hipnod Exp $
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

#ifndef __GIFT_FILE_CACHE_H
#define __GIFT_FILE_CACHE_H

typedef struct _file_cache
{
	Dataset *d;
	time_t   mtime;
	char    *file;
} FileCache;

FileCache    *file_cache_new    (char *file);
void          file_cache_free   (FileCache *cache);
int           file_cache_load   (FileCache *cache);
BOOL          file_cache_sync   (FileCache *cache);

char         *file_cache_lookup (FileCache *cache, char *key);
void          file_cache_insert (FileCache *cache, char *key, char *value);
void          file_cache_remove (FileCache *cache, char *key);

#endif /* __GIFT_FILE_CACHE_H */
