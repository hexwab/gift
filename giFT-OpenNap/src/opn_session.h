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

#ifndef __OPN_SESSION_H
#define __OPN_SESSION_H

typedef enum {
	OPN_SESSION_STATE_DISCONNECTED,
	OPN_SESSION_STATE_CONNECTING,
	OPN_SESSION_STATE_HANDSHAKING,
	OPN_SESSION_STATE_CONNECTED,
} OpnSessionState;

typedef struct {
	uint32_t users;
	uint32_t files;
	double size; /* Given in GB */
} OpnStats;

typedef struct {
	TCPC *con;
	OpnSessionState state;
	OpnNode *node;
	OpnStats stats;
} OpnSession;

OpnSession *opn_session_new();
void opn_session_free(OpnSession *session);
OpnSession *opn_session_find(OpnUrl *url);
int opn_session_connect(OpnSession *session, OpnNode *node);

#endif

