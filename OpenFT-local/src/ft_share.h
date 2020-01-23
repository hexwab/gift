/*
 * $Id: ft_share.h,v 1.13 2003/07/10 15:32:04 jasta Exp $
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

#ifndef __FT_SHARE_H
#define __FT_SHARE_H

/*****************************************************************************/

#include "ft_share_file.h"

/*****************************************************************************/

void       ft_share_local_add        (FileShare *file, int syncing);
void       ft_share_local_remove     (FileShare *file, int syncing);
void       ft_share_local_flush      ();
void       ft_share_local_sync       (int begin);
char      *ft_share_local_verify     (char *filename);
FileShare *ft_share_local_find       (char *filename);
void       ft_share_local_submit     (TCPC *c);

/*****************************************************************************/

void *openft_share_new (Protocol *p, FileShare *file);
void openft_share_free (Protocol *p, FileShare *file, void *data);
int openft_share_add (Protocol *p, FileShare *file, void *data);
int openft_share_remove (Protocol *p, FileShare *file, void *data);
void openft_share_sync (Protocol *p, int begin);
void openft_share_hide (Protocol *p);
void openft_share_show (Protocol *p);

/*****************************************************************************/

#endif /* __FT_SHARE_H */
