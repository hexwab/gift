/*
 * share_cache.h
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

#ifndef __SHARE_CACHE_H
#define __SHARE_CACHE_H

/*****************************************************************************/

#include "share_file.h"

/*****************************************************************************/

void       share_update_index ();
void       share_add_entry    (char *host_path);
void       share_remove_entry (char *host_path);
int        share_update_entry (FileShare *file);
void       share_write_index  ();
Dataset   *share_read_index   ();
void       share_clear_index  ();
Dataset   *share_index        (unsigned long *files, double *size);
List      *share_index_sorted ();
void       share_foreach      (HashFunc foreach_func, void *data);

FileShare *share_find_file    (char *filename);

/*****************************************************************************/

#endif /* __SHARE_CACHE_H */
