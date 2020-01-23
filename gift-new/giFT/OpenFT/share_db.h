/*
 * share_db.h
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

#ifndef __SHARE_DB_H
#define __SHARE_DB_H

/*****************************************************************************/

int  ft_share_flush          (FileShare *file, char *path, int purge);
int  ft_share_unflush        (FileShare *file, char *path);
int  ft_share_import         (FT_HostShare *h_share, char *path);
void ft_host_share_new_db    (FT_HostShare *h_share);
void ft_host_share_del_db    (FT_HostShare *h_share);

/*****************************************************************************/

#endif /* __SHARE_DB_H */
