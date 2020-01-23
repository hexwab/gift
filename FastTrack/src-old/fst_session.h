/*
 * $Id: fst_session.h,v 1.3 2003/06/26 18:34:37 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_SESSION_H
#define __FST_SESSION_H

#include "fst_node.h"
#include "fst_packet.h"
#include "fst_crypt.h"

/*****************************************************************************/

// session states
typedef enum
{
	SessNew,
	SessConnecting,
	SessHandshaking,
	SessWaitingNetName,
	SessEstablished,
	SessDisconnected
} FSTSessionState;

// session messages sent to callback
typedef enum
{
	// our own stuff
	SessMsgConnected = 0x01FF,		// tcp connection established
	SessMsgEstablished = 0x02FF,	// encryption initialized and we received "KaZaA" network name
	SessMsgDisconnected = 0x03FF,	// tcp connection closed
	// FastTrack messages
	SessMsgNodeList = 0x00,
	SessMsgQuery = 0x06,
	SessMsgQueryReply = 0x07,
	SessMsgQueryEnd = 0x08,
	SessMsgNetworkStats = 0x09,
	SessMsgNetworkName = 0x1d,

} FSTSessionMsg;

typedef struct
{
	FSTCipher *in_cipher;
	FSTCipher *out_cipher;
	unsigned int in_xinu;
	unsigned int out_xinu;

	FSTPacket *in_packet;	// buffer for incoming data

	FSTSessionState state;
	TCPC *tcpcon;		// tcp connection
	FSTNode *node;			// coyp of node this session is connected to

//	FSTSessionCallback callback;
	int (*callback)(void *session, FSTSessionMsg msg_type, FSTPacket *msg_packet);
	
} FSTSession;

// if callback returns FALSE session is no longer save to use
typedef int (*FSTSessionCallback)(FSTSession *session, FSTSessionMsg msg_type, FSTPacket *msg_packet);

/*****************************************************************************/

// allocate and init session
FSTSession *fst_session_create(FSTSessionCallback callback);

// free session
void fst_session_free(FSTSession *session);

// connect to node, node is automatically freed in fst_session_free()
int fst_session_connect(FSTSession *session, FSTNode *node);

// disconnect session
int fst_session_disconnect(FSTSession *session);

// sends 0x4B packet over session
int fst_session_send_message(FSTSession *session, FSTSessionMsg msg_type, FSTPacket *msg_data);

/*****************************************************************************/

#endif /* __FST_SESSION_H */
