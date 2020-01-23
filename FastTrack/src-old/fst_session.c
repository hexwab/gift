/*
 * $Id: fst_session.c,v 1.5 2003/07/10 19:38:11 mkern Exp $
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

#include "fst_fasttrack.h"
#include "fst_session.h"

/*****************************************************************************/

char *valid_network_names[] = { "kazaa", "fileshare", "grokster", NULL};

/*****************************************************************************/

static void session_connected(int fd, input_id input, FSTSession *session);
static void session_decrypt_packet(int fd, input_id input, FSTSession *session);
static int session_do_handshake(FSTSession *session);
static int session_greet_suppernode(FSTSession *session);
static int session_send_pong(FSTSession *session);
static int session_send_ping(FSTSession *session);

/*****************************************************************************/

// allocate and init session
FSTSession *fst_session_create(FSTSessionCallback callback)
{
	FSTSession *session;

	session = malloc(sizeof(FSTSession));

	session->in_cipher = fst_cipher_create();
	session->out_cipher = fst_cipher_create();
	session->in_xinu = 0x51;
	session->out_xinu = 0x51;

	session->in_packet = fst_packet_create();

	session->state = SessNew;
	session->tcpcon = NULL;
	session->node = NULL;

	// allocate and init session
	session->callback = (int (*)(void *session, FSTSessionMsg msg_type, FSTPacket *msg_packet))callback;

	return session;
}

// free session
void fst_session_free(FSTSession *session)
{
	if(!session)
		return;

	fst_cipher_free(session->in_cipher);
	fst_cipher_free(session->out_cipher);
	
	fst_packet_free(session->in_packet);

	tcp_close(session->tcpcon);

	fst_node_free (session->node);

	free(session);
}

// connect to node, node is automatically freed in fst_session_free()
int fst_session_connect(FSTSession *session, FSTNode *node)
{
	struct hostent *he;

	if(!session || session->state != SessNew || !node)
		return FALSE;

	session->state = SessConnecting;

	// TODO: make this non-blocking
	if (!(he = gethostbyname (node->host)))
	{
		session->state = SessDisconnected;
		FST_WARN_1 ("gethostbyname failed for host %s", node->host);
		return FALSE;
   }

	FST_DBG_3 ("connecting to %s(%s):%d", node->host, net_ip_str(*((in_addr_t*)he->h_addr_list[0])), node->port);

	session->tcpcon = tcp_open (*((in_addr_t*)he->h_addr_list[0]), node->port, FALSE);

	if (!session->tcpcon)
	{
		session->state = SessDisconnected;
		FST_WARN_1 ("tcp_open() failed for %s. no route to host?", node->host);
		return FALSE;
	}
	
	session->tcpcon->udata = (void*)session;
	session->node = node;

	input_add (session->tcpcon->fd, (void*)session, INPUT_WRITE, (InputCallback) session_connected, FST_SESSION_CONNECT_TIMEOUT);

	return TRUE;
}

// disconnect session
int fst_session_disconnect(FSTSession *session)
{
	if(!session)
		return FALSE;

	tcp_close(session->tcpcon);
	session->tcpcon = NULL;

	session->state = SessDisconnected;
	
	FST_DBG_2 ("disconnected from %s:%d", session->node->host, session->node->port);

	// call back
	session->callback (session, SessMsgDisconnected, NULL);

	return TRUE;
}

