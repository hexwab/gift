/*
 * $Id: share_cache.h,v 1.9 2003/04/19 02:45:48 jasta Exp $
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

#ifndef __SHARE_CACHE_H
#define __SHARE_CACHE_H

/*****************************************************************************/

#include "share_file.h"

/*****************************************************************************/

#define RESYNC_INTERVAL \
	config_get_int (gift_conf, "sharing/auto_resync_interval=86400")

/*****************************************************************************/

void       share_update_index (void);
void       share_add_entry    (char *host_path);
void       share_remove_entry (char *host_path);
int        share_update_entry (FileShare *file);
void       share_write_index  (SubprocessData *subproc);
Dataset   *share_read_index   (void);
void       share_clear_index  (void);
Dataset   *share_index        (unsigned long *files, double *size);
List      *share_index_sorted (void);
void       share_foreach      (DatasetForeach foreach_func, void *data);

FileShare *share_find_file    (char *filename);
int        share_indexing     (void);
void       share_init_timer   (int timeout);

/*****************************************************************************/

#endif /* __SHARE_CACHE_H */
