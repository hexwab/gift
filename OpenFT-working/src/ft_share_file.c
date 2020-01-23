/*
 * $Id: ft_share_file.c,v 1.23 2004/08/02 20:44:55 hexwab Exp $
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

#include "md5.h"

#include "ft_search.h"
#include "ft_tokenize.h"
#include "ft_share_file.h"
#include "ft_tokenize.h"

/*****************************************************************************/

FTShare *ft_share_new_data (Share *file, FTNode *node, ft_nodeinfo_t *ninfo)
{
	FTShare *share;

	if (!file)
		return NULL;

	if (!(share = malloc (sizeof (FTShare))))
		return NULL;

	/* hack hack hack */
	if (node)
		assert (ninfo != NULL);

	share->node = node;
	share->ninfo = ninfo;
<<<<<<< ft_share_file.c
	share->tokens = ft_tokenize_share (file, &share->order);
=======
	share->tokens = ft_tokenize_share (file, NULL);
>>>>>>> 1.23

	return share;
}

Share *ft_share_new (FTNode *node, off_t size,
                     unsigned char *md5, char *mime, char *filename)
{
	Share   *file;
	FTShare *share;

	if (!(file = share_new_ex (FT, filename, mime, size, 0)))
		return NULL;

	if (!share_set_hash (file, "MD5", md5, 16, TRUE))
	{
		ft_share_unref (file);
		return NULL;
	}

	assert (node != NULL);

	if (!(share = ft_share_new_data (file, node, &node->ninfo)))
	{
		ft_share_unref (file);
		return NULL;
	}

	share_set_udata (file, FT->name, share);
	assert (share_get_udata (file, FT->name) == share);

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
	free (share->order);
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

	/* tokenize this share for fast searching */
	if (!share->tokens)
<<<<<<< ft_share_file.c
		share->tokens = ft_tokenize_share (file, &share->order);
=======
		share->tokens = ft_tokenize_share (file, NULL);
>>>>>>> 1.23

	if (!share->tokens)
		return FALSE;

	return TRUE;
}
