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

#include "sl_soulseek.h"
#include "sl_packet.h"
#include "sl_session.h"
#include "sl_stats.h"
#include "sl_utils.h"

#include <errno.h>
#include <sys/socket.h>

/*****************************************************************************/

static void sl_session_cb_read(int fd, input_id input, SLSession *session);

/*****************************************************************************/

// called when the connection with the server is established
static void sl_session_cb_connect(int fd, input_id input, SLSession *session)
{
	// check whether the socket is still open
	if(fd == -1 || !input || net_sock_error(fd))
	{
		sl_session_disconnect(session);
		SL_PROTO->warn(SL_PROTO, "SoulSeek connection failed or closed.");
		return;
	}

	// we'll now use the read callback
	input_remove(input);
	input_add(session->tcp_conn->fd, session, INPUT_READ,
	          (InputCallback) sl_session_cb_read, 30 * SECONDS);

	// send the login request
	sl_session_send_login_request(session);
}

// reads new data from the network
static void sl_session_cb_read(int fd, input_id input, SLSession *session)
{
	int n;
	uint32_t  type;
	uint32_t  length;
	uint8_t  *data;
	FDBuf *buf;

	// check whether the socket is still open
	if(fd == -1 || !input || net_sock_error(fd))
	{
		sl_proto->warn(sl_proto, "SoulSeek server disconnected.");
		sl_session_disconnect(session);
		return;
	}

	// get the buffer
	buf = tcp_readbuf(session->tcp_conn);
	length = buf->flag + sizeof(uint32_t);

	if((n = fdbuf_fill(buf, length)) < 0)
	{
		sl_proto->warn(sl_proto, "SoulSeek server disconnected.");
		sl_session_disconnect(session);
		return;
	}
	else if(n > 0)
	{
		return;
	}

	data = fdbuf_data(buf, NULL);

	memcpy(&length, data, sizeof(uint32_t));
	if(buf->flag || !length)
	{
		buf->flag = 0;
		fdbuf_release(buf);

		memcpy(&type, data + sizeof(uint32_t), sizeof(uint32_t));
		switch(type)
		{
			case SLLogin:
				sl_session_receive_login_confirmation(session, data);
				break;

			case SLGetPeerAddress:
				sl_session_receive_get_peer_address(session, data);
				break;

			case SLRoomList:
				sl_stats_receive_roomlist(SL_PLUGIN->stats, data);
				break;

			case SLGlobalUserList:
				sl_stats_receive_global_userlist(SL_PLUGIN->stats, data);
				break;

			case SLConnectToPeer:
				sl_session_receive_connect_to_peer_request(session, data);
				break;

			default:
				SL_PROTO->dbg(SL_PROTO, "Received unknown (%d) packet.", type);
		}
	}
	else if(!buf->flag)
	{
		buf->flag = length;
	}
}

/*****************************************************************************/

// allocates and returns a new session
SLSession *sl_session_create()
{
	SLSession *new_session = (SLSession *) MALLOC(sizeof(SLSession));

	new_session->state = SLNew;
	new_session->tcp_conn = NULL;
	new_session->in_packets = NULL;

	return new_session;
}

// connects to the network
int sl_session_connect(SLSession *session)
{
	assert(session != NULL);

	SL_PROTO->dbg(SL_PROTO, "Starting SoulSeek connection...");

	// connect the socket to the SoulSeek server
	session->tcp_conn = tcp_open(net_ip(SL_PLUGIN->server), SL_PLUGIN->port, FALSE);
	if(session->tcp_conn == NULL)
	{
		SL_PROTO->warn(SL_PROTO, "Creating socket failed.");
		return FALSE;
	}

	// hook up the socket to the callback function
	input_add(session->tcp_conn->fd, session, INPUT_WRITE,
	          (InputCallback) sl_session_cb_connect, 30 * SECONDS);

	return TRUE;
}

// disconnects from the network
void sl_session_disconnect(SLSession *session)
{
	assert(session != NULL);

	tcp_close_null(&session->tcp_conn);
}

// destroys a previously allocated session
void sl_session_destroy(SLSession *session)
{
	assert(session != NULL);

	sl_session_disconnect(session);
	free(session);
}

// sends a Login request
void sl_session_send_login_request(SLSession *session)
{
	// create the login packet
	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLLogin);
	sl_packet_insert_string(packet, SL_PLUGIN->username);
	sl_packet_insert_string(packet, sl_string_create_with_contents(config_get_str(SL_PLUGIN->conf, "main/passwd")));
	sl_packet_insert_integer(packet, SL_VERSION);

	// send the packet
	sl_packet_send(session->tcp_conn, packet);

	sl_packet_destroy(packet);
}

// tells the server on which port we're listening
void sl_session_send_set_wait_port(SLSession *session)
{
	// create the SetWaitPort packet
	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLSetWaitPort);
	sl_packet_insert_integer(packet, SL_PEER_PORT);

	// send the packet
	sl_packet_send(session->tcp_conn, packet);

	sl_packet_destroy(packet);
}

// sends a GetPeerAddress request
void sl_session_send_get_peer_address(SLSession *session, sl_string *username)
{
	// create the SetWaitPort packet
	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLSetWaitPort);
	sl_packet_set_type(packet, SLGetPeerAddress);
	sl_packet_insert_string(packet, username);

	// send the packet
	sl_packet_send(session->tcp_conn, packet);

	sl_packet_destroy(packet);
}

