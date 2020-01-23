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

#ifdef WIN32
# include <direct.h> /* mkdir () */
# include <io.h>     /* mktemp() */
#endif /* WIN32 */

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

int file_stat (int fd, off_t *size, time_t *mtime)
{
	struct stat st;

	if (fd < 0)
		return FALSE;

	if (fstat (fd, &st))
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

/* "secure" a path by requiring it be absolute and contain no directory entries
 * matching '.' or '..' */
char *file_secure_path (char *path)
{
	char *s_path;
	char *s_path0;

	if (!path)
		return NULL;

	/* THIS IS TEMPORARY! */
#ifndef WIN32
	if (*path != '/')
		return NULL;
#else /* WIN32 */
	if (path[1] != ':')
		return NULL;
#endif /* !WIN32 */

	/* get our own memory here ;) */
	s_path0 = s_path = path = STRDUP (path);

	while ((s_path = string_sep_set (&path, "\\/")))
	{
#ifndef WIN32
		if (!strcmp (s_path, ".") || !strcmp (s_path, ".."))
#else
		if (*s_path == '.')
#endif
		{
			/* TODO -- strmove should do this, damnit */
			if (path)
				strmove (s_path, path);
			else
				*s_path = 0;

			/* rewind so that we read the "next" path, which just so happens
			 * to be s_path right now :) */
			path = s_path;

			continue;
		}

		/* unset the \0
		 * NOTE: this is safe because path will be incremented by at least
		 * one each successful iteration */
		if (path)
			*(path - 1) = '/';
	}

	return s_path0;
}

/*****************************************************************************/

/* wrap fgets() to allow for next_arg to manipulate the string */
char *file_read_line (FILE *f, char **outbuf)
{
	char buf[1024];

	if (!f || !outbuf)
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

char *file_expand_path (char *path)
{
	static char newp[PATH_MAX];
	char *p1;

	if (!path)
		return NULL;

	path = strdup (path);

	/* hack the ~ out */
	if ((p1 = strchr (path, '~')) != NULL)
	{
		char *p0 = path;

		*p1++ = 0;

		if (*p0)
			snprintf (newp, sizeof (newp) - 1, "%s%s%s", p0,
			          platform_home_dir (), p1);
		else
			snprintf (newp, sizeof (newp) - 1, "%s%s", platform_home_dir (), p1);
	}
	else
		snprintf (newp, sizeof (newp) - 1, "%s", path);

	free (path);

	return newp;
}

/* portable mkdir -p
 * return identical to file_exists */
int file_create_path (char *p)
{
	char *p0, *p1;
	int   ret;

	if (!p)
		return FALSE;

	/* must make sure we're working w/ our own memory here */
	p0 = p1 = p = strdup (p);

	/* skip leading / */
	while (*p1 == '/')
		p1++;

	while ((p1 = strchr (p1, '/')) != NULL)
	{
		*p1 = 0;

		/* Really should stat() here, but if the directory already exists,
		 * mkdir() will just fail anyway */
#ifndef WIN32
		mkdir (p0, 0755);
#else /* WIN32 */
		mkdir (p0);
#endif /* !WIN32 */

		/* ignore stupid shit like /path////////dir */
		*p1 = '/';
		while (*p1 == '/')
			p1++;
	}

	ret = file_exists (p0, NULL, NULL);

	free (p);

	return ret;
}

/*****************************************************************************/

/* open a unique temporary file
 * NOTE: this is unused and needs completion */
FILE *file_temp (char **out, char *module)
{
	char  buf[PATH_MAX];
	char *path;
	FILE *ret;
#ifndef WIN32
	int   fd;
#endif /* !WIN32 */

	snprintf (buf, sizeof(buf) - 1, "%s",
	          gift_conf_path ("%s/%s.XXXX", module, module));

#ifndef WIN32
	if ((fd = mkstemp (buf)) < 0)
		return NULL;

	path = buf;

	ret = fdopen (fd, "w");
#else /* WIN32 */
	if (!(path = mktemp (buf)))
		return NULL;

	ret = fopen (path, "wb");
#endif /* !WIN32 */

	if (out)
		*out = STRDUP (path);

	return ret;
}

/*****************************************************************************/

int file_rmdir (char *path)
{
	DIR           *dir;
	struct dirent *d;
	struct stat    st;
	int            ret = TRUE;

	if (!path || !*path)
		return FALSE;

	if (!(dir = file_opendir (path)))
		return FALSE;

	while ((d = file_readdir (dir)))
	{
		char fullpath[PATH_MAX];

		/* skip '.' and '..' */
		if (!strcmp (d->d_name, ".") || !strcmp (d->d_name, ".."))
			continue;

		snprintf (fullpath, sizeof (fullpath) - 1, "%s/%s", path, d->d_name);

		if ((stat (fullpath, &st)) == -1)
		{
			perror ("stat");
			ret = FALSE;
			continue;
		}

		if (S_ISDIR (st.st_mode))
		{
			file_rmdir (fullpath);
		}
		else
		{
			if (unlink (fullpath) == -1)
				perror ("unlink");

			ret = FALSE;
		}
	}

	file_closedir (dir);

	/* delete thyself */
	if ((rmdir (path)))
	{
		perror ("rmdir");
		ret = FALSE;
	}

	return ret;
}

/*****************************************************************************/

int file_mv (char *src, char *dst)
{
	FILE  *src_f;
	FILE  *dst_f;
	char   buf[2048];
	size_t n;

	/* try to optimize w/ single dev links first */
	if (rename (src, dst) != -1)
		return TRUE;

	/* damn.  this may be because of an invalid cross-device link. manually
	 * copy and unlink the source */
	if (!(src_f = fopen (src, "rb")))
	{
		GIFT_ERROR (("unable to open %s (read)", src));
		perror ("fopen");
		return FALSE;
	}

	if (!(dst_f = fopen (dst, "wb")))
	{
		GIFT_ERROR (("unable to open %s (write)", dst));
		perror ("fopen");

		/* leaking fd's is bad */
		fclose (src_f);
		return FALSE;
	}

	while ((n = fread (buf, sizeof (char), sizeof (buf), src_f)) > 0)
	{
		if (fwrite (buf, sizeof (char), n, dst_f) == -1)
		{
			GIFT_ERROR (("unable to write to %s", dst));
			perror ("fwrite");

			/* we need to cleanup and get out */
			fclose (src_f);
			fclose (dst_f);

			/* remove the partially copied file */
			unlink (dst);

			return FALSE;
		}
	}

	/* cleanup */
	fclose (src_f);
	fclose (dst_f);

	/* unlink the source */
	unlink (src);

	return TRUE;
}

/*****************************************************************************/

/* slurp contents of a file (data will be allocated for you) */
int file_slurp (char *file, char **data, unsigned long *len)
{
	FILE          *f;
	char          *f_data;
	off_t          f_len;
	struct stat    st;

	if (!file || !data)
		return FALSE;

	*data = NULL;

	if (len)
		*len = 0;

	if (!(f = fopen (file, "rb")))
	{
		GIFT_ERROR (("Can't open %s", file));
		return FALSE;
	}

	if (fstat (fileno (f), &st) < 0)
	{
		GIFT_ERROR (("Can't stat %s", file));
		fclose (f);
		return FALSE;
	}

	f_len = st.st_size;

	if (!(f_data = malloc (f_len)))
	{
		GIFT_ERROR (("Out of memory"));
		fclose (f);
		return FALSE;
	}

	if (fread (f_data, sizeof (char), f_len, f) != f_len)
	{
		GIFT_ERROR (("failed to read %s", file));
		perror ("fread");
		free (f_data);
		fclose (f);
		return FALSE;
	}

	*data = f_data;

	if (len)
		*len = f_len;

	fclose (f);

	return TRUE;
}

/* write the contents of the data buffer to file */
int file_dump (char *file, char *data, unsigned long len)
{
	FILE *f;

	if (!file || !data)
		return FALSE;

	if (!(f = fopen (file, "wb")))
	{
		GIFT_ERROR (("Can't create %s", file));
		return FALSE;
	}

	if (fwrite (data, sizeof (char), len, f) != len)
	{
		GIFT_ERROR (("Can't write to %s", file));
		perror ("fwrite");
		fclose (f);
		unlink (file);
		return FALSE;
	}

	fclose (f);

	return TRUE;
}

/*****************************************************************************/

/* opendir () */
DIR *file_opendir (char *dir)
{
#ifdef HAVE_DIRENT_H
	return opendir (dir);
#else /* !HAVE_DIRENT_H */
	char path[_MAX_PATH];
	DIR *dh;

	if (!dir)
		return NULL;

	dh = malloc (sizeof (DIR));

	dh->finddata = malloc (sizeof (struct _finddata_t));
	dh->direntry = malloc (sizeof (struct dirent));

	/* glob matching all files */
	_snprintf (path, _MAX_PATH, "%s/%s", dir, "*");

	if ((dh->findfirst_handle = _findfirst (path, dh->finddata)) == -1)
	{
		free (dh->finddata);
		free (dh->direntry);
		free (dh);

		return NULL;
	}

	dh->direntry->d_name = dh->finddata->name;

	return dh;
#endif /* HAVE_DIRENT_H */
}

/* readdir () */
struct dirent *file_readdir (DIR *dh)
{
#ifdef HAVE_DIRENT_H
	return readdir (dh);
#else /* !HAVE_DIRENT_H */
	int rv;

	if (!dh)
		return NULL;

	if ((rv = _findnext (dh->findfirst_handle, dh->finddata)) == -1)
		return NULL;

	dh->direntry->d_name = dh->finddata->name;

	return dh->direntry;
#endif /* HAVE_DIRENT_H */
}

/* closedir () */
int file_closedir (DIR *dh)
{
#ifdef HAVE_DIRENT_H
	return closedir (dh);
#else /* !HAVE_DIRENT_H */
	int rv;

	if (!dh)
		return FALSE;

	rv = _findclose (dh->findfirst_handle);

	free (dh->finddata);
	free (dh->direntry);
	free (dh);

	return rv;
#endif /* HAVE_DIRENT_H */
}
