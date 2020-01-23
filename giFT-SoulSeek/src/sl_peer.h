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

#ifndef __SL_PEER_H
#define __SL_PEER_H

#include "sl_soulseek.h"
#include "sl_string.h"

/*****************************************************************************/

#define MAX_PEER_CONNECTIONS  64
#define SL_PEER_PORT 1634

/*****************************************************************************/

typedef enum
{
	SLConnUnknown,
	SLConnPeer,
	SLConnFileTransfer
} SLPeerConnType;

typedef enum
{
	SLReqUnknown,
	SLReqWaiting,
	SLReqConnecting
} SLPeerConnReqState;

typedef struct
{
	sl_string *username;

	uint32_t     ip;
	uint32_t     port;

	uint8_t      free_slots;
	uint32_t     speed;
	uint32_t     in_queue;
	
	List        *conns;
	List        *requests;
} SLPeer;

typedef void (*SLPeerConnReqConnReadyCallback) (TCPC *peer_conn, void *udata);
typedef void (*SLPeerConnReqFailedCallback)    (void *udata);

typedef struct
{
	SLPeer *peer;

	SLPeerConnType                 conn_type;
	SLPeerConnReqConnReadyCallback cb_conn_ready;
	SLPeerConnReqFailedCallback    cb_failed;
	SLPeerConnReqState             state;
	
	void *udata;
} SLPeerConnReq;

typedef struct
{
	SLPeer         *peer;
	SLPeerConnType  type;
	
	uint32_t          token;
	BOOL              pushed;

	SLPeerConnReq  *req;
} SLPeerConn;

#define PEER_CONN(c) ((SLPeerConn *)c->udata)

typedef struct
{
	SLPeer **peers;
	int numpeers;
} SLPeerList;

/*****************************************************************************/

// creates new peer
SLPeer *sl_peer_new(sl_string *username);

// searches for peer in peerlist by username
SLPeer *sl_find_peer(sl_string *username);

// returns a connection to a peer
TCPC *sl_peer_get_conn(SLPeer *peer, SLPeerConnType type);

// sets ip & port of a peer
void sl_peer_set_ip_port(SLPeer *peer, uint32_t ip, uint32_t port);

// creates a new peer connection
TCPC *sl_peer_connect(SLPeer *peer, uint32_t token,
                        SLPeerConnType type, BOOL pushed);

// requests a peer connection of given type and invokes a callback, passing 
// udata, when its up
void sl_peer_req_conn(SLPeer *peer, SLPeerConnType conn_type,
                        SLPeerConnReqConnReadyCallback cb_conn_ready,
			SLPeerConnReqFailedCallback cb_failed,
			void *udata);

// creates a new PeerConnRequest object
SLPeerConnReq *sl_peer_conn_req_new(SLPeer *peer, SLPeerConnType conn_type,
                                        SLPeerConnReqConnReadyCallback cb_conn_ready, 
					SLPeerConnReqFailedCallback cb_failed,
					void *udata);

// frees a PeerConnRequest object
void sl_peer_conn_req_free(SLPeerConnReq *req);

// queue a request
void sl_peer_add_conn_req(SLPeer *peer, SLPeerConnReq *req);

// destroys a PeerConn (closing the tcp connection and removing it from
// peer->conns
void sl_peer_conn_destroy(TCPC *peer_conn);

// sends a PeerInit packet
void sl_peer_conn_send_peer_init(TCPC *peer_conn);

// sends a PierceFW packet
void sl_peer_conn_send_pierce_fw(TCPC *peer_conn);

// receives a UserInfoRequest packet
void sl_peer_conn_receive_user_info_request(TCPC *peer_conn, unsigned char *data);

// receives a SharedFileList packet
void sl_peer_conn_receive_shared_file_list(TCPC *peer_conn, unsigned char *data);

// receives a GetSharedFileList packet
void sl_peer_conn_receive_get_shared_file_list(TCPC *peer_conn, unsigned char *data);

// sends SharedFileList packet
void sl_peer_conn_send_shared_file_list(TCPC *peer_conn);

// sends UserInfoReply packet
void sl_peer_conn_send_user_info_reply(TCPC *peer_conn);

// sends a GetSharedList packet
void sl_peer_conn_send_get_shared_file_list(TCPC *peer_conn);

// receives a PeerInit packet
void sl_peer_conn_receive_peer_init(TCPC *peer_conn, uint8_t *data);

// receives a FileSearchResult packet
void sl_peer_conn_receive_file_search_result(TCPC *peer_conn, unsigned char *data);

// destroy a peer (automatically disconnects and removes itself from the list)
void sl_peer_destroy(SLPeer *peer);

/*****************************************************************************/

// creates a peer list
SLPeerList *sl_peer_list_create();

// adds a peer to the list
int sl_peer_list_add(SLPeerList *peerlist, SLPeer *peer);

// removes a peer from the list
int sl_peer_list_remove(SLPeerList *peerlist, SLPeer *peer);

// destroys a peerlist
void sl_peer_list_destroy(SLPeerList *peerlist);

/*****************************************************************************/

#endif /* __SL_HTTP_H */
