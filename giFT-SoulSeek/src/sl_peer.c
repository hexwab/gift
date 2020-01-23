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
#include "sl_meta.h"
#include "sl_packet.h"
#include "sl_peer.h"
#include "sl_search.h"
#include "sl_utils.h"
#include "sl_filelist.h"

#include <libgift/mime.h>

/*****************************************************************************/

static void sl_peer_conn_cb_connect(int fd, input_id input, TCPC *peer_conn);
static void sl_peer_conn_cb_read(int fd, input_id input, TCPC *peer_conn);

static SLPeerList *peerlist = NULL;
static TCPC *incoming;
static uint32_t current_token;

/*****************************************************************************/

// called to handle incoming connection requests
static void handle_incoming(int fd, input_id id, TCPC *c)
{
	TCPC *peer_conn;

	assert(peerlist != NULL);

	if(!(peer_conn = tcp_accept(c, FALSE)))
		return;

#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Incoming peer connection accepted. (%s)", net_ip_str(peer_conn->host));
#endif /* HEAVY_DEBUG */

	input_add(peer_conn->fd, peer_conn, INPUT_WRITE,
	          (InputCallback) sl_peer_conn_cb_connect, FALSE);
}

// called when a peer connection is initialized
static void sl_peer_conn_cb_connect(int fd, input_id input, TCPC *peer_conn)
{
	assert(peer_conn != NULL);

	// check whether the socket is still open
	if(fd == -1 || !input || net_sock_error(fd))
	{
#ifdef HEAVY_DEBUG
		SL_PROTO->dbg(SL_PROTO, "Peer connection failed or closed.");
#endif /* HEAVY_DEBUG */
		sl_peer_conn_destroy(peer_conn);
		return;
	}

	if(peer_conn->outgoing)
	{
		assert(peer_conn->udata != NULL);
		
		if(((SLPeerConn *)peer_conn->udata)->pushed == TRUE)
			sl_peer_conn_send_pierce_fw(peer_conn);
		else
		{
			SLPeerConnReq *req = ((SLPeerConn *)peer_conn->udata)->req;

			assert(req != NULL);
			
			sl_peer_conn_send_peer_init(peer_conn);
			req->cb_conn_ready(peer_conn, req->udata);
			sl_peer_conn_req_free(req);
		}
	}

	input_remove(input);
	input_add(fd, peer_conn, INPUT_READ, (InputCallback) sl_peer_conn_cb_read, 30 * SECONDS);
}

