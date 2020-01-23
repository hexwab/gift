/*
 * $Id: gt_gnutella.h,v 1.38 2004/11/16 18:47:21 hexwab Exp $
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

#ifndef GIFT_GT_GNUTELLA_H_
#define GIFT_GT_GNUTELLA_H_

/*****************************************************************************/

#include "config.h"

/*****************************************************************************/

#define GT_VERSION           GT_MAJOR_VERSION "." GT_MINOR_VERSION "." \
                             GT_MICRO_VERSION GT_EXTRA_VERSION

#define GT_RELEASE_DATE      1100630492 /* Tue Nov 16 18:41:32 2004 */

#define      GT_DEBUG
#define LIBGIFT_DEBUG

#define GIFT_PLUGIN
#include <libgift/libgift.h>

/* this works because duplicate case labels are disallowed in C */
#define assert_at_compile(cond) \
        do { switch (0) { default: case 0: case cond: break; } } while (0)

/*****************************************************************************/

#include <libgift/proto/protocol.h>
#include <libgift/proto/share.h>
#include <libgift/proto/share_hash.h>
#include <libgift/proto/transfer_api.h>

/*****************************************************************************/

#include <libgift/event.h>
#include <libgift/dataset.h>
#include <libgift/file.h>

#include <libgift/network.h>
#include <libgift/fdbuf.h>

/*****************************************************************************/

#include "gt_guid.h"
#include "gt_conf.h"

#include <ctype.h>

/*****************************************************************************/
/* global variables */

extern Protocol        *GT;

extern struct gt_node  *GT_SELF;

extern gt_guid_t       *GT_SELF_GUID;

/*****************************************************************************/

#define GNUTELLA_LOCAL_MODE     gt_config_get_int("local/lan_mode=0")
#define GNUTELLA_LOCAL_FW       gt_config_get_int("local/firewalled=0")
#define GNUTELLA_LOCAL_ALLOW    gt_config_get_str("local/hosts_allow=LOCAL")

/*****************************************************************************/

BOOL gt_is_local_ip (in_addr_t ip, in_addr_t src);

/*****************************************************************************/

/*
 * libgift doesn't define this...
 */
#ifndef EDAYS
#define EDAYS (24 * EHOURS)
#endif

/*
 * This needs libgift >= 0.11.5
 */
#ifndef GIFT_PLUGIN_EXPORT
#define GIFT_PLUGIN_EXPORT
#endif

/*
 * Invalid input/timer ids used for readability and to help porting to libgift
 * 0.12.
 */
#define INPUT_NONE   ((input_id)0)
#define TIMER_NONE   ((timer_id)0)

/*
 * The entry-point for the giFT daemon
 */
GIFT_PLUGIN_EXPORT
  BOOL Gnutella_init (Protocol *p);

/*****************************************************************************/

#endif /* GIFT_GT_GNUTELLA_H_ */
