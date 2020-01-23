/*
 * zip.h
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

#ifndef __ZIP_H
#define __ZIP_H

#include "gift.h"

#ifdef USE_ZLIB

/* ZData contains the entire compressed data stream that is built by
 * share_compress(), which is then broken up into chunks (FT_Chunk's)
 * and transmitted by ft_share_local_submit().
 */
#define ZDATA_HEADER_LEN (sizeof(ft_uint32)+sizeof(ft_uint32)+sizeof(ft_uint32))

typedef struct
{
	ft_uint32 u_len;   /* length of uncompressed data, used for sanity check */
	ft_uint32 z_len;   /* length of compressed data */
	char      data[1]; /* compressed data. pointer trickery.
	                    * z_len bytes after this are still
	                    * our valid memory segment */
} ZData;

/*****************************************************************************/

#define FT_CHUNK_HEADER_LEN (sizeof(ft_uint16) + sizeof(ft_uint16) + sizeof(ft_uint32))

typedef struct
{
	ft_uint16 id;	   /* id of the packet, 0-based, is 64k chunks enough? */
	ft_uint16 done;	   /* TRUE if last packet */
	char      data[1]; /* pointer trickery.  len bytes after this are still our
				        * valid memory segment */
} FT_Chunk;

/* maximum size of chunk to send */
#define MAX_CHUNK_SIZE (0x4000U - FT_CHUNK_HEADER_LEN) /* 16k per jasta */

/*****************************************************************************/

int zip_shares       (Dataset *shares, ft_uint32 share_bytes,
					  ZData **pzdata, ft_uint32 *pz_len);
int unzip_shares     (Connection *c, ZData *zdata);
int zip_write_shares (Dataset *shares, ft_uint32 share_len);

#endif /* USE_ZLIB */

#endif /* __ZIP_H */
