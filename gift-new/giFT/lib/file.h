/*
 * file.h
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

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

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
#endif /* WIN32 */

/*****************************************************************************/

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else /* !HAVE_DIRENT_H */

/* implement dir functions for Windows, mingw already defines them */
#include <io.h>
#include <stddef.h>

struct dirent
{
	char *d_name;
};

struct _dir
{
	long                findfirst_handle;
	struct _finddata_t *finddata;
	struct dirent      *direntry;
};

typedef struct _dir DIR;

#endif /* HAVE_DIRENT_H*/

/*****************************************************************************/

/**
 * Determine if a file exists on disk
 *
 * @param file  Pathname to stat
 * @param size  Memory location to store the size of the file
 * @param mtime Memory location to store on disk modification time
 *
 * @retval TRUE  File exists
 * @retval FALSE File does not exist
 */
int   file_exists      (char *file, off_t *size, time_t *mtime);

/**
 * Stat an already opened file
 *
 * @see file_exists
 */
int   file_stat        (int fd, off_t *size, time_t *mtime);

/**
 * Retrieve the directory element from a fully qualified path
 */
char *file_dirname     (char *file);

/**
 * Retrieve the last element of a fully qualified path
 */
char *file_basename    (char *file);

/**
 * Secure a path by stripping '.' and '..' (and /.+/ on Windows) and failing
 * if the path does not begin with a slash
 *
 * @return Duplicated memory containing the new secure path
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
char *file_read_line   (FILE *f, char **outbuf);

/*****************************************************************************/

/**
 * Expand a shell-like path to a fully qualified path
 *
 * @return Expanded path
 */
char *file_expand_path (char *path);

/**
 * Identical to mkdir -p
 *
 * @retval TRUE  Path exists
 * @retval FALSE Unable to fully construct the supplied path
 */
int file_create_path (char *path);

/*****************************************************************************/

/**
 * Unused
 */
FILE *file_temp (char **out, char *module);

/*****************************************************************************/

/**
 * Identical to rm -rf
 *
 * @retval TRUE  Success
 * @retval FALSE Failure
 */
int file_rmdir (char *path);

/**
 * @brief Move a file
 *
 * This function will attempt to use rename () first, and cp/unlink if that
 * fails
 *
 * @param src Original location
 * @param dst New location
 *
 * @retval TRUE  Success, file has been moved
 * @retval FALSE Unable to completely move the file, src file left unlinked
 */
int file_mv    (char *src, char *dst);

/**
 * @brief Slurp in a file
 *
 * This function creates a massive linear character array.  Very unwise unless
 * you really know what you're doing.
 *
 * @param path Location of the file on disk you wish to slurp
 * @param data Memory location to store the allocated array
 * @param len  Memory location to store the allocation size
 *
 * @retval TRUE  File was slurped
 * @retval FALSE Failed to open path or allocate data
 */
int file_slurp (char *path, char **data, unsigned long *len);

/**
 * Dumps the supplied data to a file on disk
 *
 * @param path Location of the file to write
 * @param data Data you wish to dump to the file
 * @param len  Length of the supplied data segment
 *
 * @retval TRUE  File has been written successfully
 * @retval FALSE Failed to create the specified file, or no data was supplied
 */
int file_dump  (char *path, char *data, unsigned long len);

/*****************************************************************************/

DIR           *file_opendir  (char *dir);
struct dirent *file_readdir  (DIR *dh);
int            file_closedir (DIR *dh);

/*****************************************************************************/

/**
 * @brief Converts a UNIX-style path to a host specific path
 *
 * For example, on Windows /C/Program Files/giFT becomes
 * C:\Program Files\giFT.
 *
 * @return The host formatted path
 */
char *file_host_path (char *gift_path);

/**
 * @brief Converts a host specific path to a UNIX-style path.
 *
 * For example, on Windows C:\Program Files\giFT becomes
 * /C/Program Files/giFT.
 *
 * @return UNIX style path
 */
char *file_unix_path (char *host_path);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __FILE_H */
