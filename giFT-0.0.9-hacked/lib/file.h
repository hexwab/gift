/*
 * $Id: file.h,v 1.22 2003/06/04 17:22:49 jasta Exp $
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

#ifndef __FILE_H
#define __FILE_H

/*****************************************************************************/

/**
 * @file file.h
 *
 * @brief File/disk routines
 *
 * Abstraction from commonly used file manipulation routines.  Also includes
 * portability wrappers for readdir and friends.
 */

/*****************************************************************************/

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#if HAVE_LINUX_LIMITS_H
# include <linux/limits.h>
#endif

#ifdef WIN32
# ifndef PATH_MAX
#  define PATH_MAX 4096
# endif
  typedef int ssize_t;
#endif /* WIN32 */

/*****************************************************************************/

#if defined (HAVE_DIRENT_H)
# include <dirent.h>
#elif defined (WIN32)

/* implement dir functions for Windows, mingw already defines them */
#include <io.h>
#include <stddef.h>

struct dirent
{
	char *d_name;
};

typedef struct _dir
{
	long                findfirst_handle;
	struct _finddata_t *finddata;
	struct dirent      *direntry;
	struct dirent      *firstentry;
} DIR;

#endif /* HAVE_DIRENT_H */

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Determine if a file exists on disk, and is a regular file.
 *
 * @param file  Pathname to stat.
 *
 * @return Boolean value indicating whether or not the path exists and is a
 *         regular file.
 */
BOOL file_exists (const char *file);

/**
 * Similar to ::file_exists, except that the path is checked to be a
 * directory instead of a regular file.
 */
BOOL file_direxists (const char *path);

/**
 * Wrapper around stat to catch stupid errors.
 *
 * @return Indicates error conditions using FALSE, instead of negative
 *         values.
 */
BOOL file_stat (const char *file, struct stat *stbuf);

/**
 * Retrieve the directory element from a fully qualified path.
 */
char *file_dirname (char *file);

/**
 * Retrieve the last element of a fully qualified path.
 */
char *file_basename (char *file);

/**
 * Secure a path by stripping '.' and '..' (and /.+/ on Windows) and failing
 * if the path does not begin with a slash.
 *
 * @return Duplicated memory containing the new secure path.
 */
char *file_secure_path (char *path);

/**
 * @brief Reads a line from an opened file
 *
 * This function is designed to be used in a while loop as shown:
 *
 * \code
 * char *line = NULL;
 *
 * // open file
 *
 * while (file_read_line (f, &line))
 * {
 *     // manipulate line...
 * }
 *
 * // close file
 * \endcode
 *
 * @param f      Already opened FILE pointer
 * @param outbuf
 *   Reference to a local character pointer.  Make sure the value held at the
 *   address is initialized to NULL.
 *
 * @return Dynamically allocated pointer to the data at this line.  Subsequent
 * calls to file_read_line will free this data, so don't modify the pointer's
 * value (this does not mean that you cannot modify the memory at this address,
 * of course).
 */
char *file_read_line (FILE *f, char **outbuf);

/*****************************************************************************/

/**
 * Expand a shell-like path to a fully qualified path.
 *
 * @return Expanded path.
 */
char *file_expand_path (char *path);

/**
 * Similar to mkdir -p, except that the path is expected to have a trailing
 * file entry which will not be created as a directory.
 *
 * @retval TRUE  Path exists.
 * @retval FALSE Unable to fully construct the supplied path.
 */
int file_create_path (char *path, int mode);

/**
 * Identical to mkdir -p.  All path elements will be created with the
 * supplied mode.
 *
 * @return Boolean success or failure.
 */
BOOL file_mkdir (char *path, int mode);

/*****************************************************************************/

/**
 * Unused.
 */
FILE *file_temp (char **out, char *module);

/*****************************************************************************/

/**
 * Identical to rm -rf.
 *
 * @return Boolean success or failure.
 */
BOOL file_rmdir (char *path);

/**
 * @brief Copy a file.
 *
 * Manually copy the contents of src to dst.  dst will be created if it
 * does not exist.
 *
 * @param src Source location.
 * @param dst Destination location.
 *
 * @retval TRUE  Success, file has been copied
 * @retval FALSE Unable to copy the file, partial writes to dst will be
 *               unlinked
 */
int file_cp (char *src, char *dst);

/**
 * @brief Move a file.
 *
 * This function will attempt to use rename () first, and cp/unlink if that
 * fails.
 *
 * @param src Original location.
 * @param dst New location.
 *
 * @retval TRUE  Success, file has been moved.
 * @retval FALSE Unable to completely move the file, src file left unlinked.
 */
int file_mv (char *src, char *dst);

/**
 * @brief Slurp in a file.
 *
 * This function creates a massive linear character array.  Very unwise unless
 * you really know what you're doing.
 *
 * @param path Location of the file on disk you wish to slurp.
 * @param data Memory location to store the allocated array.
 * @param len  Memory location to store the allocation size.
 *
 * @retval TRUE  File was slurped.
 * @retval FALSE Failed to open path or allocate data.
 */
int file_slurp (char *path, char **data, unsigned long *len);

/**
 * Dumps the supplied data to a file on disk.
 *
 * @param path Location of the file to write.
 * @param data Data you wish to dump to the file.
 * @param len  Length of the supplied data segment.
 *
 * @retval TRUE  File has been written successfully.
 * @retval FALSE Failed to create the specified file, or no data was supplied.
 */
int file_dump (char *path, char *data, unsigned long len);

/*****************************************************************************/

/**
 * Wrapper for fopen.
 */
FILE *file_open (char *path, char *mode);

/**
 * Wrapper for fclose.
 */
int file_close (FILE *f);

/**
 * Wrapper for ::file_close which will set the value pointed to by fhandle
 * to NULL.
 */
#define FILE_CLOSE(fhandle) \
	do { \
		file_close (*(fhandle)); \
		*(fhandle) = NULL; \
	} while (0)

/**
 * Wrapper for unlink.
 */
int file_unlink (char *path);

/*****************************************************************************/

/**
 * Portable opendir.
 */
DIR *file_opendir (char *dir);

/**
 * Portable readdir.
 */
struct dirent *file_readdir (DIR *dh);

/**
 * Portable closedir.
 */
int file_closedir (DIR *dh);

/*****************************************************************************/

/**
 * @brief Converts a UNIX-style path to a host specific path.
 *
 * For example, on Windows /C/Program Files/giFT becomes
 * C:\Program Files\giFT.
 *
 * @return The host formatted path.
 */
char *file_host_path (char *gift_path);

/**
 * @brief Converts a host specific path to a UNIX-style path.
 *
 * For example, on Windows C:\Program Files\giFT becomes
 * /C/Program Files/giFT.
 *
 * @return UNIX style path.
 */
char *file_unix_path (char *host_path);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __FILE_H */
