/*
 * $Id: ft_openft.h,v 1.49 2004/08/06 02:01:54 jasta Exp $
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

#ifndef __FT_OPENFT_H
#define __FT_OPENFT_H

/*****************************************************************************/

/**
 * @file ft_openft.h
 *
 * @brief Main plugin initialization for OpenFT.
 *
 * This also functions as header centralization for OpenFT.
 */

/*****************************************************************************/

/*
 * Undefine if you don't wish your node to participate in DDoS attacks at
 * jasta's request.
 */
#define EVIL_DDOS_NODE

/*****************************************************************************/

/*
 * Defines OpenFT versioning information and other useful compile-time
 * features.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#define OPENFT_0_2_0_1 (0x00020001)

/*****************************************************************************/

/* hooray for debug */
#define  OPENFT_DEBUG
#define LIBGIFT_DEBUG

#define GIFT_PLUGIN
#include <libgift/libgift.h>

/*****************************************************************************/

#include <libgift/proto/protocol.h>
#include <libgift/proto/share.h>
#include <libgift/proto/share_hash.h>

/*****************************************************************************/

#include <libgift/file.h>
#include <libgift/stopwatch.h>

/*****************************************************************************/

#include "ft_conf.h"
#include "ft_share.h"
#include "ft_share_file.h"
#include "ft_stats.h"

#include "ft_protocol.h"

#include "ft_packet.h"
#include "ft_stream.h"
#include "ft_node.h"

#include "ft_version.h"
#include "ft_utils.h"

/*****************************************************************************/

#ifndef INLINE
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  define INLINE inline
# elif defined(__GNUC__)
#  define INLINE inline
# else
#  define INLINE
# endif
#endif /* !INLINE */

/*****************************************************************************/

/**
 * Special data attached to our protocol structure.  This is almost entirely
 * setup and maintained from plugin start (openft_start), but is made
 * available to the entire plugin as a convenient means of accessing our
 * local nodes properties.
 */
struct openft_data
{
	Config          *cfg;              /**< ~/.giFT/OpenFT/OpenFT.conf */

	ft_nodeinfo_t    ninfo;            /**< Bound ports, node class, etc */

	TCPC            *bind_openft;      /**< Listen on the OpenFT port */
	TCPC            *bind_http;        /**< Listen on the OpenFT HTTP port */

	ft_class_t       klass_alw;        /**< Class mask for auto-promotion */

	unsigned int     avail;            /**< Cached upload availability */

	timer_id         cmaintain_timer;  /**< Main OpenFT connection maintenance
	                                    *   timer */

	BOOL             shutdown;         /**< Try to hackishly eliminate a race
	                                    *   condition when our plugin is
	                                    *   unloading */
};

/*
 * Very ugly hack to export the two global symbols for this plugin whose
 * implementation rests in ft_openft.c.  Please do not consider this as
 * suggested plugin behaviour, as I frankly wish I had done something
 * differently.
 */
#ifndef __FT_OPENFT_C
extern Protocol *FT;
extern struct openft_data *openft;
#endif /* __FT_OPENFT_C */

/*****************************************************************************/

/*
 * Defined for Windows portability by giFT 0.11.5.  Not present in any
 * version prior to that.
 */
#ifndef GIFT_PLUGIN_EXPORT
# define GIFT_PLUGIN_EXPORT
#endif /* !GIFT_PLUGIN_EXPORT */

/**
 * First function called by giFT to initialize the protocol structure for
 * usage.
 *
 * @return Boolean success or failure.  If this function fails, the plugin
 *         loading process will be aborted.
 */
GIFT_PLUGIN_EXPORT
  BOOL OpenFT_init (Protocol *p);

/*****************************************************************************/

#endif /* __FT_OPENFT_H */
