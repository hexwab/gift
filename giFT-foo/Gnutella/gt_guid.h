/*
 * $Id: gt_guid.h,v 1.4 2003/06/01 09:38:03 hipnod Exp $
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

#ifndef __GT_GUID_H__
#define __GT_GUID_H__

/*****************************************************************************/

#define GUID_LEN      16

/*****************************************************************************/

typedef unsigned char gt_guid_t;

/*****************************************************************************/

gt_guid_t  *gt_guid_new    (void);
int         gt_guid_cmp    (gt_guid_t *a, gt_guid_t *b);
char       *gt_guid_str    (gt_guid_t *guid);
gt_guid_t  *gt_guid_dup    (gt_guid_t *guid);
gt_guid_t  *gt_guid_bin    (char *guid_ascii);

/*****************************************************************************/

#endif /* __GT_GUID_H__ */
