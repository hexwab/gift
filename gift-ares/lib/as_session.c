/*
 * $Id: as_session.c,v 1.51 2006/01/05 16:37:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

static void session_connected (int fd, input_id input, ASSession *session);
static void session_get_packet (int fd, input_id input, ASSession *session);

static as_bool session_dispatch_packet (ASSession *session, ASPacketType type,
                                        ASPacket *packet);

static as_bool session_handshake (ASSession *session,  ASPacketType type,
                                  ASPacket *packet);

static as_bool session_set_state (ASSession *session, ASSessionState state,
                                  as_bool raise_callback);

static as_bool session_error (ASSession *session);

/* timer callbacks */
static as_bool session_ping (ASSession *session);
static as_bool session_ping_timeout (ASSession *session);
static as_bool session_handshake_timeout (ASSession *session);

/*****************************************************************************/

/* Create new session with specified callbacks. */
ASSession *as_session_create (ASSessionStateCb state_cb,
                              ASSessionPacketCb packet_cb)
{
	ASSession *session;

	if (!(session = malloc (sizeof (ASSession))))
		return NULL;

	session->host       = INADDR_NONE;
	session->port       = 0;
	session->c          = NULL;
	session->input      = INVALID_INPUT;
	session->cipher     = NULL;
	session->packet     = NULL;
	session->state      = SESSION_DISCONNECTED;
	session->state_cb   = state_cb;
	session->packet_cb  = packet_cb;
	session->udata      = NULL;
	session->search_id  = 0;
	session->ping_timer      = INVALID_TIMER;
	session->pong_timer      = INVALID_TIMER;
	session->handshake_timer = INVALID_TIMER;

	return session;
}

static void session_cleanup (ASSession *session)
{
	input_remove (session->input);
	timer_remove_zero (&session->ping_timer);
	timer_remove_zero (&session->pong_timer);
	timer_remove_zero (&session->handshake_timer);

	tcp_close (session->c);
	as_cipher_free (session->cipher);
	as_packet_free (session->packet);

	session->input  = INVALID_INPUT;
	session->c      = NULL;
	session->cipher = NULL;
	session->packet = NULL;
}

/* Disconnect and free session. */
void as_session_free (ASSession *session)
{
	if (!session)
		return;

	session_cleanup (session);

	free (session);
}

/*****************************************************************************/

/* Returns current state of session */
ASSessionState as_session_state (ASSession *session)
{
	return session->state;
}

/* Connect to ip and port. Fails if already connected. */
as_bool as_session_connect (ASSession *session, in_addr_t host,
                            in_port_t port)
{
	assert (session);
	assert (session->c == NULL);

	session->host = host;
	session->port = port;

	if (!(session->c = tcp_open (session->host, session->port, FALSE)))
	{
		AS_ERR_2 ("tcp_open failed for %s:%d", net_ip_str (host), port);
		return FALSE;
	}

	/* wait for connect result */
	session->input = input_add (session->c->fd, session, INPUT_WRITE,
	                            (InputCallback)session_connected, 
	                            AS_SESSION_CONNECT_TIMEOUT);

	if (session->input == INVALID_INPUT)
	{
		tcp_close (session->c);
		session->c = NULL;
		return FALSE;
	}

	session_set_state (session, SESSION_CONNECTING, TRUE);

	return TRUE;
}

/* Disconnect but does not free session. Triggers state callback if
 * specified.
 */
void as_session_disconnect (ASSession *session, as_bool raise_callback)
{
	assert (session);

	session_cleanup (session);

	session_set_state (session, SESSION_DISCONNECTED, raise_callback);
}

/*****************************************************************************/

/* Send packet to supernode. flag specifies if the packet should be encrypted
 * or compressed. The body packet will be modified.
 */
