/*
 * Copyright (C) 2003 Markus Kern (mkern@users.sourceforge.net)
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

#ifndef __FASTTRACK_H
#define __FASTTRACK_H

/*****************************************************************************/

/*
#ifndef LOG_PFX
# define LOG_PFX "FastTrack: "
#endif
*/
#define FILE_LINE_FUNC __FILE__,__LINE__,__PRETTY_FUNCTION__

#define FST_DBG(fmt)				FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt)
#define FST_DBG_1(fmt,a)			FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a)
#define FST_DBG_2(fmt,a,b)			FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b)
#define FST_DBG_3(fmt,a,b,c)		FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b,c)
#define FST_DBG_4(fmt,a,b,c,d)		FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b,c,d)
#define FST_DBG_5(fmt,a,b,c,d,e)	FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b,c,d,e)

#ifdef HEAVY_DEBUG
# define FST_HEAVY_DBG(fmt)				FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt)
# define FST_HEAVY_DBG_1(fmt,a)			FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a)
# define FST_HEAVY_DBG_2(fmt,a,b)		FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b)
# define FST_HEAVY_DBG_3(fmt,a,b,c)		FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b,c)
# define FST_HEAVY_DBG_4(fmt,a,b,c,d)	FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b,c,d)
# define FST_HEAVY_DBG_5(fmt,a,b,c,d,e)	FST_PROTO->trace(FST_PROTO,FILE_LINE_FUNC,fmt,a,b,c,d,e)
#else
# define FST_HEAVY_DBG(fmt)				
# define FST_HEAVY_DBG_1(fmt,a)			
# define FST_HEAVY_DBG_2(fmt,a,b)		
# define FST_HEAVY_DBG_3(fmt,a,b,c)		
# define FST_HEAVY_DBG_4(fmt,a,b,c,d)	
# define FST_HEAVY_DBG_5(fmt,a,b,c,d,e)	
#endif

//#include "giftconfig.h"

typedef signed char fst_int8;
typedef unsigned char fst_uint8;
typedef signed short fst_int16;
typedef unsigned short fst_uint16;
typedef signed int fst_int32;
typedef unsigned int fst_uint32;

/*****************************************************************************/
/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
*/
#include <ctype.h>

#include "gift.h"

#include "protocol.h"
#include "if_event.h"
#include "file.h"
#include "parse.h"
#include "transfer.h"
#include "download.h" // download_remove_source
#include "network.h"
#include "dataset.h"
#include "tcpc.h"

#include "fst_node.h"
#include "fst_packet.h"
#include "fst_session.h"
#include "fst_hash.h"
#include "fst_search.h"
#include "fst_download.h"
#include "fst_stats.h"
#include "fst_utils.h"
#include "fst_meta.h"


/*****************************************************************************/

#define FST_PLUGIN ((FSTPlugin*)fst_proto->udata)
#define FST_PROTO (fst_proto)

#define FST_NETWORK_NAME "KaZaA"		// network name we send and which we require from other supernodes
#define FST_USER_NAME "giFTed"		// our user name on the network

#define FST_MAX_NODESFILE_SIZE 1000	// max number of nodes we save in nodes file

#define FST_MAX_SEARCH_RESULTS 0xFF	// max number of results we want to be returned per search

// various timeouts in ms
#define FST_SESSION_CONNECT_TIMEOUT		(3*1000)
#define FST_SESSION_HANDSHAKE_TIMEOUT	(8*1000)

#define FST_DOWNLOAD_CONNECT_TIMEOUT		(20*1000)
#define FST_DOWNLOAD_HANDSHAKE_TIMEOUT	(10*1000)
#define FST_DOWNLOAD_DATA_TIMEOUT		(10*1000)

/*****************************************************************************/

typedef struct
{
//	Config *conf;				// ~/.giFT/FastTrack/FastTrack.conf

	FSTNodeCache *nodecache;		// cache that holds known supernode addresses

	FSTSession *session;			// established session to supernode we're currently using

	FSTSearchList *searches;		// list containing all currently running searches

	FSTStats *stats;				// network statistics

} FSTPlugin;

/*****************************************************************************/

// global pointer to plugin struct
extern Protocol *fst_proto;

// called by gift to init plugin
#ifdef WIN32
int __declspec(dllexport) FastTrack_init (Protocol *p);
#else
int FastTrack_init (Protocol *p);
#endif

/*****************************************************************************/

#endif /* __FASTTRACK_H */
