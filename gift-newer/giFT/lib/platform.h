/*
 * platform.h
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

#ifndef __PLATFORM_H
#define __PLATFORM_H

/*****************************************************************************/

#include <errno.h>
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

typedef struct
{
	int   fd;
	void *udata;
} SubprocessData;

typedef int (*ParentFunc) (int read_fd, void *udata);

#ifndef WIN32

#define SUBPROCESS(func) int func(void *param)
typedef int (*ChildFunc) (void *udata);

#else /* WIN32 */

#define SUBPROCESS(func) DWORD WINAPI func(LPVOID param)
typedef unsigned (__stdcall *ChildFunc) (void *udata);

#endif /* !WIN32 */

/*****************************************************************************/
/* big sloppy mess of platform-dep code */

/* misc changes */
#ifndef __GNUC__
# define __PRETTY_FUNCTION__ "???"
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

#ifdef WIN32
/* not needed: #define strcasecmp(s1, s2) stricmp(s1, s2) */
/* mingw32 port needs these: */
# ifndef vsnprintf
#  define vsnprintf  _vsnprintf
# endif
# ifndef snprintf
#  define snprintf   _snprintf
# endif
#endif /* WIN32 */

/*****************************************************************************/

char *platform_data_dir       ();
char *platform_plugin_dir     ();
char *platform_local_dir      ();
char *platform_home_dir       ();

int   platform_init           ();
void  platform_cleanup        ();

void  platform_gettimeofday   (struct timeval *tv, void *unused);
int   platform_child_proc     (ChildFunc c_func, ParentFunc p_func, void *udata);
char *platform_path_to_host   (char *gift_path);
char *platform_path_to_gift   (char *host_path);
char *platform_version        ();
char *platform_last_error     ();
char *platform_last_net_error ();

/* platform_net_perror() emulates perror() in *nix.
 * In win32, it emulates perror but uses WSAGetLastError() to generate error
 * string.
 */
void  platform_net_perror     (const char *msg);

/*****************************************************************************/

#endif /* __PLATFORM_H */
