/*
 * win32/config.h
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

/*
 * Hand coded Windows version of ./configure generated file.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define PACKAGE "giFT"
#define VERSION "0.10.0"

#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STRING_H 1
#undef HAVE_SYS_TIME_H
#define HAVE_TIME_H 1
#undef HAVE_UNISTD_H

#define HAVE_SELECT 1
#define HAVE_SOCKET 1
#define HAVE_STRDUP 1
#define HAVE_STRSTR 1
#define STDC_HEADERS 1

/* define on the command line as the MSVC project files don't yet support */
/* #define USE_DLOPEN 1 */

/* define to use ZLIB compress library */
/* #define USE_ZLIB */

/* These are used!  Don't remove! -Ross */
#define PATH_SEP_CHAR ';'
#define PATH_SEP_STR ";"
/* These aren't currently used. */
#if 0
#define DIR_SEP_CHAR '/'
#define DIR_SEP_STR "/"
#endif

/* speeds compiles as not *every* header is included automatically */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

/* MSVC specifics */
#ifdef _MSC_VER
/* inline appears to be broken in MSVC 6, so
# define inline __inline
fails on us, so we're left with:
*/
# define inline
# define strcasecmp(s1, s2) _stricmp(s1, s2)
# define strncasecmp(s1, s2, count) _strnicmp(s1, s2, count)

# ifndef vsnprintf
#  define vsnprintf  _vsnprintf
# endif
# ifndef snprintf
#  define snprintf   _snprintf
# endif
#endif /* _MSC_VER */

/* if _WALL is defined, then set the warning level to 3 */
#ifdef _WALL
# ifdef _MSC_VER
#  pragma warning(push, 3)
# endif
#else
/* if _WALL is not defined, then turn off disabled warnings */
# ifdef _MSC_VER
/* warning C4018: '>=' : signed/unsigned mismatch */
#  pragma warning(disable : 4018)
/* warning C4142: benign redefinition of type */
#  pragma warning(disable : 4142)
/* warning C4244: '=' : conversion from 'long ' to 'unsigned short ',
 possible loss of data */
#  pragma warning(disable : 4244)
/* warning C4761: integral size mismatch in argument; conversion supplied */
#  pragma warning(disable : 4761)
# endif
#endif

#endif /* __CONFIG_H */
