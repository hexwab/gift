/*
 * zip.c
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

#include "zip.h"

/*****************************************************************************/

#ifdef USE_ZLIB

#include <zlib.h>

#include "openft.h"

#include "file.h"
#include "netorg.h"

/*****************************************************************************/

#define COMPRESSION_LEVEL Z_BEST_COMPRESSION

/*****************************************************************************/

static void calc_share_len_foreach (char *key, FileShare *share,
								    ft_uint32 *share_len)
{
	assert (share);
	assert (share->path);

	/* strlen(md5) + null + strlen(size) + null + strlen(share->path) + null */
	*share_len += 33 + 12 + strlen(share->path) + 1;
}

/* calculate the maximum length required to store the share data
 * (md5/len/file) */
static ft_uint32 calc_share_len (Dataset *shares)
{
	ft_uint32 share_len = 0U;

	assert (shares);

	hash_table_foreach (shares, (HashFunc) calc_share_len_foreach, &share_len);

	return share_len;
}

static void add_share (char *key, FileShare *share, char **pp)
{
	ft_uint32 size;

	assert (share);
	assert (share->path);
	assert (share->md5);

	/* TODO report correct size for 4GB+ files */
	size = MIN (share->size, 0xffffffffU);

	/* TODO store md5 and size as binary to save space and time */
	*pp += sprintf (*pp, "%s", share->md5) + 1;
	*pp += sprintf (*pp, "%" G_FT_UINT32_FORMAT, size) + 1;
	*pp += sprintf (*pp, "%s", share->path) + 1;
}
/*****************************************************************************/

/* Create a buffer of compressed data containing the md5, size, and filename
 * for all local shares.
 * Return buffer in data of length len. */
int zip_shares (Dataset *shares, ft_uint32 share_len,
				ZData **pzdata, ft_uint32 *pz_len)
{
	ZData     *zdata;
	char      *u_data;
	ft_uint32  u_len;
	ft_uint32  z_len;
	int        ret;
	char      *p;

	assert(shares);

	*pzdata = NULL;
	*pz_len = 0;

	/* calculate the maximum length required to store the share data
	 * (md5/len/file)
	 */
	if (!share_len)
		share_len = calc_share_len (shares);

	u_data = malloc (share_len);
	if (!u_data)
	{
		perror ("Out of memory");
		return FALSE;
	}

	p = u_data;

	/* build the buffer to compress */
	hash_table_foreach (shares, (HashFunc) add_share, &p);

	u_len = p - u_data;

	assert (u_len <= share_len);

	/* see zlib.h line 630:
	 * "which must be at least 0.1% larger than sourceLen plus 12 bytes."
	 * the len of the uncompressed len value
	 */
	z_len = u_len + ((u_len + 999) / 1000) + 12 + sizeof (ZData);

	zdata = (ZData*) malloc (z_len);
	if (!zdata)
	{
		perror ("Out of memory");
		free (u_data);
		return FALSE;
	}

	zdata->u_len = u_len;

	if ((ret = compress2 ((Bytef*) zdata->data,
						  (uLongf*) &z_len, (Bytef*) u_data,
						  (uLong) u_len, COMPRESSION_LEVEL)))
	{
		fprintf (stderr, "zlib error %d\n", ret);
		free (zdata);
		free (u_data);
		return FALSE;
	}

	/* not needed, but a nice sanity check */
	zdata->z_len = z_len;

	free (u_data);

	*pzdata = zdata;
	*pz_len = z_len;

	return TRUE;
}

/*****************************************************************************/

int unzip_shares (Connection *c, ZData *zdata)
{
	char      *z_data; /* compressed buffer */
	ft_uint32  z_len;  /* compressed buffer size */
	char      *u_data; /* uncompressed buffer */
	ft_uint32  u_len;  /* uncompressed buffer size */
	char      *ptr;
	char      *sentinel;
	int        ret;

	assert (zdata);

	z_len  = zdata->z_len;
	u_len  = zdata->u_len;
	z_data = zdata->data;

	u_data = malloc (u_len);
	if (!u_data)
	{
		perror ("Out of memory");
		return FALSE;
	}

    if ((ret = uncompress (u_data, (uLong*) &u_len, z_data, z_len)))
	{
		free (u_data);
		fprintf (stderr, "zlib error: %d\n", ret);
		return FALSE;
	}

	if (!u_len)
	{
		free (u_data);
		/* should we still call submit_share_digest()? */
		return FALSE;
	}

	assert (u_len == zdata->u_len);

	ptr = u_data;
	sentinel = u_data + u_len;

	while (ptr < sentinel)
	{
		char      md5[33];
		char      s[12];
		ft_uint32 size;
		char      filename[PATH_MAX];

		strncpy (md5, ptr, sizeof (md5));
		md5[32] = '\0';
		ptr += strlen (md5) + 1;
		assert (ptr < sentinel);

		strncpy (s, ptr, sizeof (s));
		s[11] = '\0';
		ptr += strlen (s) + 1;
		sscanf (s, "%" G_FT_UINT32_FORMAT, &size);
		assert (ptr < sentinel);

		strncpy (filename, ptr, sizeof (filename));
		filename[PATH_MAX - 1] = '\0';
		ptr += strlen (filename) + 1;
		assert (ptr <= sentinel);

		/* actually create and place the share */
		ft_share_add (NODE (c)->verified, NODE (c)->ip, NODE (c)->port,
			          NODE (c)->http_port, size, md5, filename);
	}

	free (u_data);

	return TRUE;
}

/*****************************************************************************/

/* not currently used, but may prove useful later */

/* Write compressed share data to OpenFT/shares.z.
 * Return TRUE if successful */
int zip_write_shares (Dataset *shares, ft_uint32 share_len)
{
	ZData     *zdata;
	ft_uint32  zdata_len; /* length of zdata header + zdata->data */
	char      *path;
	FILE      *f;
	size_t     written;

	path = gift_conf_path ("shares.z");

	unlink (path);

	if (!(zip_shares (shares, share_len, &zdata, &zdata_len)))
		return FALSE;

	if (!(f = fopen (path, "wb")))
	{
		perror ("fopen");
		free (zdata);
		return FALSE;
	}

	written = fwrite (zdata, sizeof(char), zdata_len, f);

	free (zdata);

	if (zdata_len != written)
	{
		perror ("fwrite");
		fclose (f);
		unlink (path);
		return FALSE;
	}

	if ((fclose (f)))
	{
		perror ("fclose");
		unlink (path);
		return FALSE;
	}

	return TRUE;
}

#endif /* USE_ZLIB */
