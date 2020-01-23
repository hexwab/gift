/*
 * $Id: share_file.c,v 1.20 2003/03/06 00:57:07 jasta Exp $
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

#include "gift.h"

#include "mime.h"

#include "share_file.h"
#include "meta.h"

/*****************************************************************************/

static char *calculate_hpath (char *root, size_t root_len, char *filename)
{
	/* no root means we dont need an hpath */
	if (!root)
		return NULL;

	/* "" will trigger this, but its not exactly iteration so who cares */
	if (!root_len)
		root_len = strlen (root);

	return (filename + root_len);
}

static int sdata_new (ShareData *sdata, char *root, size_t root_len,
                      char *path, time_t mtime)
{
	/* i should probably have root handled as just a length into path, oh
	 * well */
	if (!root || root_len)
		sdata->root = STRDUP_N (root, root_len);
	else
		sdata->root = STRDUP ("");

	sdata->path  = STRDUP (path);
	sdata->hpath = calculate_hpath (sdata->root, root_len, sdata->path);
	sdata->mtime = mtime;
	sdata->meta  = NULL;

	sdata->hash.hashes = NULL;
	sdata->hash.hash   = NULL;

	return TRUE;
}

FileShare *share_new (Protocol *p, char *root, size_t root_len,
                      char *path, char *mime, off_t size, time_t mtime)
{
	FileShare *file;

	if (!(file = malloc (sizeof (FileShare))))
		return NULL;

	if (!sdata_new (SHARE_DATA(file), root, root_len, path, mtime))
	{
		free (file);
		return NULL;
	}

	if (mime)
		file->mime = mime_type_lookup (mime);
	else
		file->mime = NULL;

	file->size = size;
	file->p    = p;
	file->data = NULL;

	file->ref = 0;
	share_ref (file);

	return file;
}

/*****************************************************************************/

static void sdata_free (ShareData *sdata)
{
	if (!sdata)
		return;

	free (sdata->root);
	free (sdata->path);

	/* don't free (sdata); on purpose */
}

void share_free (FileShare *file)
{
	if (!file)
		return;

	if (!file->p)
		dataset_clear ((Dataset *) file->data);

	share_hash_clear (file);
	meta_clear (file);

	sdata_free (SHARE_DATA(file));

	free (file);
}

/*****************************************************************************/

static unsigned short change_ref (FileShare *file, int change)
{
	if (!file)
		return 0;

	if (file->ref == 0 && change < 0)
	{
		GIFT_WARN (("FileShare %p requested negative ref count!", file));
		return 0;
	}

	file->ref += change;

	return file->ref;
}

unsigned short share_ref (FileShare *file)
{
	return change_ref (file, 1);
}

unsigned short share_unref (FileShare *file)
{
	unsigned short ref;

	if (!(ref = change_ref (file, -1)))
		share_free (file);

	return ref;
}

/*****************************************************************************/

void *share_lookup_data (FileShare *file, char *protocol)
{
	if (!file || !protocol)
		return NULL;

	/* if this is locally shared, lookup the data in a Dataset */
	if (!file->p)
		return dataset_lookup ((Dataset *) file->data, protocol, STRLEN_0 (protocol));

	/* otherwise, to conserve memory it's actually the data itself */
	return file->data;
}

void share_insert_data (FileShare *file, char *protocol, void *data)
{
	if (!file || !protocol)
		return;

	if (!file->p)
	{
		dataset_insert ((Dataset **) &file->data,
		                protocol, STRLEN_0 (protocol), data, 0);
		return;
	}

	/* see above for more info on why i do it this way */
	file->data = data;
}

void share_remove_data (FileShare *file, char *protocol)
{
	if (!file || !protocol)
		return;

	if (!file->p)
	{
		dataset_remove ((Dataset *) file->data, protocol, STRLEN_0 (protocol));
		return;
	}

	file->data = NULL;
}

/*****************************************************************************/

int share_complete (FileShare *file)
{
	if (!file || !SHARE_DATA(file))
		return FALSE;

	if (SHARE_DATA(file)->path && file->size)
	{
		/* absolute path only */
		if (SHARE_DATA(file)->path[0] != '/')
			return FALSE;
	}

	return TRUE;
}