// tell the server that we couldn't connect to the peer he told us
void sl_session_send_cant_connect_to_peer(SLSession *session, uint32_t token, sl_string *username)
{
	// create the CantConnectToPeer packet
	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLCantConnectToPeer);
	sl_packet_insert_integer(packet, token);
	sl_packet_insert_string(packet, username);

	// send the packet
	sl_packet_send(session->tcp_conn, packet);

	sl_packet_destroy(packet);
}

// receives a reply to the Login request
int sl_session_receive_login_confirmation(SLSession *session, uint8_t *data)
{
	sl_string *string;
	SLPacket *packet;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return FALSE;

	if(!sl_packet_get_byte(packet, NULL))
	{
		if((string = sl_packet_get_string(packet)))
			SL_PROTO->err(SL_PROTO, "Login was rejected: %s", string->contents);
		else
			SL_PROTO->err(SL_PROTO, "Login failed, no reason given.");
		sl_session_disconnect(session);
		return FALSE;
	}

	if((string = sl_packet_get_string(packet)))
	{
		// now send the server a message to tell it on which port we're listening
		sl_session_send_set_wait_port(session);
		return TRUE;
	}

	SL_PROTO->err(SL_PROTO, "Received invalid login response.");
	sl_session_disconnect(session);
	return FALSE;
}

static void connect_if_waiting(SLPeerConnReq *req, void **args)
{
	if (req->state == SLReqConnecting)
		return;

	SL_PROTO->dbg(SL_PROTO, "Found queued PeerConnReq. Connecting...");
	req->state = SLReqConnecting;
	((SLPeerConn *)sl_peer_connect(req->peer, 0, req->conn_type, FALSE)->udata)->req = req;
}

static void cb_failed_and_free(SLPeerConnReq *req, void **args)
{
	req->cb_failed(req->udata);
	sl_peer_conn_req_free(req);
}

// receives a reply to a GetPeerAddress request
void sl_session_receive_get_peer_address(SLSession *session, uint8_t *data)
{
#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Received GetPeerAddress packet.");
#endif /* HEAVY_DEBUG */

	int error;
	
	sl_string *username;
	uint32_t     ip;
	uint32_t     port;
	
	SLPacket     *packet;
	SLPeer       *peer;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	username = sl_packet_get_string(packet);
	ip       = sl_swap_byte_order(sl_packet_get_integer(packet, &error));
	port     = sl_packet_get_integer(packet, &error);

	if(username == NULL || error)
	{
		SL_PROTO->dbg(SL_PROTO, "Received malformed GetPeerAddress response.");
		return;
	}

	if((peer = sl_find_peer(username)) == NULL)
	{
		SL_PROTO->dbg(SL_PROTO, "Server told us the Addr of a peer we don't even know about. This should not happen");
		if(ip != 0)
			peer = sl_peer_new(username);
	}

	if(peer)
		sl_peer_set_ip_port(peer, ip, port);

	if(ip != 0)
		list_foreach(peer->requests, (ListForeachFunc)connect_if_waiting, NULL);
	else
	{
		list_foreach(peer->requests, (ListForeachFunc)cb_failed_and_free, NULL);
		peer->requests = list_free(peer->requests);
	}
		
	sl_packet_destroy(packet);
}

// receives a request from a peer to connect to him
void sl_session_receive_connect_to_peer_request(SLSession *session, uint8_t *data)
{
	int error;
	
	sl_string *username;
	sl_string *type_str;
	
	uint32_t     ip;
	uint32_t     port;
	uint32_t     token;
	
	SLPacket       *packet;
	SLPeer         *peer;
	SLPeerConnType  type;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	username = sl_packet_get_string(packet);
	type_str = sl_packet_get_string(packet);
	ip       = sl_swap_byte_order(sl_packet_get_integer(packet, &error));
	port     = sl_packet_get_integer(packet, &error);
	token    = sl_packet_get_integer(packet, &error);

	if(username == NULL || type_str == NULL || error)
	{
		SL_PROTO->dbg(SL_PROTO, "Received malformed ConnectToPeer request.");
		sl_packet_destroy(packet);
		return;
	}

	switch(type_str->contents[0])
	{
		case 'P': type = SLConnPeer;         break;
		case 'F': type = SLConnFileTransfer; break;
		default:
			SL_PROTO->dbg(SL_PROTO, "Received malformed ConnectToPeer request.");
			return;
	}	

	SL_PROTO->dbg(SL_PROTO, "Received request to connect to %s (token: %d)",
			net_ip_str(ip), token);

	if((peer = sl_find_peer(username)) == NULL)
		peer = sl_peer_new(username);
	
	sl_peer_set_ip_port(peer, ip, port);
	
	if((sl_peer_connect(peer, token, type, TRUE)) == NULL)
	{
		SL_PROTO->dbg(SL_PROTO, "Couldn't create peer connection.");
		sl_session_send_cant_connect_to_peer(session, token, username);
		sl_packet_destroy(packet);
		return;
	}

	sl_packet_destroy(packet);
}

/*****************************************************************************/
