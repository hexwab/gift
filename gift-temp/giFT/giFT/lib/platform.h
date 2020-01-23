/*
 * $Id: platform.h,v 1.17 2003/02/09 22:54:33 jasta Exp $
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

#ifndef __PLATFORM_H
#define __PLATFORM_H

/*****************************************************************************/

/**
 * @file platform.h
 *
 * @brief Platform specific routines.
 *
 * Contains routines that are specific to a platform (*nix, Windows, etc).
 * You should be aware that you are expected to call \ref platform_init
 * before using any routines held within libgiFT.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#include <sys/stat.h> /* stat macros */

#ifdef WIN32
# ifndef FD_SETSIZE
#  define FD_SETSIZE 1024
# endif
# include <winsock2.h>	/* include *before* windows */
# include <windows.h>
#endif /* !WIN32 */

/*****************************************************************************/
/* definitions for platform_child_proc */

struct _sproc_data;

#define PARENT_FUNC(func) int func (SubprocessData *subproc, void *udata)
typedef int (*ParentFunc) (struct _sproc_data *sdata, void *udata);

#define CHILD_FUNC(func) int func (SubprocessData *subproc, void *udata)
typedef int (*ChildFunc) (struct _sproc_data *sdata, void *udata);

#ifndef WIN32
# define SUBPROCESS(func) int func (void *param)
#else /* WIN32 */
# define SUBPROCESS(func) DWORD WINAPI func (LPVOID param)
#endif /* !WIN32 */

/**
 * Structure containing data passed between parent and child processes.
 */
typedef struct _sproc_data
{
	int         fd;
	ChildFunc   cfunc;
	ParentFunc  pfunc;
	char       *data;
	size_t      len;
	size_t      data_len;
	void       *udata;
} SubprocessData;

/*****************************************************************************/

#ifndef __GNUC__
# define __PRETTY_FUNCTION__ ""
#endif

#ifndef S_ISDIR
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISFIFO
# define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif

#ifndef S_ISCHR
# define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif

#ifndef S_ISBLK
# define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif

#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

/*****************************************************************************/

/**
 * Contains installed data, such as mime.types.
 *
 * UNIX:    this is defined by configure as DATA_DIR
 * Windows: The plugin_dir + /data
 */
char *platform_data_dir ();

/**
 * Contains all dynamically loaded plugins that this giFT installation has
 * available.
 *
 * UNIX:    this is defined by configure as PLUGIN_DIR
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_LOCAL_MACHINE\Software\giFT\giFT\instpath registry key
 *          2. Directory where giFT started from
 *          3. Current directory
 */
char *platform_plugin_dir ();

/**
 * Contains the user's 'home' directory.
 *
 * UNIX:    assigned to $HOME or /home/$USER
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_CURRENT_USER\Software\giFT\giFT\instpath registry key
 *          2. %HOME% directory, if %HOME% is defined in the environment.
 *          3. Directory where giFT started from.
 *          4. Current directory.
 */
char *platform_home_dir ();

/**
 * Contains all user preferences (.conf files).
 *
 * UNIX:    assigned to $HOME/.giFT
 * Windows: Searches for the first valid directory in:
 *          1. home_dir
 *          2. home_dir + /.giFT
 */
char *platform_local_dir ();

/**
 * Platform specific initialization.
 *
 * \ref platform_init must be called before calling any more functions
 * in this library.
 *
 * @return TRUE if successful, FALSE if not.
 */
int platform_init ();

/**
 * Platform specific cleanup.  Frees any memory that was allocated by
 * \ref platform_init.
 *
 * \ref platform_cleanup should be called on program termination.
 */
void platform_cleanup ();

/**
 * Emulates *nix's gettimeofday() function.
 *
 * @param tv time structure.
 * @param unused not used.
 */
void platform_gettimeofday (struct timeval *tv, void *unused);

/**
 * Spawn's a child process. In *nix, fork() is used, in Windows, a thread is
 * used.
 *
 * @param c_func Pointer to child function.
 * @param p_func Pointer to parent function.
 * @param udata
 *
 * @return TRUE if successful, FALSE if not.
 */
int platform_child (ChildFunc cfunc, ParentFunc pfunc, void *udata);

/**
 * Send a message to a parent process from a child process
 *
 * @param sdata SubprocessData of the child context
 * @param data message to send
 * @param len size of the message to send
 *
 * @return see send ()
 */
int platform_child_sendmsg (SubprocessData *sdata, char *msg, size_t len);

/**
 * Receive a message from a child process in a parent process
 *
 * The data is returned in sdata->data, and the length in
 * sdata->len.
 *
 * @param sdata SubprocessData of the child process
 *
 * @return see recv ()
 */
int platform_child_recvmsg (SubprocessData *sdata);

/**
 * @return a HTTP_USER_AGENT string describing the giFT version and OS version.
 */
char *platform_version ();

/**
 * @return the error code of the last system error.
 *
 * On Windows, GetLastError() is returned.
 */
unsigned long platform_errno ();

/**
 * @return the error message of the last system error.
 *
 * On Windows, FormatMessage() and GetLastError() are used.
 */
char *platform_error ();

/**
 * @return the error code of the last socket error.
 *
 * This function is required as Windows has different functions for socket and
 * non-socket errors.
 */
unsigned long platform_net_errno ();

/**
 * This function is required as Windows has different functions for socket and
 * non-socket errors.
 *
 * @return the error message of the last socket error.
 */
char *platform_net_error ();

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __PLATFORM_H */
