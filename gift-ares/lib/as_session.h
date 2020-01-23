/*
 * $Id: as_session.h,v 1.11 2005/10/30 18:14:25 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SESSION_H_
#define __AS_SESSION_H_

/*****************************************************************************/

typedef enum
{
	SESSION_DISCONNECTED, /* tcp disconnected */
	SESSION_CONNECTING,   /* tcp connecting */
	SESSION_FAILED,       /* tcp connect failed */
	SESSION_HANDSHAKING,  /* tcp connected, working out crypto */
	SESSION_CONNECTED     /* session established */
} ASSessionState;

/*****************************************************************************/

typedef struct as_session_t ASSession;

/* Called for state changes. Return FALSE if the session has been freed and
 * can no longer be used after the callback.
 */
typedef as_bool (*ASSessionStateCb) (ASSession *session,
                                     ASSessionState new_state);

/* Called for all non-handshake packets. Return FALSE if the session has been
 * freed and can no longer be used after the callback.
 */
typedef as_bool (*ASSessionPacketCb) (ASSession *session, ASPacketType type,
                                      ASPacket *packet);

struct as_session_t
{
	in_addr_t      host;
	in_port_t      port;
	TCPC          *c;
	input_id       input;   /* input id of event associated with c */

	ASCipher      *cipher;
	ASPacket      *packet;  /* buffer for incoming data */

	ASSessionState state;

	ASSessionStateCb  state_cb;
	ASSessionPacketCb packet_cb;

	as_uint16      search_id;

	timer_id       ping_timer;
	timer_id       pong_timer;
	timer_id       handshake_timer;

	void *udata; /* user data */
};

/*****************************************************************************/

/* Create new session with specified callbacks. */
ASSession *as_session_create (ASSessionStateCb state_cb,
                              ASSessionPacketCb packet_cb);

/* Disconnect but does not free session. Triggers state callback. */
void as_session_free (ASSession *session);

/*****************************************************************************/

/* Returns current state of session */
ASSessionState as_session_state (ASSession *session);

/* Connect to ip and port. Fails if already connected. */
as_bool as_session_connect (ASSession *session, in_addr_t host,
                            in_port_t port);

/* Disconnect but does not free session. Triggers state callback if
 * specified.
 */
void as_session_disconnect (ASSession *session, as_bool raise_callback);

/*****************************************************************************/

/* Send packet to supernode. flag specifies if the packet should be encrypted
 * or compressed. The body packet will be modified.
 */
as_bool as_session_send (ASSession *session, ASPacketType type,
                         ASPacket *body, ASPacketFlag flag);

/*****************************************************************************/

#endif /* __AS_SESSION_H_ */