as_bool as_session_send (ASSession *session, ASPacketType type,
                         ASPacket *body, ASPacketFlag flag)
{
	/* encrypt or compress packet body */
	switch (flag)
	{
	case PACKET_PLAIN:
		break;

	case PACKET_ENCRYPT:
#ifdef AS_COMPRESS_ALL
		if (type != PACKET_HANDSHAKE && body->used > 10)
		{
			as_packet_header (body, type);
			type = PACKET_COMPRESSED;
			/* fall-through */
		}
		else
#endif
		{
			/* If cipher is NULL this is an unencrypted connection. */
			if (session->cipher)
			{
				if (!as_cipher_encrypt_packet (session->cipher, body))
				{
					AS_ERR ("Encrypt failed");
					return FALSE;
				}
			}
			break;
		}
	case PACKET_COMPRESS:
		if (!as_packet_compress (body))
		{
			AS_ERR ("Compression failed");
			return FALSE;
		}
		break;
	}

	/* added packet header  */
	if (!as_packet_header (body, type))
	{
		AS_ERR ("Insufficient memory");
		return FALSE;
	}

	/* send it off */
	if (!as_packet_send (body, session->c))
	{
		AS_ERR ("Send failed");
		return FALSE;
	}

	timer_reset (session->ping_timer);

	return TRUE;
}

/*****************************************************************************/

static void session_connected (int fd, input_id input, ASSession *session)
{
	ASPacket *packet;

	input_remove (input);
	session->input = INVALID_INPUT;

	if (net_sock_error (fd))
	{
#if 0
		AS_HEAVY_DBG_2 ("Connect to %s:%d failed",
		                net_ip_str (session->host), session->port);
#endif
		session_error (session);
		return;
	}

	AS_DBG_2 ("Connected to %s:%d", net_ip_str (session->host),
	          session->port);
	
	/* set up packet buffer */
	if (!(session->packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		session_error (session);
		return;
	}

	/* send syn packet */
	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		session_error (session);
		return;
	}

	/* Packet body. The first two bytes were originally random. Setting them
	 * to 0x06 will make the supernode use an encryptionless session if
	 * supported. The last byte is a marker for the current protocol as
	 * opposed to some old one which has never been supported by this library.
	 */
	as_packet_put_8 (packet, 0x06);
	as_packet_put_8 (packet, 0x06);
	as_packet_put_8 (packet, 0x05);

	if (!as_session_send (session, PACKET_SYN, packet, PACKET_PLAIN))
	{
		AS_ERR ("Send failed");
		as_packet_free (packet);
		session_error (session);
		return;
	}

	as_packet_free (packet);

	if (!session_set_state (session, SESSION_HANDSHAKING, TRUE))
		return; /* session was freed */

	/* wait for supernode handshake packet */
	session->input = input_add (session->c->fd, session, INPUT_READ, 
	                            (InputCallback)session_get_packet, 0);

	/* Add explicit handshake timeout since the input timeout is removed once
	 * the first data has arrived. Since that may not be the entire handshake
	 * packet we can be made to wait indefinitely by the supernode without an
	 * separate timeout.
	 */
	session->handshake_timer = timer_add (AS_SESSION_HANDSHAKE_TIMEOUT,
	                               (TimerCallback)session_handshake_timeout,
	                               session);

	return;
}

static void session_get_packet (int fd, input_id input, ASSession *session)
{
	ASPacket *packet;
	ASPacketType packet_type;
	as_uint16 packet_len;

	if (net_sock_error (fd))
	{
		AS_HEAVY_DBG_2 ("Connection with %s:%d closed remotely",
		                net_ip_str (session->host), session->port);
		session_error (session);
		return;
	}

	if (!as_packet_recv (session->packet, session->c))	
	{
		AS_WARN_2 ("Recv failed from %s:%d", net_ip_str (session->host),
		           session->port);
		session_error (session);
		return;
	}

	/* dispatch all complete packets we have */
	while (as_packet_remaining (session->packet) >= 3)
	{
		packet_len = as_packet_get_le16 (session->packet);
		packet_type = as_packet_get_8 (session->packet);

		if (as_packet_remaining (session->packet) < packet_len)
		{
			/* rewind length and type and wait for more */
			as_packet_rewind (session->packet);
			return;
		}

		/* make copy of packet body */
		if (!(packet = as_packet_create_copy (session->packet, packet_len)))
		{
			AS_ERR ("Insufficient memory");
			session_error (session);
			return;
		}

		/* remove packet from buffer */
		as_packet_truncate (session->packet);

		/* dispatch packet */
		if (!session_dispatch_packet (session, packet_type, packet))
		{
			/* the connection has been closed/removed. Do nothing further */
			as_packet_free (packet);
			return;
		}

		/* free packet now that it has been handled */
		as_packet_free (packet);
	}

	timer_remove_zero (&session->pong_timer);

	return; /* wait for more */
}

