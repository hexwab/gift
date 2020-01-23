/*
 * share_file.c
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

#include "gift.h"

#include "mime.h"

#include "share_file.h"

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

static char *calculate_ext (char *path)
{
	char *ext;

	if (!path)
		return NULL;

	if ((ext = strrchr (path, '.')))
		ext++;

	return ext;
}

static ShareData *sdata_new (char *root, size_t root_len, char *path,
                             char *md5, time_t mtime)
{
	ShareData *sdata;

	if (!(sdata = malloc (sizeof (ShareData))))
		return NULL;

	/* i should probably have root handled as just a length into path, oh
	 * well */
	sdata->root  = STRDUP_N (root, root_len);
	sdata->path  = STRDUP (path);
	sdata->hpath = calculate_hpath (sdata->root, root_len, sdata->path);

	sdata->md5   = STRDUP (md5);

	return sdata;
}

FileShare *share_new (Protocol *p, int p_notify, char *root, size_t root_len,
					  char *path, char *md5, unsigned long size, time_t mtime)
{
	FileShare *file;
	char      *ext;

	if (!(file = malloc (sizeof (FileShare))))
		return NULL;

	file->ref     = 0;
	file->flushed = FALSE;

	file->sdata   = sdata_new (root, root_len, path, md5, mtime);

	ext           = calculate_ext (file->sdata->hpath ? file->sdata->hpath : file->sdata->path);
	file->type    = mime_type     (file->sdata->path, ext);
	file->mtime   = mtime;

	file->size    = size;
	file->p       = p;
	file->data    = NULL;

	share_ref (file);

	if (p_notify)
		protocol_send (p, PROTOCOL_SHARE, PROTOCOL_SHARE_ADD, file, NULL);

	return file;
}

/*****************************************************************************/

static void sdata_free (ShareData *sdata)
{
	if (!sdata)
		return;

	free (sdata->root);
	free (sdata->path);
	free (sdata->md5);

	free (sdata);
}

void share_free (FileShare *file)
{
	assert (file);

	protocol_send (file->p, PROTOCOL_SHARE, PROTOCOL_SHARE_REMOVE, file, NULL);

	if (!file->p)
		list_dataset_clear ((ListDataset **) &file->data);

	sdata_free (file->sdata);

	free (file);
}

/*****************************************************************************/

static unsigned short change_ref (FileShare *file, int change)
{
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
		return list_dataset_lookup ((ListDataset **) &file->data, protocol);

	/* otherwise, to conserve memory it's actually the data itself */
	return file->data;
}

void share_insert_data (FileShare *file, char *protocol, void *data)
{
	if (!file || !protocol)
		return;

	if (!file->p)
	{
		list_dataset_insert ((ListDataset **) &file->data, protocol, data);
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
		list_dataset_remove ((ListDataset **) &file->data, protocol);
		return;
	}

	file->data = NULL;
}

/*****************************************************************************/

int share_complete (FileShare *file)
{
	if (!file || !file->sdata)
		return FALSE;

	if (file->sdata->path && file->sdata->md5 && file->size)
	{
		if (file->sdata->path[0] != '/')
			return FALSE;
	}

	return TRUE;
}
