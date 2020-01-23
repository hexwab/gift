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

/* #define GC_LEAK_DETECT */
/* #define TCG_LEAK_DETECT */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "giftconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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

#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <assert.h>

/*****************************************************************************/

#include "platform.h"

#include "parse.h"
#include "hash.h"
#include "dataset.h"
#include "connection.h"
#include "protocol.h"
#include "interface.h"
#include "if_event.h"

#include "conf.h"

/*****************************************************************************/

#ifdef TCG_LEAK_DETECT
# include <tcgprof.h>
#endif /* TCG_LEAK_DETECT */

#ifdef GC_LEAK_DETECT
# include "leak_detector.h"
static char *GC_strdup (char *x) { char *str = GC_malloc (strlen (x)); strcpy (str, x); return str; }
# define strdup(x) GC_strdup(x)
#endif /* GC_LEAK_DETECT */

/*****************************************************************************/

/* TODO move to config.h eventually: --enable-debug/--enable-release */
#define DEBUG

#ifdef DEBUG
# define GIFT_DEBUG(args) (printf ("** gift-debug:    "), printf args, printf ("\n"), fflush (stdout))
# define GIFT_ERROR(args) (printf ("** gift-error:    "), printf args, printf ("\n"), fflush (stdout))
# define GIFT_WARN(args)  (printf ("** gift-warning:  "), printf args, printf ("\n"), fflush (stdout))
# define GIFT_FATAL(args) (printf ("** gift-fatal:    "), printf args, printf ("\n"), fflush (stdout)), exit (1)
# define TRACE(args)      (printf ("%-12s:%-4i %s: ", __FILE__, __LINE__, __PRETTY_FUNCTION__), printf args, printf ("\n"), fflush (stdout))
# define TRACE_FUNC()     (printf ("%-12s:%-4i %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__), fflush (stdout))
# define TRACE_MEM(ptr, len) \
do {\
	int i;\
	char *p = (char *)ptr;\
	for (i = 0; i < len; i++)\
	{\
		unsigned char c;\
		if ((i % 16) == 0)\
			printf ("%04x: ", i); \
		c = p[i];\
		printf ("%02x ", c);\
		if (((i + 1) % 16) == 0)\
			printf ("\n");\
	}\
	printf ("\n");\
	fflush (stdout);\
} while (0)

# define DEBUG_TAG 0x12345678U
# define VALIDATE(arg) (arg = DEBUG_TAG)
# define INVALIDATE(arg) (arg = ~DEBUG_TAG)
# define ASSERT_VALID(arg) assert(arg == DEBUG_TAG)
#else  /* !DEBUG */
# define GIFT_DEBUG(args)
# define GIFT_ERROR(args)
# define GIFT_WARN(args)
# define GIFT_FATAL(args)
# define TRACE(args)
# define TRACE_FUNC()
# define TRACE_MEM(ptr, len)
# define VALIDATE(arg)
# define INVALIDATE(arg)
# define ASSERT_VALID(arg)
#endif /* DEBUG */

/*****************************************************************************/

#ifndef TRUE
# define TRUE 1
#endif /* TRUE */

#ifndef FALSE
# define FALSE 0
#endif /* FALSE */

/*****************************************************************************/

#ifndef MIN
# define MIN(x,y)              ((x < y) ? x : y)
#endif /* MIN */

#ifndef MAX
# define MAX(x,y)              ((x > y) ? x : y)
#endif /* MAX */

#ifndef CLAMP
# define CLAMP(value,min,max)  ((value < min) ? min : ((value > max) ? max : value))
#endif /* CLAMP */

/*****************************************************************************/

#endif /* __GIFT_H */