// sends 0x4B packet over session
int fst_session_send_message(FSTSession *session, FSTSessionMsg msg_type, FSTPacket *msg_data)
{
	FSTPacket *new_packet;
    unsigned char hi_len, lo_len;
    int xtype;

	if(!session || session->state != SessEstablished || msg_type > 0xFF || !msg_data)
	{
		FST_DBG_4 ("cannot happen: fst_session_send_message() failed! msg_type = 0x%02X, "
			   "session = 0x%08X, session->state = %d, msg_data = 0x%08X",
			   msg_type, session, session->state, msg_data);
		return FALSE;
	}

	FST_HEAVY_DBG_1 ("sending msg with msg_type: 0x%02X", msg_type);

	new_packet = fst_packet_create();

    lo_len = fst_packet_size(msg_data) & 0xff;
    hi_len = (fst_packet_size(msg_data) >> 8) & 0xff;

	fst_packet_put_uint8 (new_packet, 0x4B); // 'K'
	
    xtype = session->out_xinu % 3;

    switch(xtype) {
	case 0:
		fst_packet_put_uint8(new_packet, msg_type);
		fst_packet_put_uint8(new_packet, 0);
		fst_packet_put_uint8(new_packet, hi_len);
		fst_packet_put_uint8(new_packet, lo_len);
	    break;
	case 1:
		fst_packet_put_uint8(new_packet, 0);
		fst_packet_put_uint8(new_packet, hi_len);
		fst_packet_put_uint8(new_packet, msg_type);
		fst_packet_put_uint8(new_packet, lo_len);
	    break;
	case 2:
		fst_packet_put_uint8(new_packet, 0);
		fst_packet_put_uint8(new_packet, lo_len);
		fst_packet_put_uint8(new_packet, hi_len);
		fst_packet_put_uint8(new_packet, msg_type);
	    break;
    }

	// update xinu state
    session->out_xinu ^= ~(fst_packet_size(msg_data) + msg_type);

	fst_packet_append (new_packet, msg_data);
//	print_bin_data(new_packet->data, new_packet->used);
	fst_packet_encrypt (new_packet, session->out_cipher);

	if(fst_packet_send (new_packet, session->tcpcon) == FALSE)
	{
		fst_packet_free(new_packet);
		return FALSE;
	}

	fst_packet_free(new_packet);
	return TRUE;
}

/*****************************************************************************/


