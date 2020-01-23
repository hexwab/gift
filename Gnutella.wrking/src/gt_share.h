/*
 * $Id: gt_share.h,v 1.9 2004/03/24 06:36:54 hipnod Exp $
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

#ifndef __GT_SHARE_H__
#define __GT_SHARE_H__

/******************************************************************************/

struct gt_share;
struct file_share;

/******************************************************************************/

FileShare *gt_share_local_lookup_by_index (uint32_t index, char *file);
FileShare *gt_share_local_lookup_by_urn   (char *urn);
int        gt_share_local_sync_is_done    (void);
char      *gt_share_local_get_urns        (FileShare *file);

/******************************************************************************/

void *gnutella_share_new    (Protocol *p, FileShare *file);
void  gnutella_share_free   (Protocol *p, FileShare *file, void *data);

int   gnutella_share_add    (Protocol *p, FileShare *file, void *data);
int   gnutella_share_remove (Protocol *p, FileShare *file, void *data);

void  gnutella_share_sync   (Protocol *p, int begin);

/*****************************************************************************/

void  gnutella_share_hide   (Protocol *p);
void  gnutella_share_show   (Protocol *p);

/******************************************************************************/

/* need these definitions but giFT doesn't provide them to plugins */
extern Dataset *share_index   (unsigned long *files, double *size);
extern void     share_foreach (DatasetForeachExFn foreach_fn, void *data);

/******************************************************************************/

#endif /* __GT_SHARE_H__ */
