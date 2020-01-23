/*
 * openft.h
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

/* NOTE - i have decided to sack compile time for header sanity ... please
 * shoot me */

#ifndef __OPENFT_H__
#define __OPENFT_H__

/*****************************************************************************/

/*
 * TODO FOR OPENFT 0.0.8
 *
 * o Node clustering
 *
 * TODO FOR OPENFT 0.0.7
 *
 * o Direct browse method over HTTP (transmit shares.db)
 * o Properly handle syncing shares
 * o Client-side availability calculations
 */
#define OPENFT_MAJOR 0
#define OPENFT_MINOR 0
#define OPENFT_MICRO 6
#define OPENFT_REV   4

/*****************************************************************************/

#ifndef LOG_PFX
# define LOG_PFX "[OpenFT  ] "
#endif

#include "giftconfig.h"

/*****************************************************************************/

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

#include "share.h"
#include "share_db.h"
#include "share_host.h"

#include "packet.h"

#include "protocol.h"
#include "network.h"
#include "node.h"

#include "conf.h"
#include "utils.h"

/* #include <tcgprof.h> */
#include <assert.h>

/*****************************************************************************/

#ifndef __OPENFT_C__
extern Protocol *openft_proto;
#endif

/*****************************************************************************/

#if 0
# ifdef DEBUG
#  define TRACE_SOCK(args)   (printf ("%-12s:%-4i %s (%s): ", __FILE__, __LINE__, __PRETTY_FUNCTION__, net_peer_ip (c ? c->fd : -1)), printf args, printf ("\n"))
# else /* !DEBUG */
#  define TRACE_SOCK(args)
# endif /* DEBUG */
#endif

/*****************************************************************************/

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/*****************************************************************************/

#define NODE_ALIAS config_get_str (openft_conf, "main/alias")

/*****************************************************************************/

/* registered callbacks to report transfer statistics to the daemon */
void ft_upload   (Chunk *chunk, char *segment, size_t len);
void ft_download (Chunk *chunk, char *segment, size_t len);

int  OpenFT_init (Protocol *p);

/*****************************************************************************/

#endif /* __OPENFT_H__ */
