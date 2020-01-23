/*
 * $Id: gt_share_file.h,v 1.7 2003/06/01 09:20:35 hipnod Exp $
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

#ifndef __GT_SHARE_FILE_H__
#define __GT_SHARE_FILE_H__

/******************************************************************************/

struct file_share;

struct gt_token_set;

struct gt_share
{
	uint32_t               index;
	char                  *filename;
	struct gt_token_set   *tokens;
};

typedef struct gt_share GtShare;

/******************************************************************************/

struct file_share   *gt_share_new   (char *filename, uint32_t index,
                                     off_t size, unsigned char *sha1);
void                 gt_share_free  (struct file_share *file);

/******************************************************************************/

struct gt_token_set *gt_share_tokenize (char *words);

/******************************************************************************/

unsigned int    gt_share_ref     (struct file_share *file);
unsigned int    gt_share_unref   (struct file_share *file);

/******************************************************************************/

GtShare    *gt_share_new_data  (struct file_share *file, uint32_t index);
void        gt_share_free_data (struct file_share *file, GtShare *share);

/******************************************************************************/

FileShare *gt_share_local_lookup_by_hash (unsigned char *sha1);

/******************************************************************************/

#endif /* __GT_SHARE_FILE_H__ */
