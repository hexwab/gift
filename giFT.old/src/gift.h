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

/* #define GC_LEAK_DETECT */
/* #define TCG_LEAK_DETECT */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define GIFT_CVSID "$Date: 2002/03/04 14:29:49 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <unistd.h>

#include <assert.h>

/*****************************************************************************/

#ifdef WIN32
#include "win32_support.h"
#endif

#include "parse.h"
#include "hash.h"
#include "dataset.h"
#include "connection.h"
#include "protocol.h"
#include "interface.h"
#include "if_event.h"

/*****************************************************************************/

#ifdef TCG_LEAK_DETECT
#include <tcgprof.h>
#endif /* TCG_LEAK_DETECT */

#ifdef GC_LEAK_DETECT
#include "leak_detector.h"
static char *GC_strdup (char *x) { char *str = GC_malloc (strlen (x)); strcpy (str, x); return str; }
#define strdup(x) GC_strdup(x)
#endif /* GC_LEAK_DETECT */

/*****************************************************************************/

#define DEBUG

#ifdef DEBUG
#ifdef WIN32
# define GIFT_DEBUG(args) (win32_printf ("** gift-debug:    "), win32_printf args, win32_printf ("\r\n"))
# define GIFT_ERROR(args) (win32_printf ("** gift-error:    "), win32_printf args, win32_printf ("\r\n"))
# define GIFT_WARN(args)  (win32_printf ("** gift-warning:  "), win32_printf args, win32_printf ("\r\n"))
# define GIFT_FATAL(args) win32_fatal args
# define TRACE(args)      (win32_printf ("%-12s:%-4i %s: ", __FILE__, __LINE__, __PRETTY_FUNCTION__), win32_printf args, win32_printf ("\r\n"))
# define TRACE_FUNC()      win32_printf ("%-12s:%-4i %s\r\n", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else /* !WIN32 */
# define GIFT_DEBUG(args) (fprintf (stderr, "** gift-debug:    "), printf args, printf ("\n"))
# define GIFT_ERROR(args) (fprintf (stderr, "** gift-error:    "), printf args, printf ("\n"))
# define GIFT_WARN(args)  (fprintf (stderr, "** gift-warning:  "), printf args, printf ("\n"))
# define GIFT_FATAL(args) (fprintf (stderr, "** gift-fatal:    "), printf args, printf ("\n")), exit (1)
# define TRACE(args)      (fprintf (stderr, "%-12s:%-4i %s: ", __FILE__, __LINE__, __PRETTY_FUNCTION__), printf args, printf ("\n"))
# define TRACE_FUNC()      fprintf (stderr, "%-12s:%-4i %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif /* WIN32 */
#else  /* !DEBUG */
# define GIFT_DEBUG(args)
# define GIFT_ERROR(args)
# define GIFT_WARN(args)
# define GIFT_FATAL(args)
# define TRACE(args)
# define TRACE_FUNC()
#endif /* DEBUG */

/*****************************************************************************/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*****************************************************************************/

#ifndef MIN
#define MIN(x,y)              ((x < y) ? x : y)
#endif

#ifndef MAX
#define MAX(x,y)              ((x > y) ? x : y)
#endif

#ifndef CLAMP
#define CLAMP(value,min,max)  ((value < min) ? min : ((value > max) ? max : value))
#endif

/*****************************************************************************/

#endif /* __GIFT_H */
