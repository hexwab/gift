/*
 * $Id: gt_share_file.c,v 1.7 2003/05/04 07:36:10 hipnod Exp $
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
#include "mime.h"

#include "gt_share_file.h"
#include "gt_query_route.h"

#include "gt_search_exec.h"

/******************************************************************************/

unsigned short gt_share_unref (FileShare *file)
{
	Gt_Share *share;

	if ((share = share_lookup_data (file, gnutella_p->name)))
	{
		if (file->ref <= 1)
		{
			gt_share_free_data (file, share);
			share_remove_data (file, gnutella_p->name);
		}
	}

	return share_unref (file);
}

unsigned short gt_share_ref (FileShare *file)
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

		tok = query_router_hash_str (next, 32);
		gt_token_set_append (token_set, tok);
	}

	free (str0);
	return token_set;
}

/******************************************************************************/

Gt_Share *gt_share_new_data (FileShare *file, uint32_t index)
{
	Gt_Share *share;
	char     *basename;

	if (!file)
		return NULL;

	if (!(share = malloc (sizeof (Gt_Share))))
		return NULL;

	assert (SHARE_DATA(file) != NULL);

	if (!(basename = file_basename (SHARE_DATA(file)->path)))
	{
		TRACE (("bad basename for %s", SHARE_DATA(file)->path));
		free (share);
		return NULL;
	}

	share->index    = index;
	share->filename = STRDUP (basename);
	share->tokens   = gt_share_tokenize (SHARE_DATA(file)->hpath);

	return share;
}

void gt_share_free_data (FileShare *file, Gt_Share *share)
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
	FileShare     *file;
	Gt_Share      *share;
	unsigned char *dup = NULL;
	char          *mime;

	/* duplicate the sha1sum just in case the address supplied was not from
	 * malloc (the share_hash_set api trusts the memory we supplied to it
	 * was allocated) */
	if (sha1 && !(dup = sha1_dup (sha1)))
		return NULL;

	/* choose mime type based on extension */
	mime = mime_type (filename);

	/* TODO: parse path out of filename. Nodes don't put paths in the
	 *       search result string yet but may in the future */
	if (!(file = share_new (gnutella_p, NULL, 0, filename,
	                        mime, size, 0)))
	{
		free (dup);
		return NULL;
	}

	if (dup && !share_hash_set (file, "SHA1", dup, SHA1_BINSIZE))
	{
		gt_share_unref (file);
		free (dup);

		return NULL;
	}

	if (!(share = gt_share_new_data (file, index)))
	{
		gt_share_unref (file);
		return NULL;
	}

	share_insert_data (file, gnutella_p->name, share);

	return file;
}

void gt_share_free (FileShare *file)
{
	gt_share_free_data (file, share_lookup_data (file, gnutella_p->name));
	share_remove_data (file, gnutella_p->name);

	share_free (file);
}
