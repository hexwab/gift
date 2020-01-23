/*
 * ft_openft.h
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

#ifndef __OPENFT_H
#define __OPENFT_H

/*****************************************************************************/

/*
 * TODO FOR OPENFT 0.10.0
 *
 * o Node clustering
 *
 * TODO FOR OPENFT 0.0.8
 *
 * o Modify FT_CHILD_REQUEST/RESPONSE so that communication of total accepted
 *   number of shares is possible before data begins transmission
 * o Do not write MD5 sums as strings
 * o Cleanup session handshaking
 * o Version negotiation should not use ft_uint16
 */
#define OPENFT_MAJOR 0
#define OPENFT_MINOR 0
#define OPENFT_MICRO 7
#define OPENFT_REV   8

/*****************************************************************************/

#ifndef LOG_PFX
# define LOG_PFX "[OpenFT  ] "
#endif

#include "giftconfig.h"

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

#include "dataset.h"
#include "file.h"
#include "conf.h"
#include "share_db.h"

#include "network.h"

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
#if 0
	Protocol     *p;                   /**< reference the protocol itself
	                                    *   for the purpose of unification
	                                    *   via the OPENFT macro. */
#endif
	unsigned char shutdown;            /**< set to TRUE when OpenFT is
	                                    *   being destroyed so that certain
	                                    *   systems dont attempt new
	                                    *   connections. */
	unsigned long main_timer;          /**< main openft connection handling
	                                    *   timer. */
	Config       *conf;                /**< ~/.giFT/OpenFT/OpenFT.conf */
	Connection   *ft;                  /**< core openft communication bind. */
	Connection   *http;                /**< http communication bind. */
} OpenFT;

#define openft_p (openft_get_proto())
#define OPENFT ((OpenFT *)(openft_get_proto()->udata))
#define FT_SELF (FT_NODE(OPENFT->ft))
#define HTTP_SELF (OPENFT->http)

/*****************************************************************************/

int OpenFT_init (Protocol *p);
Protocol *openft_get_proto (void);

/*****************************************************************************/

#endif /* __OPENFT_H */
