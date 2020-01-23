/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __OPENNAP_H
#define __OPENNAP_H

#define GIFT_PLUGIN
#define LOG_PFX "OpenNap: "

#include "../config.h"

#include <libgift/libgift.h>
#include <libgift/file.h>
#include <libgift/proto/protocol.h>
#include <libgift/proto/if_event_api.h>

#include "opn_node.h"
#include "opn_url.h"
#include "opn_session.h"
#include "opn_protocol.h"
#include "opn_packet.h"

typedef struct {
	Config *cfg;
	OpnNodeList *nodelist;
	List *sessions;
	List *searches;
	List *downloads;
} OpnPlugin;

/* Called by giFT to init plugin */
#ifdef WIN32
BOOL __declspec(dllexport) OpenNap_init(Protocol *p);
#else
BOOL OpenNap_init(Protocol *p);
#endif

void main_timer();
BOOL opn_is_connected();

#ifndef __OPN_OPENNAP_C
extern Protocol *opn_proto;
#endif

#define OPENNAP ((OpnPlugin *) opn_proto->udata)
#define IS_ONLINE() (OPENNAP && OPENNAP->session && OPENNAP->session->node)

#define OPENNAP_USERNAME config_get_str(OPENNAP->cfg, "main/username")
#define OPENNAP_DATAPORT config_get_int(OPENNAP->cfg, "main/dataport=6699")
#define OPENNAP_CLIENTNAME "giFT-OpenNap"

#endif

