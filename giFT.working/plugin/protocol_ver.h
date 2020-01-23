/*
 * $Id: protocol_ver.h.in,v 1.2 2003/10/16 18:50:55 jasta Exp $
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

#ifndef __PROTOCOL_VER_H
#define __PROTOCOL_VER_H

/*****************************************************************************/

/* plugin developers will want to use this to describe the version they are
 * known to be compatible with */
#define LIBGIFTPROTO_MKVERSION(major,minor,micro) \
    ((((major) & 0xff) << 24) |                   \
     (((minor) & 0xff) << 16) |                   \
     (((micro) & 0xff) <<  8))

/* autoconf will fill this in for us... */
#ifndef _MSC_VER
#define LIBGIFTPROTO_VERSION                      \
    LIBGIFTPROTO_MKVERSION(0,  \
                           11,  \
                           6)
#else /* _MSC_VER */
#include "giftconfig_win32.h"
#define LIBGIFTPROTO_VERSION                      \
    LIBGIFTPROTO_MKVERSION(LIBGIFTPROTO_MAJOR,  \
                           LIBGIFTPROTO_MINOR,  \
                           LIBGIFTPROTO_MICRO)
#endif /* !_MSC_VER */

/*****************************************************************************/

#endif /* __PROTOCOL_VER_H */