/*****************************************************************************/

static as_bool session_dispatch_packet (ASSession *session, ASPacketType type,
                                        ASPacket *packet)
{
	if (as_session_state (session) == SESSION_HANDSHAKING)
	{
		if (type != PACKET_ACK && type != PACKET_ACK2 &&
		    type != PACKET_ACK_BUSY && type != PACKET_ACK_NOCRYPT)
		{
			AS_ERR_3 ("Handshake with %s:%d failed. Got 0x%02x instead of known ACK.",
			          net_ip_str (session->host), session->port, (int)type);
			session_error (session);
			return FALSE;
		}

		return session_handshake (session, type, packet);
	}
	else
	{
#if 0
		AS_HEAVY_DBG_2 ("Received packet type 0x%02x, length %d", (int)type,
		                as_packet_remaining (packet));
#endif

		if (type == PACKET_SHARE)
		{
			/* compressed packet */
			assert (0);
		}
		else
		{
			/* Decrypted packet if this is an encrypted session. */
			if (session->cipher)
			{
				if (!as_cipher_decrypt_packet (session->cipher, packet))
				{
					AS_ERR_3 ("Packet decrypt failed for type 0x%02X from %s:%d",
					          (int)type, net_ip_str (session->host),
					          session->port);
					session_error (session);
					return FALSE;
				}
			}
		}

		/* raise callback for this packet */
		if (session->packet_cb)
			return session->packet_cb (session, type, packet);
		else
			return TRUE;
	}
}
  
/*****************************************************************************/

static as_bool session_send_handshake (ASSession *session, ASPacketType type,
                                       as_uint8 supernode_guid[16])
{
	ASPacket *packet;
	as_uint8 *nonce = NULL;

	/* Create our part of the handshake */
	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory");
		return FALSE;
	}

	/* There are two different nonces used by the supernode to determine
	 * whether we are a legitimate client. The first nonce is used by Ares
	 * versions prior to 2962 and sent in response to PACKET_ACK (0x33). The
	 * second is used in Ares 2962 and later and sent in response to
	 * PACKET_ACK2 (0x38). Calculation of the later nonce required almost 1 MB
	 * of code in Ares but was removed when Ares went open source. It is now
	 * sufficient to simply randomize the nonce.
	 */
	if (type == PACKET_ACK)
	{
		/* hardcoded zero byte (only if original nonce is used) */
		as_packet_put_8 (packet, 0x00);

		/* 22 byte nonce created from supernode guid */
		assert (session->cipher);
		nonce = as_cipher_nonce (session->cipher, supernode_guid);
		if (!nonce)
		{
			AS_ERR ("Handshake nonce creation failed");
			as_packet_free (packet);
			return FALSE;
		}
	
		as_packet_put_ustr (packet, nonce, 22);
	}
	else if (type == PACKET_ACK2 || type == PACKET_ACK_NOCRYPT)
	{
		/* 20 byte nonce2 created from supernode guid */
		if (!(nonce = as_cipher_nonce2 (supernode_guid)))
		{
			AS_ERR ("Handshake nonce2 creation failed");
			as_packet_free (packet);
			return FALSE;
		}

		as_packet_put_ustr (packet, nonce, 20);
	}

	free (nonce);

	if (AS->upman)
	{
		/* FIXME: These values are not accurate if queuing is done by giFT */
		as_packet_put_le16 (packet, 0); /* 'my_speed' */
		as_packet_put_8 (packet, (as_uint8)AS->upman->nuploads);
		as_packet_put_8 (packet, (as_uint8)AS_CONF_INT (AS_UPLOAD_MAX_ACTIVE));
		as_packet_put_8 (packet, (as_uint8)0); /* 'proxy_count' */
		as_packet_put_8 (packet, (as_uint8)AS->upman->nqueued);
	}
	else
	{	
		as_packet_put_le16 (packet, 0);
		as_packet_put_le32 (packet, 0);
	}

	/* our listening port. */
	as_packet_put_le16 (packet, AS->netinfo->port);

	/* user name, zero terminated */
	as_packet_put_ustr (packet, AS_CONF_STR (AS_USER_NAME),
	                    strlen (AS_CONF_STR (AS_USER_NAME)) + 1);

	/* client GUID, 16 bytes */
	as_packet_put_ustr (packet, AS->netinfo->guid, 16);

	/* hardcoded zero byte */
	as_packet_put_8 (packet, 0x00);
	/* hardcoded zero byte */
	as_packet_put_8 (packet, 0x00);

	/* client name, zero terminated */
	as_packet_put_ustr (packet, AS_CLIENT_NAME, sizeof (AS_CLIENT_NAME));
	
	/* local ip */
	as_packet_put_ip (packet, net_local_ip (session->c->fd, NULL));

