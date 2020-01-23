/*
 * $Id: fst_session.c,v 1.34 2006/08/17 14:36:43 mkern Exp $
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

static void session_connected (int fd, input_id input, FSTSession *session);
static void session_decrypt_packet (int fd, input_id input, FSTSession *session);
static int session_do_handshake (FSTSession *session);
static void session_received_pong (FSTSession *session);
static BOOL session_ping (FSTSession *session);
static BOOL session_ping_timeout (FSTSession *session);
static int session_send_pong (FSTSession *session);
static int session_send_ping (FSTSession *session);

/*****************************************************************************/

/* allocate and init session */
FSTSession *fst_session_create (FSTSessionCallback callback)
{
	FSTSession *session;

	if (! (session = malloc (sizeof (FSTSession))))
		return NULL;

	memset (session, 0, sizeof (FSTSession));

	session->in_cipher = fst_cipher_create ();
	session->out_cipher = fst_cipher_create ();
	session->in_packet = fst_packet_create ();

	if (!session->in_cipher ||
		!session->out_cipher ||
		!session->in_packet)
	{
		fst_session_free (session);
		return NULL;
	}

	session->state = SessNew;
	session->was_established = FALSE;
	session->shared_files = 0;
	session->in_xinu = 0x51;
	session->out_xinu = 0x51;

	session->tcpcon = NULL;
	session->node = NULL;

	session->ping_timer = 0;
	session->callback = callback;

	return session;
}

/* Free session. Sets session->node->session to NULL. */
void fst_session_free (FSTSession *session)
{
	if (!session)
		return;

	fst_cipher_free (session->in_cipher);
	fst_cipher_free (session->out_cipher);

	fst_packet_free (session->in_packet);
	tcp_close (session->tcpcon);
	
	fst_peer_remove (FST_PLUGIN->peers, session->node, session->peers);
	
	if (session->node)
		session->node->session = NULL;
	fst_node_release (session->node);

	timer_remove (session->ping_timer);

	free (session);
}

/* Connect to node. Increments node's refcount and keeps a pointer to it.
 * Sets node->session to self.
 */
int fst_session_connect (FSTSession *session, FSTNode *node)
{
	in_addr_t ip;

	if (!session || session->state != SessNew || !node)
		return FALSE;

	assert (!node->session);

	session->state = SessConnecting;

	if ((ip = net_ip (node->host)) == INADDR_NONE)
	{
		struct hostent *he;

		/* TODO: make this non-blocking */
		if (! (he = gethostbyname (node->host)))
		{
			session->state = SessDisconnected;
			FST_WARN_1 ("gethostbyname failed for host %s", node->host);
			return FALSE;
		}

		/* hmm */
		ip = *((in_addr_t*)he->h_addr_list[0]);
	}

	FST_DBG_3 ("connecting to %s:%d, load: %d%%",
	           node->host, node->port, node->load);

	session->tcpcon = tcp_open (ip, node->port, FALSE);

	if (!session->tcpcon)
	{
		session->state = SessDisconnected;
		FST_WARN_1 ("tcp_open() failed for %s. no route to host?",
					node->host);
		return FALSE;
	}
	
	session->tcpcon->udata = (void*)session;
	session->node = node;
	fst_node_addref (session->node);

	session->node->session = session;

	input_add (session->tcpcon->fd, (void*) session, INPUT_WRITE,
			   (InputCallback) session_connected, FST_SESSION_CONNECT_TIMEOUT);

	return TRUE;
}

/* disconnect session */
int fst_session_disconnect (FSTSession *session)
{
	if (!session)
		return FALSE;

	tcp_close_null (&session->tcpcon);
	timer_remove_zero (&session->ping_timer);

	session->state = SessDisconnected;
	
	FST_DBG_2 ("disconnected from %s:%d",
			   session->node->host, session->node->port);

	/* raise  callback */
	session->callback (session, SessMsgDisconnected, NULL);

	return TRUE;
}

