/*
 * $Id: gt_gnutella.h,v 1.8 2003/05/04 08:17:37 hipnod Exp $
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

#ifndef __GT_GNUTELLA_H__
#define __GT_GNUTELLA_H__

/*****************************************************************************/

#ifndef LOG_PFX
# define LOG_PFX "[Gnutella] "
#endif

#define GT_MAJOR_VERSION     "0"
#define GT_MINOR_VERSION     "0"
#define GT_MICRO_VERSION     "0"

#define GT_VERSION           GT_MAJOR_VERSION "." GT_MINOR_VERSION "." \
                             GT_MICRO_VERSION

/*****************************************************************************/

#include "gift.h"
#include "share_file.h"
#include "download.h"
#include "upload.h"
#include "event.h"

#include "dataset.h"
#include "file.h"
#include "conf.h"
#include "share_db.h"

#include "network.h"

#include "share_cache.h"

#include "fdbuf.h"

#include "protocol.h"

#include "http.h"

#include "gt_guid.h"

#include <ctype.h>

/*****************************************************************************/
/* common definitions for all files */

extern Protocol      *GT;
#define gnutella_p    GT
#define gt_proto      GT

extern struct _gt_node *gt_self;
#define GT_SELF gt_self

extern Config        *gt_conf;

extern HTTP_Protocol *gt_http;

extern gt_guid       *gt_client_guid;

/*****************************************************************************/

char    *gt_version     (void);
time_t   gt_uptime      (void);
int      gt_is_local_ip (in_addr_t ip, in_addr_t src);

/*****************************************************************************/

#endif /* __GT_GNUTELLA_H__ */
