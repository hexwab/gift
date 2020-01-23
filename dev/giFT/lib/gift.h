/*
 * gift.h
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

#ifndef __GIFT_H
#define __GIFT_H

/*****************************************************************************/

/**
 * \mainpage libgiFT documentation
 *
 * \todo This.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/* #define GC_LEAK_DETECT */
/* #define TCG_LEAK_DETECT */

#ifndef LOG_PFX
# define LOG_PFX "[giFT    ] "
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
# include <time.h>
# include <sys/time.h>
#else /* !TIME_WITH_SYS_TIME */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif /* TIME_WITH_SYS_TIME */

/*****************************************************************************/

/* do not change the order here, or you'll break the build */
#include "giftconfig.h"
#include "platform.h"
#include "memory.h"
#include "parse.h"

/* #include "hash.h" */
#include "dataset.h"
#include "list.h"
#include "list_queue.h"
#include "list_lock.h"
/* #include "list_dataset.h" */
#include "tree.h"

#include "connection.h"
#include "protocol.h"
#include "interface.h"
#include "if_event.h"

#include "conf.h"
#include "log.h"

/*****************************************************************************/

#ifdef TCG_LEAK_DETECT
# include <tcgprof.h>
#endif

#ifdef GC_LEAK_DETECT
# include "leak_detector.h"
static char *GC_strdup (char *x) { char *str = GC_malloc (strlen (x)); strcpy (str, x); return str; }
# define strdup(x) GC_strdup(x)
#endif

/*****************************************************************************/

#ifndef TRUE
# define TRUE 1
#endif

#ifndef FALSE
# define FALSE 0
#endif

/*****************************************************************************/

#ifndef MIN
# define MIN(x,y)              (((x)<(y))?(x):(y))
#endif

#ifndef MAX
# define MAX(x,y)              (((x)>(y))?(x):(y))
#endif

#ifndef CLAMP
# define CLAMP(value,min,max)  (((value)<(min))?(min):(((value)>(max))?(max):(value)))
#endif

#ifndef INTCMP
# define INTCMP(x,y)           (((x)>(y))?1:(((x)<(y))?-1:0))
#endif

/*****************************************************************************/

/* some functions have a leading underscore on windows */
#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
#define snprintf _snprintf
#endif /* !HAVE_SNPRINTF && HAVE__SNPRINTF */

#if !defined(HAVE_VSNPRINTF) && defined(HAVE__VSNPRINTF)
#define vsnprintf _vsnprintf
#endif /* !HAVE_VSNPRINTF && HAVE__VSNPRINTF */

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __GIFT_H */