#ifdef AS_LOGIN_STRING
	/* Append encrypted login string added in Ares 2951. Removed again in
	 * Ares 2955. See as_ares.h. If the session is unencrypted we don't have a
	 * cipher and cannot calculate the login string.
	 */
	if (session->cipher)
	{
		as_uint8 *login_str, *login_hex;

		login_str = strdup (AS_LOGIN_STRING);
		assert (login_str);

		/* Encrypt login string. */
		as_encrypt_login_string (login_str, strlen (login_str),
		                         session->cipher->session_seed_16,
		                         session->cipher->session_seed_8);

		/* Hex encode login string. */
		if ((login_hex = as_hex_encode (login_str, strlen (AS_LOGIN_STRING))))
		{
			/* Append it to packet with zero termiantor */
			as_packet_put_strnul (packet, login_hex);
			free (login_hex);
		}

		free (login_str);
	}
#endif

	/* Send packet. */
	if (!as_session_send (session, PACKET_HANDSHAKE, packet,
	                      PACKET_ENCRYPT))
	{
		AS_ERR ("Send failed");
		as_packet_free (packet);
		return FALSE;
	}

	as_packet_free (packet);

	return TRUE;
}

static as_bool session_handshake (ASSession *session, ASPacketType type,
                                  ASPacket *packet)
{
	as_uint16 children;
	as_uint16 seed_16;
	as_uint8 seed_8;
	as_uint8 *supernode_guid;

	/* PACKET_ACK and PACKET_ACK2 only differ in how the nonce is calculated.
	 * The later once required elaborate calculations but this was removed in
	 * open source Ares. New Ares supernodes accept any nonce.
	 * PACKET_ACK_NOCRYPT is the same as PACKET_ACK2 but the connection is no
	 * longer encrypted.
	 */
	assert (type == PACKET_ACK || type == PACKET_ACK2 ||
	        type == PACKET_ACK_BUSY || type == PACKET_ACK_NOCRYPT);

	/* If supernode is full drop connection */
	if (type == PACKET_ACK_BUSY)
	{
		AS_DBG_2 ("Handshake with %s:%d aborted. Supernode is busy.",
		          net_ip_str (session->host), session->port);
		session_error (session);
		return FALSE;
	}

	if (as_packet_remaining (packet) < 0x15)
	{
		AS_ERR_2 ("Handshake with %s:%d failed. ACK packet too short.",
		          net_ip_str (session->host), session->port);
		session_error (session);
		return FALSE;
	}

	/* Create cipher with port as handshake key. */
	if (!(session->cipher = as_cipher_create (session->c->port)))
	{
		AS_ERR ("Insufficient memory");
		session_error (session);
		return FALSE;
	}

	/* decrypt handshake packet */
	as_cipher_decrypt_handshake (session->cipher, packet->read_ptr,
	                             as_packet_remaining (packet));

	/* If this is a encryptionless session we no longer need the cipher */
	if (type == PACKET_ACK_NOCRYPT)
	{
		as_cipher_free (session->cipher);
		session->cipher = NULL;
	}

#if 0
	as_packet_dump (packet);
#endif

	/* This is the child count of the supernode. */
	children = as_packet_get_le16 (packet);

	/* Get supernode GUID used in our reply below. */
	supernode_guid = as_packet_get_ustr (packet, 16);

	/* get cipher seeds */
	seed_16 = as_packet_get_le16 (packet);
	seed_8 = as_packet_get_8 (packet);

	/* Add supplied nodes to our cache. */
	while (as_packet_remaining (packet) >= 6)
	{
		in_addr_t host = as_packet_get_ip (packet);
		in_port_t port = (in_port_t) as_packet_get_le16 (packet);

		/* FIXME: The session manager should really do this. Accessing the
		 * node manager from here is ugly.
		 */
		as_nodeman_update_reported (AS->nodeman, host, port);
	}

#if 0
	/* Don't connect to older nodes for testing (but still use them to
	 * get new IPs).
	 */
	if (type == PACKET_ACK)
	{
		session_error (session);
		free (supernode_guid);
		return FALSE;
	}
#endif

#if 0	
	if (children > 350)
	{
		/* Ares disconnects if there are more than 350 children. Do the
		 * same.
		 */
		AS_DBG_3 ("Handshake with %s:%d aborted. Supernode has %d (>350) children.",
		          net_ip_str (session->host), session->port, (int)children);
		session_error (session);
		free (supernode_guid);
		return FALSE;
	}
#endif	

	/* Set up cipher. */
	if (session->cipher)
		as_cipher_set_seeds (session->cipher, seed_16, seed_8);

	/* Send our part of the handshake. */
	if (!session_send_handshake (session, type, supernode_guid))
	{
		AS_ERR_2 ("Handshake send failed to %s:%d",
		          net_ip_str (session->host), session->port);
		session_error (session);
		free (supernode_guid);
		return FALSE;
	}

	free (supernode_guid);

	/* Handshake is complete now. */
	AS_DBG_5 ("Handshake with %s:%d complete. ACK: 0x%02X, seeds: 0x%04X and 0x%02X",
		      net_ip_str (session->host), session->port, (int)type,
			  (int)seed_16, (int)seed_8);


	/* remove handshake timeout */
	timer_remove_zero (&session->handshake_timer);

	if (!session_set_state (session, SESSION_CONNECTED, TRUE))
		return FALSE; /* session was freed */

	session->ping_timer = timer_add (AS_SESSION_IDLE_TIMEOUT,
	                                 (TimerCallback)session_ping, session);

	return TRUE;
}

