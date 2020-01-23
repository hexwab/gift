/*
 * $Id: ft_share_file.c,v 1.19 2003/06/24 19:57:20 jasta Exp $
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

#include "ft_openft.h"

#include "src/meta.h"
#include "md5.h"

#include "ft_search.h"
#include "ft_search_exec.h"
#include "ft_share_file.h"

/*****************************************************************************/

FTShare *ft_share_new_data (Share *file, FTNode *node)
{
	FTShare *share;

	if (!file)
		return NULL;

	if (!(share = malloc (sizeof (FTShare))))
		return NULL;

	share->node   = node;
	share->tokens = ft_search_tokenizef (file);

	return share;
}

Share *ft_share_new (FTNode *node, off_t size,
                     unsigned char *md5, char *mime, char *filename)
{
	Share *file;

	if (!(file = share_new_ex (FT, NULL, 0, filename, mime, size, 0)))
		return NULL;

	if (!share_set_hash (file, "MD5", md5, 16, TRUE))
	{
		ft_share_unref (file);
		return NULL;
	}

	share_set_udata (file, "OpenFT", ft_share_new_data (file, node));

	return file;
}

/*****************************************************************************/

void ft_share_free_data (Share *file, FTShare *share)
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

void ft_share_free (Share *file)
{
	ft_share_free_data (file, share_get_udata (file, "OpenFT"));
	share_set_udata (file, "OpenFT", NULL);

	share_free (file);
}

/*****************************************************************************/

unsigned int ft_share_ref (Share *file)
{
	return share_ref (file);
}

unsigned int ft_share_unref (Share *file)
{
	FTShare *share;

	if ((share = share_get_udata (file, "OpenFT")))
	{
		if (file->ref <= 1)
		{
			ft_share_free_data (file, share);
			share_set_udata (file, "OpenFT", NULL);
		}
	}

	return share_unref (file);
}

/*****************************************************************************/

BOOL ft_share_complete (Share *file)
{
	FTShare *share;

	/* check if giFT thinks its complete before we handle protocol
	 * specific checks */
	if (!share_complete (file))
		return FALSE;

	if (!(share = share_get_udata (file, "OpenFT")))
		return FALSE;

	/* tokenize this query for fast searching */
	if (!share->tokens)
		share->tokens = ft_search_tokenizef (file);

	if (!share->tokens)
		return FALSE;

	return TRUE;
}
