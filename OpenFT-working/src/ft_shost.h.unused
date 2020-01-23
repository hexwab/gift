/*
 * $Id: ft_shost.h,v 1.18 2003/05/05 09:49:11 jasta Exp $
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

#ifndef __FT_SHOST_H
#define __FT_SHOST_H

/*****************************************************************************/

/**
 * @file ft_shost.h
 *
 * @brief Manage child nodes.
 *
 * Organizes and manages the sharing aspect of children nodes.
 *
 * @todo This API needs to reflect the newly rewritten FTNode system by
 *       providing more sane flexibility with regard to ft_shost_new.
 */

/*****************************************************************************/

#ifdef USE_LIBDB
# ifdef HAVE_DB4_DB_H
#  include <db4/db.h>
# endif
# ifdef HAVE_DB_H
#  include <db.h>
# endif
# ifdef HAVE_DB3_DB_H
#  include <db3/db.h>
# endif
#endif /* USE_LIBDB */

/*****************************************************************************/

/**
 * Share host structure.
 *
 * \todo Use a direct link to the corresponding node structure rather than
 * copying the data.
 */
typedef struct _ft_shost
{
	in_addr_t      host;               /**< IPv4 address */
	in_port_t      ft_port;            /**< OpenFT connection port */
	in_port_t      http_port;          /**< HTTP connection port */
	char          *alias;              /**< nickname */

	unsigned char  verified : 1;       /**< ports verified to be accurate? */

	unsigned long  availability;       /**< precalculated availability
	                                    *   report: eg. total number of
	                                    *   immediately available upload
	                                    *   positions */

#if 0
	/**
	 * Temporary list of MATCHED searched results from this node so that they
	 * may be unref'd if this node disconnects.
	 */
	List          *files;
#endif

	unsigned long  shares;             /**< precalculated total shares */
	double         size;               /**< precalculated total size (MB) */

	unsigned char  dirty : 1;          /**< host has been scheduled for
	                                    *   removal and no search results
	                                    *   should be considered valid */
	void         (*removecb) (struct _ft_shost *shost);

	unsigned int   index;              /**< index in shosts */

#ifdef USE_LIBDB
	StopWatch     *sw;                 /**< used when removing shares */
	DB            *pri;                /**< primary database/index.
	                                    *   MD5 (16 byte) key: FileShare data. */
	DB            *sec;                /**< secondary index.
	                                    *   token (4 byte) key: MD5 data. */
	Array         *tokens;           /**< tokens used */

	DBC           *pri_curs;           /**< cursor for removal */
	DB_TXN        *pri_tid;            /**< transaction wrapper for removal */
#endif /* USE_LIBDB */
} FTSHost;

/*****************************************************************************/

/**
 * Accessor for all share hosts.
 */
Dataset *ft_shosts ();

/**
 * Allocate a new share host.  This does not add to the shost table.
 */
FTSHost *ft_shost_new (int verified, in_addr_t host,
                       in_port_t ft_port, in_port_t http_port,
                       char *alias);

/**
 * Unallocate a share host.
 */
void ft_shost_free (FTSHost *host);

FTSHost *ft_shost_get (in_addr_t host);
FTSHost *ft_shost_get_index (int index);
int ft_shost_add (FTSHost *shost);
void ft_shost_remove (in_addr_t host);

/*****************************************************************************/

void ft_shost_verified (in_addr_t ip, in_port_t ft_port,
                        in_port_t http_port, int verified);
void ft_shost_avail (in_addr_t ip, unsigned long avail);

int ft_shost_digest (in_addr_t host, unsigned long *shares,
                     double *size, unsigned long *avail);

/*****************************************************************************/

#endif /* __FT_SHOST_H */