// called when a peer connection is initialized
static void sl_peer_conn_cb_read(int fd, input_id input, TCPC *peer_conn)
{
	FDBuf *buf;
	uint32_t type;
	uint32_t length;
	uint8_t *data;
	int n;

	assert(peer_conn != NULL);

	// check whether the socket is still open
	if(fd == -1 || !input || net_sock_error(fd))
	{
		sl_peer_conn_destroy(peer_conn);
		return;
	}

	// get the buffer
	buf = tcp_readbuf(peer_conn);
	length = buf->flag + sizeof(uint32_t);

	if((n = fdbuf_fill(buf, length)) < 0)
	{
		sl_peer_conn_destroy(peer_conn);
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
		if((type & 0x000000ff) == 0 || (type & 0x000000ff) == 1)
			type &= 0x000000ff;
		switch(type)
		{
			case SLPeerInit:
				sl_peer_conn_receive_peer_init(peer_conn, data);
				break;

			case SLSharedFileList:
				sl_peer_conn_receive_shared_file_list(peer_conn, data);
				break;

			case SLGetSharedFileList:
				sl_peer_conn_receive_get_shared_file_list(peer_conn, data);
				break;

			case SLUserInfoRequest:
				sl_peer_conn_receive_user_info_request(peer_conn, data);
				break;

			case SLFileSearchResult:
				sl_peer_conn_receive_file_search_result(peer_conn, data);
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

// creates new peer
SLPeer *sl_peer_new(sl_string *username)
{
	SLPeer *peer = (SLPeer *) MALLOC(sizeof(SLPeer));

	assert(username != NULL);

	peer->conns      = NULL;
	peer->requests   = NULL;

	peer->username   = sl_string_copy(username);

	peer->ip         = 0;
	peer->port       = 0;

	peer->free_slots = 0;
	peer->speed      = 0;
	peer->in_queue   = 0;

	sl_peer_list_add(peerlist, peer);

	return peer;
}

// searches for peer in peerlist by username
SLPeer *sl_find_peer(sl_string *username)
{
	int i;

	for (i = 0; i < peerlist->numpeers; i++)
	{
		if (!sl_string_cmp(username, peerlist->peers[i]->username))
			return peerlist->peers[i];
	}

	return NULL;
}

static int check_peer_conn_for_type(TCPC *peer_conn, SLPeerConnType *type)
{
	if (peer_conn->udata == NULL)
		return 0;

	if (((SLPeerConn *)peer_conn->udata)->pushed == *type)
		return -1;
	else
		return 0;
}

// returns an existing connection to a peer
TCPC *sl_peer_get_conn(SLPeer *peer, SLPeerConnType type)
{
	List *l = list_find_custom(peer->conns, &type, (CompareFunc)check_peer_conn_for_type);

	if(l == NULL)
		return NULL;
	else
		return l->data;
}

// sets ip & port of a peer
void sl_peer_set_ip_port(SLPeer *peer, uint32_t ip, uint32_t port)
{
	assert(peer != NULL);
	
	peer->ip   = ip;
	peer->port = port;
}

static SLPeerConn *create_peer_conn(SLPeer *peer,
                                      SLPeerConnType type,
                                      uint32_t token, BOOL pushed)
{
	SLPeerConn *udata = MALLOC(sizeof(SLPeerConn));

	udata->peer   = peer;
	udata->token  = token;
	udata->type   = type;
	udata->pushed = pushed;
	udata->req    = NULL;

	return udata;
}

static void free_peer_conn(TCPC *peer_conn)
{
	free(peer_conn->udata);
}


// creates a new peer connection
TCPC *sl_peer_connect(SLPeer *peer, uint32_t token,
                        SLPeerConnType type, BOOL pushed)
{
	TCPC *peer_conn;
	
	assert(peer->port > 0);
	
	if((peer_conn = tcp_open(peer->ip, peer->port, FALSE)) == NULL)
		return NULL;
	
	peer_conn->udata = create_peer_conn(peer, type, token, pushed);
	peer->conns = list_append(peer->conns, peer_conn);
	
	input_add(peer_conn->fd, peer_conn, INPUT_WRITE,
		  (InputCallback) sl_peer_conn_cb_connect, 30 * SECONDS);
	
	return peer_conn;
}

// requests a peer connection of given type and invokes a callback, passing 
// udata, when its up
void sl_peer_req_conn(SLPeer *peer, SLPeerConnType conn_type,
                        SLPeerConnReqConnReadyCallback cb_conn_ready, 
			SLPeerConnReqFailedCallback cb_failed,
			void *udata)
{
	TCPC *peer_conn;

	SL_PROTO->dbg(SL_PROTO, "PeerConn requested");

	if((peer_conn = sl_peer_get_conn(peer, conn_type)) == NULL)
	{
		SLPeerConnReq *req;

		SL_PROTO->dbg(SL_PROTO, "No PeerConn found. Queueing PeerConnReq...");
		
		req = sl_peer_conn_req_new(peer, conn_type, cb_conn_ready, cb_failed, udata);
		sl_peer_add_conn_req(peer, req);

#if 0
		if(!(strcmp(peer->username->contents, "flexo_")))
		{
			peer->ip = net_ip("127.0.0.1");
			peer->port = 2234;
		}
#endif

		if(peer->port > 0)
		{
			SL_PROTO->dbg(SL_PROTO, "PeerAddr is known. Connecting...");
			req->state = SLReqConnecting;
			((SLPeerConn *)sl_peer_connect(peer, 0, conn_type, FALSE)->udata)->req = req;
		}
		else
		{
			SL_PROTO->dbg(SL_PROTO, "PeerAddr unknown. Asking server...");
			req->state = SLReqWaiting;
			sl_session_send_get_peer_address(SL_SESSION, peer->username);
		}
	}
	else
	{
		SL_PROTO->dbg(SL_PROTO, "Reusing open connection. Invoking CB...");
		cb_conn_ready(peer_conn, udata);
	}
}

// creates a new PeerConnRequest object
SLPeerConnReq *sl_peer_conn_req_new(SLPeer *peer, SLPeerConnType conn_type,
                                        SLPeerConnReqConnReadyCallback cb_conn_ready, 
                                        SLPeerConnReqFailedCallback    cb_failed,
					void *udata)
{
	SLPeerConnReq *req;

	req = MALLOC(sizeof(SLPeerConnReq));

	req->peer          = peer;
	req->conn_type     = conn_type;
	req->cb_conn_ready = cb_conn_ready;
	req->cb_failed     = cb_failed;
	req->udata         = udata;
	req->state         = SLReqUnknown;

	return req;
}

// frees a PeerConnRequest object
void sl_peer_conn_req_free(SLPeerConnReq *req)
{
	free(req->udata);
	free(req);
}

// queue a request
void sl_peer_add_conn_req(SLPeer *peer, SLPeerConnReq *req)
{
	peer->requests = list_append(peer->requests, req);
}

// destroys a PeerConn (closing the tcp connection and removing it from
// peer->conns
void sl_peer_conn_destroy(TCPC *c)
{
	if ((PEER_CONN(c) != NULL) &&
	    (PEER_CONN(c)->peer != NULL))
		PEER_CONN(c)->peer->conns = list_remove(PEER_CONN(c)->peer->conns, c);
	free_peer_conn(c);
	tcp_close(c);
}

// sends a PeerInit packet
void sl_peer_conn_send_peer_init(TCPC *peer_conn)
{
	sl_string *type_str;

	assert(peer_conn != NULL);
	assert(((SLPeerConn *)peer_conn->udata)->type != SLConnUnknown);

	if(((SLPeerConn *)peer_conn->udata)->type == SLConnPeer)
		type_str = sl_string_create_with_contents("P");
	else
		type_str = sl_string_create_with_contents("F");

	SLPacket *packet = sl_packet_create();
	sl_packet_set_type_byte(packet);
	sl_packet_set_type(packet, SLPeerInit);
	sl_packet_insert_string(packet, SL_PLUGIN->username);
	sl_packet_insert_string(packet, type_str);
	sl_packet_insert_integer(packet, current_token);
	sl_packet_send(peer_conn, packet);

#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Sent PeerInit");
#endif /* HEAVY_DEBUG */
	
	sl_packet_destroy(packet);

	((SLPeerConn *)peer_conn->udata)->token = current_token;
	current_token++;
}

// sends a PierceFW packet
void sl_peer_conn_send_pierce_fw(TCPC *peer_conn)
{
	assert(peer_conn != NULL);

	/* create the PierceFW packet */

	SLPacket *packet;

	packet = sl_packet_create();
	sl_packet_set_type_byte(packet);

	sl_packet_set_type(packet, SLPierceFW);
	sl_packet_insert_integer(packet, ((SLPeerConn *)peer_conn->udata)->token);

	sl_packet_send(peer_conn, packet);

#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Sent PierceFW (token: %d)", ((SLPeerConn *)peer_conn->udata)->token);
#endif /* HEAVY_DEBUG */

	sl_packet_destroy(packet);
}

// receives a UserInfoRequest packet
void sl_peer_conn_receive_user_info_request(TCPC *peer_conn, unsigned char *data)
{
#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Got UserInfoRequest");
#endif /* HEAVY_DEBUG */

	sl_peer_conn_send_user_info_reply(peer_conn);
}

// receives a SharedFileList packet
void sl_peer_conn_receive_shared_file_list(TCPC *peer_conn, unsigned char *data)
{
	SLPacket     *packet;
	List         *files;

	uint32_t num_dirs;
	int      error;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	if(sl_packet_decompress_block(packet) == FALSE)
	{
		SL_PROTO->dbg(SL_PROTO, "Decompressing SharedFileList failed.");
		return;
	}

	num_dirs = sl_packet_get_integer(packet, &error);

	if(error == TRUE)
	{
		SL_PROTO->dbg(SL_PROTO, "Received malformed SharedFileList packet.");
		sl_packet_destroy(packet);
		return;
	}

	SL_PROTO->dbg(SL_PROTO, "Received SharedFileList (%d dirs)", num_dirs);

	while(num_dirs--)
	{
		sl_string *dir = sl_packet_get_string(packet);
		
		SL_PROTO->dbg(SL_PROTO, "SharedDir: %s", dir->contents);

		files = sl_filelist_new_from_packet(packet);
	}

	/* sl_filelist_report(files, search_item->event, peer); */

	/* FIXME: free the lists */

	sl_packet_destroy(packet);
}

// receives a GetSharedFileList packet
void sl_peer_conn_receive_get_shared_file_list(TCPC *peer_conn, unsigned char *data)
{
#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Got GetSharedFileList");
#endif /* HEAVY_DEBUG */

	sl_peer_conn_send_shared_file_list(peer_conn);
}

// sends SharedFileList packet
void sl_peer_conn_send_shared_file_list(TCPC *peer_conn)
{
	assert(peer_conn != NULL);

	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLSharedFileList);
	sl_packet_insert_integer(packet, 0); /* number of directories (FIXME) */
	sl_packet_send(peer_conn, packet);
	sl_packet_destroy(packet);
}

// sends UserInfoReply packet
void sl_peer_conn_send_user_info_reply(TCPC *peer_conn)
{
	/* create the UserInfoReply packet */

	char       *pic_path;
	FILE       *f;
	SLPacket *packet;

	packet = sl_packet_create();

	sl_packet_set_type(packet, SLUserInfoReply);
	sl_packet_insert_string(packet, sl_string_create_with_contents(config_get_str(SL_PLUGIN->conf, "userinfo/description")));
	pic_path = config_get_str(SL_PLUGIN->conf, "userinfo/picture");

	if((pic_path) && (strlen(pic_path) > 0) &&
	   (f = fopen(pic_path, "r")))
	{
		struct stat statinfo;
		sl_string *str;

		sl_packet_insert_byte(packet, 1);

		fstat(fileno(f), &statinfo);

		str = sl_string_create_with_size(statinfo.st_size);

		str->length = statinfo.st_size; /* FIXME: should this be done in sl_string_create_with_size() maybe? */
		fread(str->contents, statinfo.st_size, 1, f);

		sl_packet_insert_string(packet, str);

		fclose(f);
	}
	else
		sl_packet_insert_byte(packet, 0);

	sl_packet_insert_integer(packet, 0); /* FIXME: Total uploads */
	sl_packet_insert_integer(packet, 0); /* FIXME: Queue size */
	sl_packet_insert_byte(packet, 0);    /* FIXME: Slots available */

#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Sent UserInfoReply");
#endif /* HEAVY_DEBUG */

	sl_packet_send(peer_conn, packet);

	sl_packet_destroy(packet);
}

// sends a GetSharedList packet
void sl_peer_conn_send_get_shared_file_list(TCPC *peer_conn)
{
	assert(peer_conn != NULL);

	SLPacket *packet = sl_packet_create();
	sl_packet_set_type(packet, SLGetSharedFileList);
	sl_packet_send(peer_conn, packet);
	sl_packet_destroy(packet);

	SL_PROTO->dbg(SL_PROTO, "GetSharedFileList packet sent.");
}

// receives a PeerInit packet
void sl_peer_conn_receive_peer_init(TCPC *peer_conn, uint8_t *data)
{
	int error;
	sl_string *username;
	sl_string *type_str;
	uint32_t token;
	SLPacket *packet;
	SLPeer *peer;
	SLPeerConnType type;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	sl_packet_set_type_byte(packet);

	username = sl_packet_get_string(packet);
	type_str = sl_packet_get_string(packet);
	token    = sl_packet_get_integer(packet, &error);

	if(username == NULL || type_str == NULL || error == TRUE)
	{
		SL_PROTO->dbg(SL_PROTO, "Received malformed PeerInit packet.");
		sl_packet_destroy(packet);
		return;
	}

	switch(type_str->contents[0])
	{
		case 'P': type = SLConnPeer;         break;
		case 'F': type = SLConnFileTransfer; break;
		default:
			SL_PROTO->dbg(SL_PROTO, "Received malformed PeerInit packet. (ConnType: '0x%.2x')", type_str->contents[0]);
			sl_packet_destroy(packet);
			return;
			break;
	}			

	assert(peer_conn->udata == NULL); /* FIXME: We should not do this. */

	if((peer = sl_find_peer(username)) == NULL)
		peer = sl_peer_new(username);

	if(peer->username == NULL)
		peer->username = sl_string_copy(username);

	peer_conn->udata = create_peer_conn(peer, type, token, FALSE);

	sl_packet_destroy(packet);
}

// receives a FileSearchResult packet
void sl_peer_conn_receive_file_search_result(TCPC *peer_conn, unsigned char *data)
{
	SLPacket     *packet;
	sl_string    *username;
	SLSearchList *search_item;
	SLPeer       *peer;
	List         *files;

	uint32_t token;
	int      error;

	if((packet = sl_packet_create_from_data(data)) == NULL)
		return;

	if(sl_packet_decompress_block(packet) == FALSE)
		return;

	username = sl_packet_get_string (packet);
	token    = sl_packet_get_integer(packet, &error);

	if(username == NULL || error == TRUE)
	{
		SL_PROTO->dbg(SL_PROTO, "Received malformed FileSearchResult packet.");
		sl_packet_destroy(packet);
		return;
	}

	search_item = sl_search_get_search(token);

	if(search_item == NULL)
	{
		sl_packet_destroy(packet);
		return;
	}

	if((peer = sl_find_peer(username)) == NULL)
		peer = sl_peer_new(username);

	files = sl_filelist_new_from_packet(packet);

	peer->free_slots = sl_packet_get_byte   (packet, &error);
	peer->speed      = sl_packet_get_integer(packet, &error);
	peer->in_queue   = sl_packet_get_integer(packet, &error);

	if(error == TRUE)
	{
		SL_PROTO->dbg(SL_PROTO, "Received malformed FileSearchResult packet.");
		sl_packet_destroy(packet);
		return;
	}

	sl_filelist_report(files, search_item->event, peer);

	/* FIXME: free the list */

	sl_packet_destroy(packet);
}

static void close_peer_conn(TCPC *c, void **args)
{
	tcp_close(c);
}

// destroy a peer (automatically disconnects and removes itself from the list)
void sl_peer_destroy(SLPeer *peer)
{
	assert(peer != NULL);
	
#ifdef HEAVY_DEBUG
	SL_PROTO->dbg(SL_PROTO, "Destroying peer...");
#endif /* HEAVY_DEBUG */

	list_foreach(peer->conns, (ListForeachFunc)close_peer_conn, NULL);

	sl_peer_list_remove(peerlist, peer);
	
	free(peer);
}

/*****************************************************************************/

// creates a peer list
SLPeerList *sl_peer_list_create()
{
	if(peerlist != NULL)
	{
		sl_peer_list_destroy(peerlist);
	}

	peerlist = MALLOC(sizeof(SLPeerList));

	peerlist->peers = MALLOC(MAX_PEER_CONNECTIONS * sizeof(SLPeer *));
	peerlist->numpeers = 0;

	current_token = rand() % 4096;

	if(!(incoming = tcp_bind(SL_PEER_PORT, FALSE)))
		return FALSE;

	input_add(incoming->fd, incoming, INPUT_READ,
	          (InputCallback) handle_incoming, FALSE);

	return peerlist;
}

// adds a peer to the list
int sl_peer_list_add(SLPeerList *peerlist, SLPeer *peer)
{
	assert(peerlist != NULL);
	assert(peer != NULL);

	if(peerlist->numpeers == MAX_PEER_CONNECTIONS)
		return FALSE;

	peerlist->peers[peerlist->numpeers] = peer;

	peerlist->numpeers++;

	return TRUE;
}

// removes a peer from the list
int sl_peer_list_remove(SLPeerList *peerlist, SLPeer *peer)
{
	assert(peerlist != NULL);
	assert(peer != NULL);

	int i;
	for(i = 0; i < peerlist->numpeers; i++)
	{
		if(peerlist->peers[i] == peer)
		{
			peerlist->peers[i] = peerlist->peers[peerlist->numpeers - 1];
			peerlist->numpeers--;
			return TRUE;
		}
	}
	return FALSE;
}

// destroys a peerlist
void sl_peer_list_destroy(SLPeerList *peerlist)
{
	assert(peerlist != NULL);

	tcp_close(incoming);

	int i;
	for(i = peerlist->numpeers - 1; i >= 0; i--)
	{
		sl_peer_destroy(peerlist->peers[i]);
	}
	free(peerlist->peers);
	free(peerlist);
	peerlist = NULL;
}

/*****************************************************************************/
