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

#ifndef __FT_SHARE_H
#define __FT_SHARE_H

/*****************************************************************************/

#include "sharing.h"

/*****************************************************************************/

typedef struct
{
	unsigned long  host;         /* ip of the user who owns the shares in
	                              * dataset */
	unsigned short port;
	unsigned short http_port;

	int            disabled;     /* this host wishes not to share the files
	                              * listed in the dataset */
	int            uploads;      /* currently active uploads */
	int            max_uploads;  /* maximum simultaneous uploads allowed */
	int            availability; /* how many open upload slots does this host
								  * have */

	int            verified;     /* set to TRUE after both ports are verified */
	Dataset       *dataset;

	int            db_timeout;
	char           db_flushed;   /* if all shares are flushed, this flag is
	                              * set to ignore the timeout */
} FT_HostShare;

typedef struct
{
	FT_HostShare  *host_share; /* reference to parent */

	ft_uint32     *tokens;     /* list of tokens for quick searching */

	unsigned short ref;
} FT_Share;

typedef struct
{
	List         *parents;     /* parent search nodes that submitted this
								* digest...when parents == NULL, the digest
								* will be destroyed and removed.  backasswards
								* ref counting. */
	unsigned long users;       /* this value depends on context of the
								* structure */
	unsigned long shares;
	double        size;        /* GB */
} FT_Stats;

/*****************************************************************************/

void       ft_share_ref              (FileShare *file);
void       ft_share_unref            (FileShare *file);

/*****************************************************************************/

FT_HostShare *ft_host_share_new      (int verified, unsigned long host,
                                      unsigned short port,
                                      unsigned short http_port);
FT_HostShare *ft_host_share_add      (int verified, unsigned long host,
                                      unsigned short port,
                                      unsigned short http_port);
void          ft_host_share_free     (FT_HostShare *h_share);
void          ft_host_share_status   (FT_HostShare *h_share, int enabled,
									  int uploads, int max_uploads);

/*****************************************************************************/

int        ft_share_complete         (FileShare *file);
void       ft_share_free             (FileShare *file);
FileShare *ft_share_new              (FT_HostShare *host_share,
                                      unsigned long size, char *md5,
                                      char *filename);

void       ft_share_add              (int verified,
									  unsigned long host, unsigned short port,
                                      unsigned short http_port,
                                      unsigned long size, char *md5,
                                      char *filename);
void       ft_share_remove           (FileShare *file);
void       ft_share_remove_by_host   (unsigned long host, int force);

void       ft_share_verified         (unsigned long host);

/*****************************************************************************/

void       ft_share_disable          (unsigned long host);
void       ft_share_enable           (unsigned long host);
void       ft_share_set_limit        (unsigned long host, int max_slots);
void       ft_share_set_uploads      (unsigned long host, int uploads);

/*****************************************************************************/

void       ft_share_local_add        (FileShare *file);
void       ft_share_local_remove     (FileShare *file);
void       ft_share_local_flush      ();
void       ft_share_local_sync       ();
void       ft_share_local_cleanup    ();
char      *ft_share_local_verify     (char *filename);
FileShare *ft_share_local_find       (char *filename);
void       ft_share_local_submit     (Connection *c);

/*****************************************************************************/

void       ft_share_stats_add        (unsigned long parent,
                                      unsigned long user,
                                      unsigned long shares,
                                      unsigned long size);
void       ft_share_stats_remove     (unsigned long parent,
                                      unsigned long user);
void       ft_share_stats_get        (unsigned long *users,
                                      unsigned long *shares,
                                      double *size);
void       ft_share_stats_get_digest (unsigned long host,
                                      unsigned long *shares,
                                      double *size, int *uploads,
									  int *max_uploads);

/*****************************************************************************/

#endif /* __FT_SHARE_H */
