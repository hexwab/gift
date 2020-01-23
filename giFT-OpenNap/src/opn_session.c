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

#include "opn_opennap.h"
#include "opn_node.h"
#include "opn_protocol.h"

static void on_session_read(int fd, input_id input, OpnSession *session)
{
	if (net_sock_error(fd))
		opn_session_free(session);
	else
		opn_packet_recv(session->con, session);
}

static void session_login(OpnSession *session)
{
	OpnPacket *packet;
	char buf[128];

	assert(session);

	snprintf(buf, sizeof(buf), "%s none %hu \""
	         OPENNAP_CLIENTNAME " " VERSION "\" 0",
	         OPENNAP_USERNAME, OPENNAP_DATAPORT);
	
	if (!(packet = opn_packet_new(OPN_CMD_LOGIN, buf, strlen(buf))))
		return;

	opn_packet_send(packet, session->con);
	opn_packet_free(packet);
}

static void on_session_connect(int fd, input_id input, OpnSession *session)
{
	if (net_sock_error(fd)) {
		opn_session_free(session);
		return;
	}

	session->state = OPN_SESSION_STATE_HANDSHAKING;

	input_remove(input);
	input_add(fd, session, INPUT_READ, (InputCallback) on_session_read, TIMEOUT_DEF);

	session_login(session);
}

BOOL opn_session_connect(OpnSession *session, OpnNode *node)
{
	assert(session);
	assert(node);
	
	if (!(session->con = tcp_open(node->ip, node->port, FALSE)))
		return FALSE;

	session->node = node;
	session->state = OPN_SESSION_STATE_CONNECTING;

	input_add(session->con->fd, session, INPUT_WRITE, (InputCallback) on_session_connect, TIMEOUT_DEF);
	
	return TRUE;
}

OpnSession *opn_session_new()
{
	OpnSession *session;

	if (!(session = malloc(sizeof(OpnSession))))
		return NULL;

	memset(session, 0, sizeof(OpnSession));

	OPENNAP->sessions = list_prepend(OPENNAP->sessions, session);

	return session;
}

void opn_session_free(OpnSession *session)
{
	if (!session)
		return;

	if (session->con)
		tcp_close(session->con);

	if (OPENNAP->sessions)
		OPENNAP->sessions = list_remove(OPENNAP->sessions, session);
	
	free(session);
}

OpnSession *opn_session_find(OpnUrl *url)
{
	OpnSession *session;
	List *l;

	assert(url);
	
	for (l = OPENNAP->searches; l; l = l->next) {
		session = (OpnSession *) l->data;

		if (session->node->ip == url->server.ip
		    && session->node->port == url->server.port)
			return session;
	}

	return NULL;
}

