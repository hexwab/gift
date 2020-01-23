/*
 * $Id: as_session_man.h,v 1.6 2005/11/26 01:42:36 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_SESSION_MAN_H_
#define __AS_SESSION_MAN_H_

/*****************************************************************************/

/* Session iterator. */
typedef as_bool (*ASSessionForeachFunc) (ASSession *session, void *udata);

typedef struct
{
	unsigned int connections; /* number of conections to keep established */

	List *connecting;         /* connecting sessions */
	List *connected;          /* established sessions */

	timer_id maintenance_timer; /* Timer which makes sure we keep number of
	                             * sessions at requested level */

} ASSessMan;

/*****************************************************************************/

/* allocate and init session manager */
ASSessMan *as_sessman_create ();

/* free manager */
void as_sessman_free (ASSessMan *man);

/*****************************************************************************/

/* Returns number of actually established sessions */
unsigned int as_sessman_established (ASSessMan *man);

/* Returns ip and port of any one of the supernodes we are connected to */
in_addr_t as_sessman_get_supernode (ASSessMan *man, in_port_t *port);

/*****************************************************************************/

/* Set number of sessions that should be maintained at all times. Setting this
 * to zero will disconnect from the network. Anything non-zero will start
 * connecting.
 */
void as_sessman_connect (ASSessMan *man, unsigned int connections);

/*****************************************************************************/

/* Calls func for each established session. Do not remove or add sessions
 * during iteration (e.g. don't call as_sessman_connect). Returns number of
 * times func returned TRUE.
 */
int as_sessman_foreach (ASSessMan *man, ASSessionForeachFunc func,
                        void *udata);

/*****************************************************************************/

#endif /* __AS_SESSION_MAN_H_ */
