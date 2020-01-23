/*
 * $Id: giftconfig.h.in,v 1.5 2003/06/04 17:22:49 jasta Exp $ -*- C -*-
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

#ifndef __GIFTCONFIG_H
#define __GIFTCONFIG_H

/*****************************************************************************/

/**
 * @file giftconfig.h
 *
 * @brief Compile-time libgiFT environment configuration.
 *
 * This file is produced automagically by autoconf during configure.  It
 * includes some hardcoded features of the basic libgiFT environment as well
 * as some determined by autoconf.
 */

/*****************************************************************************/

#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END   }
#else /* !__really_crappy_language */
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif /* __really_crappy_language */

/*****************************************************************************/

/*
 * Import all the configured features that need to be installed here.
 */
#ifndef _MSC_VER

#ifndef TIME_WITH_SYS_TIME
#define TIME_WITH_SYS_TIME 1
#endif
#ifndef HAVE_SYS_TIME_H
#define HAVE_SYS_TIME_H 1
#endif
#ifndef SIZEOF_SHORT
#define SIZEOF_SHORT 2
#endif
#ifndef SIZEOF_INT
#define SIZEOF_INT 4
#endif
#ifndef SIZEOF_LONG
#define SIZEOF_LONG 4
#endif
#ifndef BOOL
#define BOOL int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef PLUGIN_DIR
#define PLUGIN_DIR "/usr/local/lib/giFT"
#endif
#ifndef DATA_DIR
#define DATA_DIR "/usr/local/share/giFT"
#endif
#ifndef USE_LIBDB
#define USE_LIBDB 1
#endif
#ifndef USE_ZLIB
#define USE_ZLIB 1
#endif
#ifndef GIFT_PACKAGE
#define GIFT_PACKAGE "giFT"
#endif
#ifndef GIFT_VERSION
#define GIFT_VERSION "0.10.0"
#endif
#ifndef HAVE_STDINT_H
#define HAVE_STDINT_H 1
#endif
#ifndef HAVE_INTTYPES_H
#define HAVE_INTTYPES_H 1
#endif
#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H 1
#endif
#ifndef HAVE_DB_H
#define HAVE_DB_H 1
#endif
#ifndef HAVE_DIRENT_H
#define HAVE_DIRENT_H 1
#endif
#ifndef HAVE_DLFCN_H
#define HAVE_DLFCN_H 1
#endif
#ifndef HAVE_FCNTL_H
#define HAVE_FCNTL_H 1
#endif
#ifndef HAVE_GETOPT_H
#define HAVE_GETOPT_H 1
#endif
#ifndef HAVE_STDLIB_H
#define HAVE_STDLIB_H 1
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_SYSLOG_H
#define HAVE_SYSLOG_H 1
#endif
#ifndef HAVE_SYS_MMAN_H
#define HAVE_SYS_MMAN_H 1
#endif
#ifndef HAVE_SYS_STAT_H
#define HAVE_SYS_STAT_H 1
#endif
#ifndef HAVE_SYS_TYPES_H
#define HAVE_SYS_TYPES_H 1
#endif
#ifndef HAVE_LIMITS_H
#define HAVE_LIMITS_H 1
#endif
#ifndef HAVE_LINUX_LIMITS_H
#define HAVE_LINUX_LIMITS_H 1
#endif
#ifndef HAVE_MEMORY_H
#define HAVE_MEMORY_H 1
#endif
/* This is an ugly hack but it works until we figure out a better way to do it
*/
#else /* _MSC_VER */
#include "giftconfig_win32.h"
#endif /* !_MSC_VER */

/*****************************************************************************/

#define LIBGIFT_VERSION                   \
    ((((0) & 0xff) << 16) | \
     (((10) & 0xff) << 8) |  \
     (((0) & 0xff)))
#define LIBGIFT_VERSIONSTR "0.10.0"

/*****************************************************************************/

/* some functions have a leading underscore on windows */
#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
# define snprintf _snprintf
#endif /* !HAVE_SNPRINTF && HAVE__SNPRINTF */

#if !defined(HAVE_VSNPRINTF) && defined(HAVE__VSNPRINTF)
# define vsnprintf _vsnprintf
#endif /* !HAVE_VSNPRINTF && HAVE__VSNPRINTF */

/*****************************************************************************/

#endif /* __GIFTCONFIG_H */
