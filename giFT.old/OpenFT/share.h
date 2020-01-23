/*
 * share.h
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

#ifndef __OPENFT_SHARE_H
#define __OPENFT_SHARE_H

/*****************************************************************************/

#include "sharing.h"

/*****************************************************************************/

typedef struct
{
	unsigned long  host;       /* ip of the user who owns this share */
	unsigned short port;
	unsigned short http_port;

	ft_uint32     *tokens;     /* list of tokens for quick searching */

	int            ref;
	int            destroying;
} OpenFT_Share;

/*****************************************************************************/

void       openft_share_ref            (FileShare *file);
void       openft_share_unref          (FileShare *file);

int        openft_share_complete       (FileShare *file);
void       openft_share_free           (FileShare *file);
FileShare *openft_share_new            (char *host, unsigned short port,
                                        unsigned short http_port,
                                        unsigned long size, char *md5, char *file);
void       openft_share_add            (FileShare *file);
void       openft_share_remove         (FileShare *file);
void       openft_share_remove_by_host (unsigned long host);

void       openft_share_local_import   ();
void       openft_share_local_cleanup  ();
char      *openft_share_local_verify   (char *filename);
FileShare *openft_share_local_find     (char *filename);
void       openft_share_local_submit   (Connection *c);

void       openft_share_stats_reset    (Connection *c);
void       openft_share_stats_add      (Connection *c, FileShare *file);
void       openft_share_stats_remove   (Connection *c, FileShare *file);
void       openft_share_stats_set      (Connection *c, unsigned long users,
                                        unsigned long shares, unsigned long megs);

/*****************************************************************************/

#endif /* __OPENFT_SHARE_H */
