/*
 * $Id: ft_openft.h,v 1.25 2003/05/05 09:49:10 jasta Exp $
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
 */

/*****************************************************************************/

/* undefine if you don't wish your node to participate in DDoS attacks at
 * jasta's request */
#define EVIL_DDOS_NODE

/*
 * TODO:
 *
 *  * Cleanup session handshaking
 *  * Version negotiation should not use uint16_t
 */
#define OPENFT_MAJOR 0
#define OPENFT_MINOR 0
#define OPENFT_MICRO 9
#define OPENFT_REV   4

/*****************************************************************************/

/*
 * NOTE:
 * I have decided to sack compile time for header [in]sanity...please shoot
 * me.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gift.h"
#include "share_file.h"
#include "download.h"
#include "upload.h"
#include "event.h"
#include "stopwatch.h"

#include "dataset.h"
#include "file.h"
#include "conf.h"
#include "share_db.h"

#include "network.h"

#include "ft_conf.h"
#include "ft_share.h"
#include "ft_share_file.h"
#include "ft_shost.h"
#include "ft_stats.h"

#include "ft_protocol.h"

#include "ft_packet.h"
#include "ft_stream.h"
#include "ft_node.h"

#include "ft_version.h"
#include "ft_utils.h"

#include <assert.h>

/*****************************************************************************/

/**
 * Arbitrary data associated w/ the openft protocol structure.
 */
typedef struct
{
	unsigned char shutdown;            /**< Set to TRUE when OpenFT is
	                                    *   being destroyed so that certain
	                                    *   systems dont attempt new
	                                    *   connections. */
	timer_id      main_timer;          /**< Main openft connection handling
	                                    *   timer. */
	Config       *conf;                /**< ~/.giFT/OpenFT/OpenFT.conf */
	TCPC         *ft;                  /**< Core OpenFT communication bind. */
	TCPC         *http;                /**< HTTP communication bind. */
} OpenFT;

#ifndef __FT_OPENFT_C
extern Protocol *FT;
#endif /* __FT_OPENFT_C */

#define OPENFT       ((OpenFT *)FT->udata)
#define FT_SELF      (FT_NODE(OPENFT->ft))
#define HTTP_SELF    (OPENFT->http)

/*****************************************************************************/

/**
 * First function called by giFT to initialize the protocol structure for
 * usage.
 *
 * @return Boolean success or failure.  If this function fails, the plugin
 *         loading process will be aborted.
 */
int OpenFT_init (Protocol *p);

/*****************************************************************************/

#endif /* __FT_OPENFT_H */