static void session_connected(int fd, input_id input, FSTSession *session)
{
	FSTPacket *packet;

	if (net_sock_error (session->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("couldn't connect to %s:%d", session->node->host, session->node->port);
		fst_session_disconnect (session);
		return;
	}

	input_remove (input);

	// notify plugin
	session->callback (session, SessMsgConnected, NULL);

	// go on with handshake
	session->state = SessHandshaking;
	session->out_cipher->enc_type = 0x29;
	session->out_cipher->seed = 0x0FACB1238; // random number?

	packet = fst_packet_create();

	fst_packet_put_uint32 (packet, htonl(0xFA00B62B)); // random number?
	fst_packet_put_uint32 (packet, htonl(session->out_cipher->seed));
	fst_packet_put_uint32 (packet, htonl(fst_cipher_encode_enc_type(session->out_cipher->seed, session->out_cipher->enc_type)));
 
	FST_HEAVY_DBG ("sending initial packet");

	if(fst_packet_send (packet, session->tcpcon))
	{
		// change the input callback to session_decrypt_packet
		input_add (session->tcpcon->fd, (void*)session, INPUT_READ, (InputCallback) session_decrypt_packet, FST_SESSION_HANDSHAKE_TIMEOUT);
	}
	else
	{
		FST_DBG ("session_connected: fst_packet_send() failed");
		fst_session_disconnect (session);
	}

	fst_packet_free (packet);
}


static void session_decrypt_packet(int fd, input_id input, FSTSession *session)
{
	FSTPacket *packet;

	input_remove(input);

	if(net_sock_error (session->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("socket error for %s:%d", session->node->host, session->node->port);
		fst_session_disconnect (session);
		return;
	}

	packet = fst_packet_create();

	if(fst_packet_recv (packet, session->tcpcon) == FALSE)
	{
		fst_packet_free (packet);
		fst_session_disconnect (session);
		return;
	}

	if(session->state == SessHandshaking)
	{
		fst_packet_append (session->in_packet, packet);
		fst_packet_free (packet);

		if(fst_packet_size (session->in_packet) < 8)
		{
			FST_DBG_1 ("received insufficient data for calculating key, got %d bytes, waiting for more...", fst_packet_size(session->in_packet));
			// get more data
			input_add (session->tcpcon->fd, (void*)session,INPUT_READ, (InputCallback) session_decrypt_packet, FST_SESSION_HANDSHAKE_TIMEOUT);
			return;
		}

		FST_HEAVY_DBG ("handshaking...");

		if(session_do_handshake (session) == FALSE)
		{
			fst_session_disconnect (session);
			return;
		}

		fst_packet_truncate (session->in_packet);
		fst_packet_decrypt (session->in_packet, session->in_cipher);

		session->state = SessWaitingNetName;
	}
	else
	{
		fst_packet_decrypt (packet, session->in_cipher);
		fst_packet_append (session->in_packet, packet);
		fst_packet_free (packet);
	}

	FST_HEAVY_DBG_1 ("session->in_packet->mem_used = %d", fst_packet_size(session->in_packet));
//	print_bin_data(session->in_packet->data, session->in_packet->used);

	if(session->state == SessWaitingNetName)
	{
		char **net_name;
		fst_uint8 c = 'a';

		while(fst_packet_remaining(session->in_packet) && (c = fst_packet_get_uint8(session->in_packet)) != '\0')
			;
		if(c != '\0')
		{
			fst_packet_rewind(session->in_packet);
			// get more data
			input_add (session->tcpcon->fd, (void*)session,INPUT_READ, (InputCallback) session_decrypt_packet, FST_SESSION_HANDSHAKE_TIMEOUT);
			return;
		}

		// check for valid network name
		for(net_name=valid_network_names; *net_name; net_name++)
			if(strcasecmp (*net_name, session->in_packet->data) == 0)
				break;

		if(*net_name == NULL)
		{
			FST_WARN_1 ("Remote network name invalid: \"%s\". closing connection", session->in_packet->data);
			fst_session_disconnect (session);
			return;
		}

		FST_DBG_1 ("remote network name is \"%s\"", session->in_packet->data);

		session->state = SessEstablished;
		fst_packet_truncate(session->in_packet);

		// notify plugin
		session->callback (session, SessMsgEstablished, NULL);

		if(session_greet_suppernode(session) == FALSE)
		{
			fst_session_disconnect (session);
			return;
		}
	}
/*
	if(fst_packet_size(session->in_packet) == 0)
	{
		// get more data
		input_add (session->tcpcon->fd, (void*)session, INPUT_READ, (InputCallback) session_decrypt_packet, 0);
		return;
	}
*/
	while (fst_packet_remaining (session->in_packet))
	{
		unsigned int type = fst_packet_get_uint8 (session->in_packet);

		/* we got ping */
		if (type == 0x50)
		{
			fst_packet_truncate (session->in_packet);	
			FST_HEAVY_DBG ("got ping, sending pong");		
			session_send_pong (session);
			continue;
		}
		/* we got pong */
		if (type == 0x52)
		{
			fst_packet_truncate (session->in_packet);	
			FST_HEAVY_DBG ("got pong");
			continue;
		}
		/* we got a message */
		if(type == 0x4B)
		{
			int xtype = session->in_xinu % 3;
			unsigned int msg_type, msg_len;
			FSTPacket *packet;

			if(fst_packet_remaining (session->in_packet) < 5)
			{
				fst_packet_rewind (session->in_packet);
				FST_HEAVY_DBG ("didn't get the whole message header, waiting for more data");
				// get more data
				input_add (session->tcpcon->fd, (void*)session,INPUT_READ, (InputCallback) session_decrypt_packet, 0);
				return;
			}
			
			switch(xtype) {
			case 0:
				msg_type = fst_packet_get_uint8 (session->in_packet);
				fst_packet_get_uint8 (session->in_packet); // zero
				msg_len = ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_len |= (unsigned int)fst_packet_get_uint8 (session->in_packet);
				break;
			case 1:
				fst_packet_get_uint8 (session->in_packet); // zero
				msg_len = ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_type = fst_packet_get_uint8 (session->in_packet);
				msg_len |= (unsigned int)fst_packet_get_uint8 (session->in_packet);
				break;
			case 2:
				fst_packet_get_uint8 (session->in_packet); // zero
				msg_len = (unsigned int)fst_packet_get_uint8 (session->in_packet);
				msg_len |= ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_type = fst_packet_get_uint8 (session->in_packet);
				break;
			}

			FST_HEAVY_DBG_2 ("got message type = %02x, length = %d byte", msg_type, msg_len);

			if(fst_packet_remaining (session->in_packet) < msg_len)
			{
				fst_packet_rewind (session->in_packet);
				FST_HEAVY_DBG ("didn't get the whole message, waiting for more data");
				// get more data
				input_add (session->tcpcon->fd, (void*)session,INPUT_READ, (InputCallback) session_decrypt_packet, 0);
				return;
			}
			
			// modify xinu state
			session->in_xinu ^= ~(msg_type + msg_len);

			// prepare packet for callback
			packet = fst_packet_create_copy (session->in_packet, msg_len);
			fst_packet_truncate (session->in_packet);
	
			// call back
			if(session->callback (session, msg_type, packet) == FALSE)
			{
				// session was probably already freed by the handler, so do not access it
				fst_packet_free (packet);
				return;
			}

			fst_packet_free (packet);
			continue;
		}
		// oops, dunno what that packet is, maybe our decryption failed?
		FST_WARN_1 ("strange packet of type 0x%02X received. closing connection.", type);
		print_bin_data(session->in_packet->data, fst_packet_remaining(session->in_packet));

		fst_session_disconnect (session);
		return;
	}

	// get more data
	input_add (session->tcpcon->fd, (void*)session, INPUT_READ, (InputCallback) session_decrypt_packet, 0);
}

static int session_do_handshake(FSTSession *session)
{
	unsigned int seed, enc_type;
	FSTPacket *packet;
	
	if(fst_packet_size (session->in_packet) < 8)
		return FALSE;

	// get seed and enc_type
	seed = ntohl (fst_packet_get_uint32 (session->in_packet));
	enc_type = ntohl (fst_packet_get_uint32 (session->in_packet));
	enc_type = fst_cipher_decode_enc_type (seed, enc_type);

	// generate send key
	session->out_cipher->seed ^= seed; // xor send cipher with received seed
	if(!fst_cipher_init(session->out_cipher, session->out_cipher->seed, session->out_cipher->enc_type))
	{
		FST_WARN_1 ("Unsupported encryption: 0x%02X", session->out_cipher->enc_type);
		return FALSE;
	}

	// generate recv key
	if(!fst_cipher_init(session->in_cipher, seed, enc_type))
	{
		FST_WARN_1 ("Unsupported encryption: 0x%02X", enc_type);
		return FALSE;
	}

	FST_DBG_2 ("outgoing enc_type: 0x%02X, incoming enc_type: 0x%02X", session->out_cipher->enc_type, enc_type);

	// send network name
	FST_HEAVY_DBG_1 ("session_do_handshake: sending network name (\"%s\")", FST_NETWORK_NAME);

	packet = fst_packet_create();

	fst_packet_put_ustr (packet, FST_NETWORK_NAME, strlen(FST_NETWORK_NAME) + 1);
	fst_packet_encrypt (packet, session->out_cipher);

	if(fst_packet_send (packet, session->tcpcon) == FALSE)
	{
		fst_packet_free(packet);
		return FALSE;
	}

	fst_packet_free(packet);
	return TRUE;
}

static int session_greet_suppernode(FSTSession *session)
{
	FSTPacket *packet = fst_packet_create();
	struct sockaddr_in sa;
	int size = sizeof(struct sockaddr);
	
	FST_DBG ("sending ip, bandwidth and user name to supernode");

	// send our ip address and port
	getsockname (session->tcpcon->fd, (struct sockaddr *) &sa, &size);
	
	fst_packet_put_uint32 (packet, sa.sin_addr.s_addr); // ip
	fst_packet_put_uint16 (packet, htons (0x0000));  // port
	
	/* This next byte represents the user's advertised bandwidth, on
	* a logarithmic scale.  0xd1 represents "infinity" (actually,
	* 1680 kbps).  The value is approximately 14*log_2(x)+59, where
	* x is the bandwidth in kbps. */
	fst_packet_put_uint8 (packet, 0xb0);

	/* 1 byte: dunno. */
	fst_packet_put_uint8 (packet, 0x00);

	// user name
	fst_packet_put_ustr (packet, FST_USER_NAME, strlen(FST_USER_NAME)); // no trailing '\0'

	if(fst_session_send_message (session, 0x02, packet) == FALSE)
	{
		fst_packet_free (packet);
		return FALSE;
	}

	session_send_ping(session);

	fst_packet_free (packet);
	return TRUE;
}

// send out pong response
static int session_send_pong(FSTSession *session)
{
	FSTPacket *packet = fst_packet_create();
	int ret;

	fst_packet_put_uint8 (packet, 0x52);
	fst_packet_encrypt (packet, session->out_cipher);
	
	ret = fst_packet_send (packet, session->tcpcon);

	fst_packet_free (packet);

	return ret;
}

// send out ping request
static int session_send_ping(FSTSession *session)
{
	FSTPacket *packet = fst_packet_create();
	int ret;

	fst_packet_put_uint8 (packet, 0x50);
	fst_packet_encrypt (packet, session->out_cipher);
	
	ret = fst_packet_send (packet, session->tcpcon);

	fst_packet_free (packet);

	return ret;
}
