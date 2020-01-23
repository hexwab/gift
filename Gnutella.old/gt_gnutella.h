/*
 * $Id: gt_gnutella.h,v 1.15 2003/07/14 16:40:47 hipnod Exp $
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
#define GT_MICRO_VERSION     "2"

#define GT_VERSION           GT_MAJOR_VERSION "." GT_MINOR_VERSION "." \
                             GT_MICRO_VERSION

/*****************************************************************************/

#define      GT_DEBUG
#define LIBGIFT_DEBUG

#include "lib/libgift.h"

/*****************************************************************************/

#include "plugin/protocol.h"
#include "plugin/share.h"
#include "plugin/share_hash.h"
#include "plugin/transfer_api.h"

/*****************************************************************************/

#include "src/share_cache.h"
#include "src/download.h"
#include "src/upload.h"

/*****************************************************************************/

#include "lib/event.h"
#include "lib/dataset.h"
#include "lib/file.h"

#include "lib/network.h"
#include "lib/fdbuf.h"

/*****************************************************************************/

#include "http.h"

#include "gt_guid.h"
#include "gt_conf.h"

#include <ctype.h>

/*****************************************************************************/
/* common definitions for all files */

extern Protocol      *GT;
#define gnutella_p    GT
#define gt_proto      GT

extern struct gt_node *gt_self;
#define GT_SELF gt_self

extern HTTP_Protocol *gt_http;

extern gt_guid_t     *gt_client_guid;

/*****************************************************************************/

#define GNUTELLA_LOCAL_MODE     gt_config_get_int("local/lan_mode=0")
#define GNUTELLA_LOCAL_ALLOW    gt_config_get_str("local/hosts_allow=LOCAL")

/*****************************************************************************/

char    *gt_version     (void);
time_t   gt_uptime      (void);
int      gt_is_local_ip (in_addr_t ip, in_addr_t src);

/*****************************************************************************/

#endif /* __GT_GNUTELLA_H__ */
