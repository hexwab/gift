/*
 * ft_share_file.c
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

#include "ft_openft.h"

#include "meta.h"
#include "md5.h"

#include "ft_search.h"
#include "ft_search_exec.h"
#include "ft_share_file.h"

/*****************************************************************************/

FTShare *ft_share_new_data (FileShare *file, FTSHost *shost)
{
	FTShare *share;

	if (!file || !shost)
		return NULL;

	if (!(share = malloc (sizeof (FTShare))))
		return NULL;

	share->shost  = shost;
	share->tokens = ft_search_tokenizef (file);

	return share;
}

FileShare *ft_share_new (FTSHost *shost, off_t size,
                         unsigned char *md5, char *mime, char *filename)
{
	FileShare     *file;
	unsigned char *dup;

	/* duplicate the md5sum just in case the address supplied was not from
	 * malloc (the share_hash_set api trusts the memory we supplied to it
	 * was allocated) */
	if (!(dup = md5_dup (md5)))
		return NULL;

	if (!(file = share_new (openft_p, NULL, 0, filename, mime, size, 0)))
	{
		free (dup);
		return NULL;
	}

	if (!share_hash_set (file, "MD5", dup, 16))
	{
		ft_share_unref (file);
		free (dup);

		return NULL;
	}

	share_insert_data (file, "OpenFT", ft_share_new_data (file, shost));

	return file;
}

/*****************************************************************************/

void ft_share_free_data (FileShare *file, FTShare *share)
{
	if (!file)
	{
		assert (share != NULL);
		return;
	}

	if (!share)
		return;

	free (share->tokens);
	free (share);
}

void ft_share_free (FileShare *file)
{
	ft_share_free_data (file, share_lookup_data (file, "OpenFT"));
	share_remove_data (file, "OpenFT");

	share_free (file);
}

/*****************************************************************************/

unsigned short ft_share_ref (FileShare *file)
{
	return share_ref (file);
}

unsigned short ft_share_unref (FileShare *file)
{
	FTShare *share;

	if ((share = share_lookup_data (file, "OpenFT")))
	{
		if (file->ref <= 1)
		{
			ft_share_free_data (file, share_lookup_data (file, "OpenFT"));
			share_remove_data (file, "OpenFT");
		}
	}

	return share_unref (file);
}

/*****************************************************************************/

int ft_share_complete (FileShare *file)
{
	FTShare *share;

	/* check if giFT thinks its complete before we handle protocol
	 * specific checks */
	if (!share_complete (file))
		return FALSE;

	if (!(share = share_lookup_data (file, "OpenFT")))
		return FALSE;

	/* tokenize this query for fast searching */
	if (!share->tokens)
		share->tokens = ft_search_tokenizef (file);

	if (!share->tokens)
		return FALSE;

	return TRUE;
}
