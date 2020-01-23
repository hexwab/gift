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

#define OPENFT_MAJOR 0
#define OPENFT_MINOR 0
#define OPENFT_MICRO 4

/*****************************************************************************/

#include "giftconfig.h"

/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gift.h"
#include "download.h"
#include "upload.h"
#include "event.h"

#include "list.h"
#include "hash.h"

#include "share.h"
#include "share_db.h"

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
/**/extern Protocol *openft_proto;
#endif

/*****************************************************************************/

#ifdef DEBUG
# define TRACE_SOCK(args)   (printf ("%-12s:%-4i %s (%s): ", __FILE__, __LINE__, __PRETTY_FUNCTION__, net_peer_ip (c ? c->fd : -1)), printf args, printf ("\n"))
#else /* !DEBUG */
# define TRACE_SOCK(args)
#endif /* DEBUG */

/*****************************************************************************/

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/*****************************************************************************/

/* registered callbacks to report transfer statistics to the daemon */
void ft_upload   (Chunk *chunk, char *segment, size_t len);
void ft_download (Chunk *chunk, char *segment, size_t len);

int  OpenFT_init (Protocol *p);

/*****************************************************************************/

#endif /* __OPENFT_H__ */
