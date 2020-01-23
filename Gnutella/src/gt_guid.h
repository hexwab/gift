/*
 * $Id: gt_guid.h,v 1.10 2004/03/05 17:49:40 hipnod Exp $
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

#ifndef GIFT_GT_GUID_H_
#define GIFT_GT_GUID_H_

/*****************************************************************************/

#define GT_GUID_LEN      16

/*****************************************************************************/

typedef uint8_t gt_guid_t;

/*****************************************************************************/

gt_guid_t  *gt_guid_new          (void);
void        gt_guid_init         (gt_guid_t *guid);
int         gt_guid_cmp          (const gt_guid_t *a, const gt_guid_t *b);
char       *gt_guid_str          (const gt_guid_t *guid);
gt_guid_t  *gt_guid_dup          (const gt_guid_t *guid);
gt_guid_t  *gt_guid_bin          (const char *guid_ascii);

/*****************************************************************************/

BOOL        gt_guid_is_empty     (const gt_guid_t *guid);

/*****************************************************************************/

void        gt_guid_self_init    (void);
void        gt_guid_self_cleanup (void);

/*****************************************************************************/

#endif /* GIFT_GT_GUID_H_ */
