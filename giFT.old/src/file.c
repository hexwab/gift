/*
 * file.c
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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gift.h"

#include "file.h"
#include "conf.h"

/*****************************************************************************/

int file_exists (char *file, size_t *size, time_t *mtime)
{
	struct stat st;

	if (!file)
		return FALSE;

	if (stat (file, &st))
		return FALSE;

	if (size)
		*size = st.st_size;
	if (mtime)
		*mtime = st.st_mtime;

	return TRUE;
}

/*****************************************************************************/

char *file_basename (char *file)
{
	char *ptr;

	if (!file)
		return NULL;

	if (!(ptr = strrchr (file, '/')))
		return file;

	ptr++;

	/* TODO, make sure its not /full/path/ */

	return ptr;
}

/*****************************************************************************/

/* wrap fgets() to allow for next_arg to manipulate the string */
char *file_read_line (FILE *f, char **outbuf)
{
	char buf[1024];

	if (!f)
		return NULL;

	/* free each iteration...gaurenteed to cleanup after ourselves */
	if (*outbuf)
		free (*outbuf);

	if (fgets (buf, sizeof(buf) - 1, f))
	{
		*outbuf = strdup (buf);
		return *outbuf;
	}

	/* EOF || error */
	return NULL;
}

/*****************************************************************************/

FILE *file_temp (char **out, char *module)
{
#ifdef WIN32
	/* TODO - implement this function */
	GIFT_WARN (("file_temp() not implemented yet in win32 port"));
	return NULL;
#else /* !WIN32 */
	char buf[4096];
	int fd;

	/* TODO - don't use gift_conf someday */
	snprintf(buf, sizeof(buf) - 1, "%s",
	         gift_conf_path("%s/%s.XXXX", module, module));

	fd = mkstemp (buf);
	if (fd < 0)
		return NULL;

	if (out)
	{
		*out = malloc (strlen (buf) + 1);
		strcpy (*out, buf);
	}

	return fdopen (fd, "w");
#endif /* WIN32 */
}
