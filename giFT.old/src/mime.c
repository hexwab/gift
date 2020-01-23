/*
 * mime.c
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

#include <ctype.h>

#include "gift.h"

#include "file.h"
#include "parse.h"

/*****************************************************************************/

struct _mime_type
{
	char *type;
	char *desc;
	char *ext;
};

Dataset *mime_types = NULL;

/*****************************************************************************/

static void load_types ()
{
	FILE *f;
	char *buf = NULL;
	char *ptr, *filename;

	filename = malloc (strlen (DATA_DIR) + 12); /* /mime.types\0 */
	sprintf (filename, "%s/mime.types", DATA_DIR);

	if (!(f = fopen (filename, "r")))
	{
		GIFT_ERROR (("failed to open %s", filename));
		return;
	}

	free (filename);

	while (file_read_line (f, &buf))
	{
		struct _mime_type *mime_info;
		char *ext, *exts0, *exts;
		char *type;

		ptr = buf;

		trim_whitespace (ptr);

		if (*ptr == '#')
			continue;

		/* break type and extension list */
		type = string_sep_set (&ptr, " \t");

		if (!ptr || !ptr[0])
			continue;

		trim_whitespace (ptr);

		/* create a unique entry for each type (to satisfy the hash table
		 * lookup) */
		exts0 = exts = STRDUP (ptr);

		while ((ext = string_sep (&exts, " ")))
		{
			mime_info = malloc (sizeof (struct _mime_type));
			mime_info->type = STRDUP (type);
			mime_info->desc = NULL;
			mime_info->ext  = STRDUP (ext);

			dataset_insert (mime_types, ext, mime_info);
		}

		free (exts0);
	}

	fclose (f);
}

/*****************************************************************************/

/* locates the appropriate mime type based on extension
 * NOTE: supplying ext as non-NULL is for optimization purposes only.  it is
 * not a requirement */
char *mime_type (char *file, char *ext)
{
	struct _mime_type *mime_info;
	char *ext_local;

	if (!mime_types)
		load_types ();

	if (ext)
		ext_local = ext;
	else
	{
		/* no extension supplied, calculate one */
		if ((ext_local = strrchr (file, '.')))
			ext_local++;
	}

	/* no known extension, text/plain */
	if (!ext_local ||
		!(mime_info = dataset_lookup (mime_types, ext_local)))
	{
		return "text/plain";
	}

	return mime_info->type;
}

/* TODO - skip id3 tags and headers */
static size_t mime_size (char *file, char *mime)
{
	size_t size = 0;

	file_exists (file, &size, NULL);

	return size;
}

/*****************************************************************************/

FILE *mime_open (char *file, char *mode, char **mime, size_t *size)
{
	FILE *f;

	assert (file);

	if (mime)
		*mime = mime_type (file, NULL);

	if (size)
		*size = mime_size (file, *mime);

	f = fopen (file, mode);

	return f;
}