/* sends 0x4B packet over session */
int fst_session_send_message(FSTSession *session, FSTSessionMsg msg_type,
							 FSTPacket *msg_data)
{
	FSTPacket *new_packet;
    unsigned char hi_len, lo_len;
    unsigned char hi_type, lo_type;
    int xtype;

	if (!session || session->state != SessEstablished)
		return FALSE;

	assert (msg_type < 0xFF);
	assert (msg_data != NULL);

	if (! (new_packet = fst_packet_create()))
		return FALSE;

	FST_HEAVY_DBG_1 ("sending msg with msg_type: 0x%02X", msg_type);

    lo_len = fst_packet_size(msg_data) & 0xff;
    hi_len = fst_packet_size(msg_data) >> 8;

	fst_packet_put_uint8 (new_packet, 0x4B); /* 'K' */

	lo_type = msg_type & 0xFF;
	hi_type = msg_type >> 8;

    xtype = session->out_xinu % 3;

    switch(xtype) {
	case 0:
		fst_packet_put_uint8 (new_packet, lo_type);
		fst_packet_put_uint8 (new_packet, hi_type);
		fst_packet_put_uint8 (new_packet, hi_len);
		fst_packet_put_uint8 (new_packet, lo_len);
	    break;
	case 1:
		fst_packet_put_uint8 (new_packet, hi_type);
		fst_packet_put_uint8 (new_packet, hi_len);
		fst_packet_put_uint8 (new_packet, lo_type);
		fst_packet_put_uint8 (new_packet, lo_len);
	    break;
	case 2:
		fst_packet_put_uint8 (new_packet, hi_type);
		fst_packet_put_uint8 (new_packet, lo_len);
		fst_packet_put_uint8 (new_packet, hi_len);
		fst_packet_put_uint8 (new_packet, lo_type);
	    break;
    }

	/* update xinu state */
    session->out_xinu ^= ~(fst_packet_size(msg_data) + msg_type);

	fst_packet_append (new_packet, msg_data);
	fst_packet_encrypt (new_packet, session->out_cipher);

	if (!fst_packet_send (new_packet, session->tcpcon))
	{
		fst_packet_free (new_packet);
		return FALSE;
	}

	fst_packet_free (new_packet);
	return TRUE;
}

/* sends info about us to supernode */
int fst_session_send_info (FSTSession *session)
{
	FSTPacket *packet;
	in_addr_t ip;
	in_port_t port;

	if (! (packet = fst_packet_create ()))
		return FALSE;

	/* send outside ip if available, local ip otherwise. */
	if (FST_PLUGIN->external_ip && FST_PLUGIN->forwarding)
		ip = FST_PLUGIN->external_ip;
	else
		ip = FST_PLUGIN->local_ip;

	/* send port if server is running, zero otherwise */
	if (FST_PLUGIN->server)
		port = FST_PLUGIN->server->port;
	else
		port = 0;

	FST_HEAVY_DBG_4 ("sending address: %s:%d, bandwidth: 0x%02x, username: %s",
	           net_ip_str (ip), port, FST_ADVERTISED_BW, FST_USER_NAME);

	/* send ip and port */
	fst_packet_put_uint32 (packet, ip);
	fst_packet_put_uint16 (packet, htons (port));

	/* This next byte represents the user's advertised bandwidth, on
 	 * a logarithmic scale.  0xd1 represents "infinity" (actually,
	 * 1680 kbps).  The value is approximately 14*log_2(x)+59, where
	 * x is the bandwidth in kbps.
	 */
	fst_packet_put_uint8 (packet, FST_ADVERTISED_BW);

	/* 1 byte: dunno. */
	fst_packet_put_uint8 (packet, 0x00);

	/* user name, no trailing '\0' */
	fst_packet_put_ustr (packet, FST_USER_NAME, strlen (FST_USER_NAME));

	if (! fst_session_send_message (session, SessMsgNodeInfo, packet))
	{
		fst_packet_free (packet);
		return FALSE;
	}

	fst_packet_free (packet);
	return TRUE;
}

