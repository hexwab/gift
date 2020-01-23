/*
 * ft_shost.h
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
 * Structure of a single shared record in the primary libdb 'table'.
 */
typedef struct
{
	off_t          size;               /**< file size */
	unsigned char  md5[16];            /**< md5sum */
	unsigned short mime;               /**< offset of mime type */
	unsigned short path;               /**< offset of path */
	unsigned short meta;               /**< offset of meta data */
	unsigned short data_len;           /**< total data length */
	unsigned char  data[8192];         /**< complete linear record data.
										*   the libdb code does NOT write
										*   all sizeof(data) bytes so be
										*   very careful with this
										*   field as its size is merely to
										*   represent the maximum rec size. */
} FTShareRec;

/**
 * Share host structure.
 *
 * \todo Use a direct link to the corresponding node structure rather than
 * copying the data.
 */
typedef struct
{
	in_addr_t      host;               /**< IPv4 address */
	unsigned short ft_port;            /**< OpenFT connection port */
	unsigned short http_port;          /**< HTTP connection port */
	char          *alias;              /**< nickname */

	unsigned char  verified : 1;       /**< ports verified to be accurate? */

	unsigned long  availability;       /**<  precalculated availability
	                                    *    report: eg. total number of
	                                    *    immediately available upload
	                                    *    positions */

	/**
	 * Temporary list of MATCHED searched results from this node so that they
	 * may be unref'd if this node disconnects.
	 */
	List          *files;

	unsigned long  shares;             /**< precalculated total shares */
	double         size;               /**< precalculated total size (MB) */

#ifdef USE_LIBDB
	DB            *pri;                /**< primary database/index.
	                                    *   MD5 (16 byte) key: FileShare data. */
	DB            *sec;                /**< secondary index.
	                                    *  token (4 byte) key: MD5 data. */
	DB            *ter;                /**< ternary index.
	                                    *   UNDEFINED/UNUSED */

	unsigned long  pri_timer;          /**< timer assigned to making sure
										*   the primary database doesnt
										*   remain constantly open */
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
                       unsigned short ft_port, unsigned short http_port,
                       char *alias);

/**
 * Unallocate a share host.
 */
void ft_shost_free (FTSHost *host);

FTSHost *ft_shost_get (in_addr_t host);
void ft_shost_add (FTSHost *shost);
void ft_shost_remove (in_addr_t host);

/*****************************************************************************/

#ifdef USE_LIBDB
DB *ft_shost_pri (FTSHost *shost);
DB *ft_shost_sec (FTSHost *shost);
#endif /* USE_LIBDB */

FileShare *ft_record_file (FTShareRec *rec, FTSHost *shost);

int ft_shost_add_file (FTSHost *shost, FileShare *file);
int ft_shost_remove_file (FTSHost *shost, unsigned char *md5);
int ft_shost_sync (FTSHost *shost);

/*****************************************************************************/

void ft_shost_verified (in_addr_t ip, unsigned short ft_port,
                        unsigned short http_port, int verified);
void ft_shost_avail (in_addr_t ip, unsigned long avail);

int ft_shost_digest (in_addr_t host, unsigned long *shares,
                     double *size, unsigned long *avail);

/*****************************************************************************/

#endif /* __FT_SHOST_H */