/*****************************************************************************/

/* no activity for a while, send a ping */
static as_bool session_ping (ASSession *session)
{
	ASPacket *p = as_packet_create ();

	/* send transfer stats */
	if (AS->upman)
	{
		/* FIXME: These values are not accurate if queuing is done by giFT */
		as_packet_put_8 (p, (as_uint8)AS->upman->nuploads);
		as_packet_put_8 (p, (as_uint8)AS_CONF_INT (AS_UPLOAD_MAX_ACTIVE));
		as_packet_put_8 (p, (as_uint8)0); /* unknown */
		as_packet_put_8 (p, (as_uint8)AS->upman->nqueued);
		as_packet_put_le16 (p, 0); /* unknown */
	}
	else
	{	
		as_packet_put_le32 (p, 0);
		as_packet_put_le16 (p, 0);
	}
	
	AS_DBG_2 ("Sent ping to %s:%d",
	          net_ip_str (session->host), session->port);

	as_session_send (session, PACKET_STATS2, p, PACKET_ENCRYPT);

	as_packet_free (p);

	assert (session->pong_timer == INVALID_TIMER);
	session->pong_timer = timer_add (AS_SESSION_PING_TIMEOUT,
	                                 (TimerCallback)session_ping_timeout,
	                                 session);

	return TRUE;
}

/* we sent a ping and got no response: disconnect */
static as_bool session_ping_timeout (ASSession *session)
{
	AS_ERR_2 ("Ping timeout for %s:%d",
	          net_ip_str (session->host), session->port);

	session_error (session); /* callback may free us */

	return FALSE;
}

/* we connected but didn't get a full handshake: disconnect */
static as_bool session_handshake_timeout (ASSession *session)
{
	AS_DBG_2 ("Handshake timeout for %s:%d",
	          net_ip_str (session->host), session->port);

	session_error (session); /* callback may free us */

	return FALSE;
}

/*****************************************************************************/

static as_bool session_set_state (ASSession *session, ASSessionState state,
                                  as_bool raise_callback)
{
	session->state = state;

	if (raise_callback && session->state_cb)
		return session->state_cb (session, session->state);

	return TRUE;
}

static as_bool session_error (ASSession *session)
{
	as_bool ret;

	session_cleanup (session);

	if (session->state == SESSION_HANDSHAKING ||
	    session->state == SESSION_CONNECTING)
		ret = session_set_state (session, SESSION_FAILED, TRUE);
	else
		ret = session_set_state (session, SESSION_DISCONNECTED, TRUE);

	return ret;
}

/*****************************************************************************/