/*****************************************************************************/

static void session_connected(int fd, input_id input, FSTSession *session)
{
	FSTPacket *packet;
	fst_uint32 encoded_enc_type;

	input_remove (input);

	if (net_sock_error (session->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("couldn't connect to %s:%d",
						 session->node->host, session->node->port);
		fst_session_disconnect (session);
		return;
	}

	/* notify plugin */
	if (!session->callback (session, SessMsgConnected, NULL))
		return;

	/* go on with handshake */
	session->state = SessHandshaking;
	session->out_cipher->enc_type = 0x29; /* our preferred encryption type */
	session->out_cipher->seed = 0x0FACB1238; /* random number? */

	FST_HEAVY_DBG_1 ("requesting outgoing enc_type: 0x%02x",
	                 session->out_cipher->enc_type);

	if (! (packet = fst_packet_create ()))
	{
		fst_session_disconnect (session);
		return;
	}

	encoded_enc_type = fst_cipher_mangle_enc_type(session->out_cipher->seed,
												  session->out_cipher->enc_type);

	fst_packet_put_uint32 (packet, htonl(0xFA00B62B)); /* random number? */
	fst_packet_put_uint32 (packet, htonl(session->out_cipher->seed));
	fst_packet_put_uint32 (packet, htonl(encoded_enc_type));
 
	FST_HEAVY_DBG ("sending initial packet");

	if (!fst_packet_send (packet, session->tcpcon))
	{
		FST_DBG ("session_connected: fst_packet_send() failed");
		fst_packet_free (packet);
		fst_session_disconnect (session);
	}

	/* change the input callback to session_decrypt_packet */
	input_add (session->tcpcon->fd, (void*) session, INPUT_READ,
			   (InputCallback) session_decrypt_packet,
			   FST_SESSION_HANDSHAKE_TIMEOUT);

	fst_packet_free (packet);
}

static void session_decrypt_packet(int fd, input_id input, FSTSession *session)
{
	FSTPacket *packet;

	input_remove (input);

	if(net_sock_error (session->tcpcon->fd))
	{
		FST_HEAVY_DBG_2 ("socket error for %s:%d",
						 session->node->host, session->node->port);
		fst_session_disconnect (session);
		return;
	}

	if (! (packet = fst_packet_create ()))
	{
		fst_session_disconnect (session);
		return;
	}

	if (!fst_packet_recv (packet, session->tcpcon))
	{
		fst_packet_free (packet);
		fst_session_disconnect (session);
		return;
	}

	if (session->state == SessHandshaking)
	{
		fst_packet_append (session->in_packet, packet);
		fst_packet_free (packet);

		if (fst_packet_size (session->in_packet) < 8)
		{
			FST_DBG_1 ("received insufficient data for calculating key, got %d bytes, waiting for more...",
					   fst_packet_size(session->in_packet));
			/* get more data */
			input_add (session->tcpcon->fd, (void*) session,INPUT_READ,
					   (InputCallback) session_decrypt_packet,
					   FST_SESSION_HANDSHAKE_TIMEOUT);
			return;
		}

		FST_HEAVY_DBG ("handshaking...");

		if (!session_do_handshake (session))
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

	if (session->state == SessWaitingNetName)
	{
		char **net_name;
		fst_uint8 c = 'a';

		while (fst_packet_remaining (session->in_packet))
			if ((c = fst_packet_get_uint8 (session->in_packet)) == '\0')
				break;

		if (c != '\0')
		{
			fst_packet_rewind (session->in_packet);
			/* get more data */
			input_add (session->tcpcon->fd, (void*) session,INPUT_READ,
					  (InputCallback) session_decrypt_packet,
					  FST_SESSION_HANDSHAKE_TIMEOUT);
			return;
		}

		/* check for valid network name */
		for (net_name=valid_network_names; *net_name; net_name++)
			if (strcasecmp (*net_name, session->in_packet->data) == 0)
				break;

		if (*net_name == NULL)
		{
			FST_WARN_1 ("Remote network name invalid: \"%s\". closing connection",
						session->in_packet->data);
			fst_session_disconnect (session);
			return;
		}

		FST_DBG_1 ("remote network name is \"%s\"",
				   session->in_packet->data);

		session->state = SessEstablished;
		fst_packet_truncate (session->in_packet);

		if (!fst_session_send_info (session))
		{
			fst_session_disconnect (session);
			return;
		}

		/* set up ping timer */
		session->ping_timer = timer_add (FST_SESSION_PING_INTERVAL,
		                                 (TimerCallback) session_ping,
		                                 session);

		session->was_established = TRUE;

		/* notify plugin of established session */
		if (!session->callback (session, SessMsgEstablished, NULL))
			return;
	}

	/* decode session packets now */

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
			session_received_pong (session);
			continue;
		}

		/* we got a message */
		if (type == 0x4B)
		{
			int xtype = session->in_xinu % 3;
			unsigned int msg_type = 0, msg_len = 0;
			FSTPacket *packet;

			if(fst_packet_remaining (session->in_packet) < 5)
			{
				fst_packet_rewind (session->in_packet);
				FST_HEAVY_DBG ("didn't get the whole message header, waiting for more data");
				/* get more data */
				input_add (session->tcpcon->fd, (void*) session,INPUT_READ,
						   (InputCallback) session_decrypt_packet, 0);
				return;
			}
			
			switch (xtype) {
			case 0:
				msg_type = (unsigned int)fst_packet_get_uint8 (session->in_packet);
				msg_type |= ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_len = ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_len |= (unsigned int)fst_packet_get_uint8 (session->in_packet);
				break;
			case 1:
				msg_type = ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8; 
				msg_len = ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_type |= (unsigned int)fst_packet_get_uint8 (session->in_packet);
				msg_len |= (unsigned int)fst_packet_get_uint8 (session->in_packet);
				break;
			case 2:
				msg_type = ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_len = (unsigned int)fst_packet_get_uint8 (session->in_packet);
				msg_len |= ((unsigned int)fst_packet_get_uint8 (session->in_packet)) << 8;
				msg_type |= (unsigned int)fst_packet_get_uint8 (session->in_packet);
				break;
			}

			FST_HEAVY_DBG_2 ("got message type = %02x, length = %d byte",
							 msg_type, msg_len);

			if (fst_packet_remaining (session->in_packet) < msg_len)
			{
				fst_packet_rewind (session->in_packet);
				FST_HEAVY_DBG ("didn't get the whole message, waiting for more data");
				/* get more data */
				input_add (session->tcpcon->fd, (void*) session,INPUT_READ,
						   (InputCallback) session_decrypt_packet, 0);
				return;
			}
			
			/* modify xinu state */
			session->in_xinu ^= ~(msg_type + msg_len);

			/* prepare packet for callback */
			packet = fst_packet_create_copy (session->in_packet, msg_len);
			fst_packet_truncate (session->in_packet);
	
			/* raise callback */
			if (!session->callback (session, msg_type, packet))
			{
				/* session was probably already freed by the handler.
				 * so don't access it
				 */
				fst_packet_free (packet);
				return;
			}

			fst_packet_free (packet);
			continue;
		}

		/* oops, dunno what that packet is, maybe our decryption failed? */
		FST_WARN_1 ("strange packet of type 0x%02X received. closing connection.",
					type);
#ifdef HEAVY_DEBUG
		print_bin_data (session->in_packet->data,
						fst_packet_remaining (session->in_packet));
#endif

		fst_session_disconnect (session);
		return;
	}

	/* get more data */
	input_add (session->tcpcon->fd, (void*)session, INPUT_READ,
			   (InputCallback) session_decrypt_packet, 0);
}

