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

#ifndef __SL_SESSION_H
#define __SL_SESSION_H

#include "sl_soulseek.h"

/*****************************************************************************/

#define SL_SERVER_ADDR "38.115.131.131"
#define SL_SERVER_PORT "2240"

#define SL_VERSION 198

/*****************************************************************************/

// session states
typedef enum
{
	SLNew,
	SLConnecting,
	SLEstablished,
	SLDisconnected
} SLSessionState;

/**
 * SLSession structure
 */
typedef struct
{
	SLSessionState state;      // our status
	TCPC *tcp_conn;              // TCP connection to the server

	SLPacket *in_packets;      // buffer for incoming data
} SLSession;

/*****************************************************************************/

// allocates and returns a new session
SLSession *sl_session_create();

// connects to the network
int sl_session_connect(SLSession *session);

// disconnects from the network
void sl_session_disconnect(SLSession *session);

// destroys a previously allocated session
void sl_session_destroy(SLSession *session);

// sends a Login request
void sl_session_send_login_request(SLSession *session);

// tells the server on which port we're listening
void sl_session_send_set_wait_port(SLSession *session);

// sends a GetPeerAddress request
void sl_session_send_get_peer_address(SLSession *session, sl_string *username);

// receives a reply to the Login request
int sl_session_receive_login_confirmation(SLSession *session, uint8_t *data);

// receives a reply to a GetPeerAddress request
void sl_session_receive_get_peer_address(SLSession *session, uint8_t *data);

// receives a request from a peer to connect to him
void sl_session_receive_connect_to_peer_request(SLSession *session, uint8_t *data);

// tell the server that we couldn't connect to the peer he told us
void sl_session_send_cant_connect_to_peer(SLSession *session, uint32_t token, sl_string *username);

/*****************************************************************************/

#endif /* __SL_SESSION_H */
