/*
 * $Id: gt_share_file.c,v 1.15 2003/06/05 05:59:25 hipnod Exp $
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

#include "gt_gnutella.h"

#include "sha1.h"
#include "src/mime.h"

#include "gt_share_file.h"
#include "gt_query_route.h"

#include "gt_search.h"
#include "gt_search_exec.h"

/******************************************************************************/

unsigned int gt_share_unref (FileShare *file)
{
	GtShare *share;

	if ((share = share_get_udata (file, gnutella_p->name)))
	{
		if (file->ref <= 1)
		{
			gt_share_free_data (file, share);
			share_set_udata (file, gnutella_p->name, NULL);
		}
	}

	return share_unref (file);
}

unsigned int gt_share_ref (FileShare *file)
{
	return share_ref (file);
}

/******************************************************************************/

/* this is duplicated with tokenize in gt_query_route.c for the time being */
GtTokenSet *gt_share_tokenize (char *hpath)
{
	GtTokenSet *token_set;
	char       *str;
	char       *str0;
	char       *next;

	if (!(str0 = str = STRDUP (hpath)))
		return NULL;

	if (!(token_set = gt_token_set_new ()))
	{
		free (str0);
		return NULL;
	}

	while ((next = string_sep_set (&str, QRP_DELIMITERS)) != NULL)
	{
		uint32_t tok;

		if (string_isempty (next))
			continue;

		tok = gt_query_router_hash_str (next, 32);
		gt_token_set_append (token_set, tok);
	}

	free (str0);
	return token_set;
}

/******************************************************************************/

GtShare *gt_share_new_data (FileShare *file, uint32_t index)
{
	GtShare  *share;
	char     *basename;

	if (!file)
		return NULL;

	if (!(share = malloc (sizeof (GtShare))))
		return NULL;

	assert (SHARE_DATA(file) != NULL);

	if (!(basename = file_basename (SHARE_DATA(file)->path)))
	{
		GT->DBGFN (GT, "bad basename for %s", SHARE_DATA(file)->path);
		free (share);
		return NULL;
	}

	share->index    = index;
	share->filename = STRDUP (basename);
	share->tokens   = gt_share_tokenize (share_get_hpath (file));

	return share;
}

void gt_share_free_data (FileShare *file, GtShare *share)
{
	if (!file)
	{
		assert (share != NULL);
		return;
	}

	if (!share)
		return;

	gt_token_set_free (share->tokens);

	free (share->filename);
	free (share);
}

FileShare *gt_share_new (char *filename, uint32_t index, off_t size,
                         unsigned char *sha1)
{
	FileShare *file;
	GtShare   *share;
	char      *mime;

	/* choose mime type based on extension */
	mime = mime_type (filename);

	/* TODO: parse path out of filename. Nodes don't put paths in the
	 *       search result string yet but may in the future */
	if (!(file = share_new (filename)))
		return NULL;

	share_set_mime (file, mime);
	file->size = size;

	/* 
	 * Set the hash if it was passed in, but also accept a null hash
	 * TODO: need a generic urn: type to stop hardcoding sha1 
	 */
	if (sha1 && !share_set_hash (file, "SHA1", sha1, SHA1_BINSIZE, TRUE))
	{
		gt_share_unref (file);
		return NULL;
	}

	if (!(share = gt_share_new_data (file, index)))
	{
		gt_share_unref (file);
		return NULL;
	}

	share_set_udata (file, gnutella_p->name, share);

	return file;
}

void gt_share_free (FileShare *file)
{
	gt_share_free_data (file, share_get_udata (file, gnutella_p->name));
	share_set_udata (file, gnutella_p->name, NULL);

	share_free (file);
}