/*****************************************************************************/

static int session_do_handshake (FSTSession *session)
{
	unsigned int seed, enc_type;
	FSTPacket *packet;
	
	if (fst_packet_size (session->in_packet) < 8)
		return FALSE;

	/* get seed and enc_type */
	seed = ntohl (fst_packet_get_uint32 (session->in_packet));
	enc_type = ntohl (fst_packet_get_uint32 (session->in_packet));
	enc_type = fst_cipher_mangle_enc_type (seed, enc_type);

	/* generate send key */
	session->out_cipher->seed ^= seed; /* xor send cipher with received seed */

	/* the correct behaviour here is to use the enc_type the supernode sent
	 * us for out_cipher too.
	 */
	session->out_cipher->enc_type = enc_type;

	if(!fst_cipher_init (session->out_cipher, session->out_cipher->seed,
						 session->out_cipher->enc_type))
	{
		FST_WARN_1 ("Unsupported encryption: 0x%02X",
					session->out_cipher->enc_type);
		return FALSE;
	}

	/* generate recv key */
	if(!fst_cipher_init (session->in_cipher, seed, enc_type))
	{
		FST_WARN_1 ("Unsupported encryption: 0x%02X", enc_type);
		return FALSE;
	}

	FST_HEAVY_DBG_2 ("outgoing enc_type: 0x%02X, incoming enc_type: 0x%02X",
			   session->out_cipher->enc_type, enc_type);

	/* send network name */
	FST_HEAVY_DBG_1 ("session_do_handshake: sending network name (\"%s\")",
					 FST_NETWORK_NAME);

	if (! (packet = fst_packet_create ()))
		return FALSE;

	fst_packet_put_ustr (packet, FST_NETWORK_NAME, strlen (FST_NETWORK_NAME) + 1);
	fst_packet_encrypt (packet, session->out_cipher);

	if (!fst_packet_send (packet, session->tcpcon))
	{
		fst_packet_free (packet);
		return FALSE;
	}

	fst_packet_free (packet);
	return TRUE;
}

