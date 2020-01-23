/*
 * $Id: giftd.h,v 1.2 2003/06/06 04:06:35 jasta Exp $
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

#ifndef __GIFTD_H
#define __GIFTD_H

/*****************************************************************************/

/**
 * @file giftd.h
 *
 * @brief Centralized header for the giFT daemon.
 */

/*****************************************************************************/

/* hack to prefix all things produced by TRACE and friends with "giFT: " */
#ifndef LOG_PFX
# define LOG_PFX "giFT: "
#endif /* LOG_PFX */

/* defined by the autotools environment when building from our own local
 * tree ... */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#define    GIFT_DEBUG
#define LIBGIFT_DEBUG

/* the giFT daemon is still a slave to libgift */
#include "lib/libgift.h"

/*****************************************************************************/

/* custom memory profiling tool written by jasta, but not specifically for
 * gift */
#ifdef TCG_LEAK_DETECT
# include <tcgprof.h>
#endif

/*****************************************************************************/

#endif /* __GIFTD_H */
