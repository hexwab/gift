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

/* Name of package */
#define PACKAGE "giFT"

/* Version number of package */
#define VERSION "0.10.0"

/* #define PLUGIN_DIR "/usr/local/lib/giFT" */
/* #define DATA_DIR "/usr/local/share/giFT" */

/* define to link protocols dynamically as .DLLs, undef to link statically */
/* #define USE_DLOPEN 1 */

/* define to use ZLIB compress library */
/* #define USE_ZLIB */

/* Define to 1 if you have the <dirent.h> header file. */
/* #define HAVE_DIRENT_H 1 */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #define HAVE_DLFCN_H 1 */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
/* #define HAVE_INTTYPES_H 1 */

/* Define to 1 if you have the `nsl' library (-lnsl). */
/* #define HAVE_LIBNSL 1 */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the `z' library (-lz). */
/* #define HAVE_LIBZ 1 */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/limits.h> header file. */
/* #define HAVE_LINUX_LIMITS_H 1 */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
/* #define HAVE_STDINT_H 1 */

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
/* #define HAVE_STRINGS_H 1 */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the <syslog.h> header file. */
/* #define HAVE_SYSLOG_H 1 */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
/* #define HAVE_SYS_TIME_H 1 */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #define HAVE_SYS_WAIT_H 1 */

/* Define to 1 if you have the <unistd.h> header file. */
/* #define HAVE_UNISTD_H 1 */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of a `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
/* #define TIME_WITH_SYS_TIME 1 */

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/*
 * priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).  Both the priorities and the facilities map roughly
 * one-to-one to strings in the syslogd(8) source code.  This mapping is
 * included in this file.
 *
 * priorities (these are ordered)
 */
#define LOG_EMERG   0 /* system is unusable */
#define LOG_ALERT   1 /* action must be taken immediately */
#define LOG_CRIT    2 /* critical conditions */
#define LOG_ERR     3 /* error conditions */
#define LOG_WARNING 4 /* warning conditions */
#define LOG_NOTICE  5 /* normal but significant condition */
#define LOG_INFO    6 /* informational */
#define LOG_DEBUG   7 /* debug-level messages */

/* speeds compiles as not *every* header is included automatically */
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN 1
#endif

/* MSVC specifics */
#ifdef _MSC_VER
# define strcasecmp(s1, s2) _stricmp(s1, s2)
# define strncasecmp(s1, s2, count) _strnicmp(s1, s2, count)
# ifndef vsnprintf
#  define vsnprintf _vsnprintf
# endif
# ifndef snprintf
#  define snprintf  _snprintf
# endif

/* from http://www.codeproject.com/tips/aggressiveoptimize.asp */

# ifdef NDEBUG
/* /Og (global optimizations), /Os (favor small code), /Oy (no frame pointers) */
#  pragma optimize("gsy",on)
#   pragma comment(linker,"/RELEASE")
/* Note that merging the .rdata section will result in LARGER exe's if you using
 * MFC (esp. static link). If this is desirable, define _MERGE_RDATA_ in your
 * project. */
#  ifdef _MERGE_RDATA_
#   pragma comment(linker,"/merge:.rdata=.data")
#  endif // _MERGE_RDATA_
#  ifdef _MAX_COMPRESSION_
#   pragma warning( disable : 4078 )
/* these linker commands cause the following warning:
 * LINK : warning LNK4078: multiple ".data" sections found with different
 * attributes (60000020) */
#   pragma comment(linker,"/merge:.text=.data")
#   pragma comment(linker,"/merge:.reloc=.data")
#  endif /* MAX_COMPRESSION */
#  if _MSC_VER >= 1000
/* Only supported/needed with VC6; VC5 already does 0x200 for release builds.
 * Totally undocumented! And if you set it lower than 512 bytes, the program crashes.
 * Either leave at 0x200 or 0x1000 */
#   pragma comment(linker,"/FILEALIGN:0x200")
#  endif /* _MSC_VER >= 1000 */
# endif /* NDEBUG */
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