/*****************************************************************************/

static void session_received_pong (FSTSession *session)
{
	FST_HEAVY_DBG ("got pong, resetting timer");

	/* remove timer, whichever it was */
	timer_remove (session->ping_timer);

	/* add ping timer */
	session->ping_timer = timer_add (FST_SESSION_PING_INTERVAL,
	                                 (TimerCallback) session_ping,
	                                 session);
}

static BOOL session_ping (FSTSession *session)
{
	/* send ping */
	if (!session_send_ping (session))
	{
		FST_WARN ("sending ping failed, disconnecting");
		fst_session_disconnect (session);

		return FALSE;
	}

	/* add timeout timer */
	session->ping_timer = timer_add (FST_SESSION_PING_TIMEOUT,
	                                 (TimerCallback) session_ping_timeout,
	                                 session);

	FST_HEAVY_DBG ("periodic session ping sent");

	/* remove ping timer */
	return FALSE;
}

static BOOL session_ping_timeout (FSTSession *session)
{
	FST_WARN ("ping timeout, disconnecting");

	/* didn't get pong, disconnect */
	fst_session_disconnect (session);

	/* remove ping timer */
	return FALSE;
}

/*****************************************************************************/

/* send out pong response */
static int session_send_pong (FSTSession *session)
{
	FSTPacket *packet;
	int ret;

	if (! (packet = fst_packet_create ()))
		return FALSE;

	fst_packet_put_uint8 (packet, 0x52);
	fst_packet_encrypt (packet, session->out_cipher);
	
	ret = fst_packet_send (packet, session->tcpcon);

	fst_packet_free (packet);

	return ret;
}

/* send out ping request */
static int session_send_ping (FSTSession *session)
{
	FSTPacket *packet;
	int ret;

	if (! (packet = fst_packet_create ()))
		return FALSE;

	fst_packet_put_uint8 (packet, 0x50);
	fst_packet_encrypt (packet, session->out_cipher);
	
	ret = fst_packet_send (packet, session->tcpcon);

	fst_packet_free (packet);

	return ret;
}
