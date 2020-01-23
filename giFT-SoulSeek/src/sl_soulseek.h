/*
 * Copyright (C) 2003 Arend van Beelen jr. (arend@auton.nl)
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

#ifndef __SOULSEEK_H
#define __SOULSEEK_H

/*****************************************************************************/

#define HEAVY_DEBUG
#define GIFT_PLUGIN
#define LOG_PFX "SoulSeek: "
#include <libgift/libgift.h>

#define TRUE  1
#define FALSE 0

/*****************************************************************************/

#include <config.h>

#include <libgift/proto/protocol.h>
#include <libgift/proto/share.h>
#include <libgift/file.h>
#include <libgift/parse.h>
#include <libgift/network.h>
#include <libgift/dataset.h>
#include <libgift/tcpc.h>
#include <libgift/conf.h>

#include <libgift/proto/transfer_api.h>

#include "sl_download.h"
#include "sl_meta.h"
#include "sl_node.h"
#include "sl_packet.h"
#include "sl_peer.h"
#include "sl_search.h"
#include "sl_session.h"
#include "sl_share.h"
#include "sl_stats.h"
#include "sl_string.h"

/*****************************************************************************/

#define SL_PLUGIN ((SLPlugin *) sl_proto->udata)
#define SL_SESSION ((SLSession *) SL_PLUGIN->session)
#define SL_PROTO (sl_proto)

/*****************************************************************************/

typedef struct
{
	Config *conf;                  // ~/.giFT/SoulSeek/SoulSeek.conf

	SLSession *session;          // established session to supernode we're currently using

	SLSearchList *searches;      // list containing all currently running searches

	SLStats *stats;              // network statistics

	SLPeerList *peerlist;        // list of peers we're connected to

	char *server;                  // server to connect to
	int port;                      // port to connect to
	sl_string *username;         // our username
	
} SLPlugin;

/*****************************************************************************/

// global pointer to plugin struct
extern Protocol *sl_proto;

// called by gift to init plugin
#ifdef WIN32
int __declspec(dllexport) SoulSeek_init (Protocol *p);
#else
int SoulSeek_init (Protocol *p);
#endif

/*****************************************************************************/

#endif /* __SOULSEEK_H */
