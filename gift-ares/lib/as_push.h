/*
 * $Id: as_push.h,v 1.2 2004/09/26 19:49:37 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_PUSH_H
#define __AS_PUSH_H

/*****************************************************************************/

#define INVALID_PUSH_ID 0

typedef enum
{
	PUSH_NEW,
	PUSH_CONNECTING, /* Connecting to source's supernode */
	PUSH_SENT,       /* Sent push and waiting */
	PUSH_OK,         /* Got pushed connection */
	PUSH_FAILED      /* Send failure or timeout */
} ASPushState;

typedef struct as_push_t ASPush;

/* Called when we get the pushed connection or if the push failed in which
 * case c is NULL. The callback is responsible for freeing the push and
 * closing the pushed connection if there is one.
 */
typedef void (*ASPushCb) (ASPush *push, TCPC *c);

struct as_push_t
{
	ASSource  *source;    /* Source we want a push from */
	ASHash    *hash;      /* Hash we want pushed */
	as_uint32  id;        /* Our chosen push id */

	TCPC      *sconn;     /* New connection to source's supernode */
	timer_id   timer;     /* Timer for push timeout */

	ASPushState state;    /* Current state */
	ASPushCb    callback;

	void *udata; /* arbitrary user data */
};

/*****************************************************************************/

/* Create push. */
ASPush *as_push_create (as_uint32 id, ASSource *source, ASHash *hash,
                        ASPushCb callback);

/* Close supernode connection and free push. */
void as_push_free (ASPush *push);

/*****************************************************************************/

/* Send push. */
as_bool as_push_send (ASPush *push);

/* Handle pushed connection, i.e. raise callback. */
as_bool as_push_accept (ASPush *push, ASHash *hash, TCPC *c);

/*****************************************************************************/

#endif /* __AS_PUSH_H */
