/*
 * $Id: as_push_man.h,v 1.1 2004/09/26 19:49:37 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_PUSH_MAN_H_
#define __AS_PUSH_MAN_H_

/*****************************************************************************/

typedef struct
{
	/* Hashtable of pushes keyed by id. */
	ASHashTable *pushes;

	as_uint32 next_push_id; /* our source of push ids */

} ASPushMan;

/*****************************************************************************/

/* Allocate and init push manager. */
ASPushMan *as_pushman_create ();

/* Free manager. */
void as_pushman_free (ASPushMan *man);

/*****************************************************************************/

/* Send push request for hash to source. push_cb is called with the result. */
ASPush *as_pushman_send (ASPushMan *man, ASPushCb push_cb, ASSource *source,
                         ASHash *hash);

/* Remove and free push. */
void as_pushman_remove (ASPushMan *man, ASPush *push);

/*****************************************************************************/

/* Handle pushed concection. */
as_bool as_pushman_accept (ASPushMan *man, ASHash *hash, as_uint32 push_id,
                           TCPC *c);

/*****************************************************************************/

/* Get push by its id. */
ASPush *as_pushman_lookup (ASPushMan *man, as_uint32 push_id);

/*****************************************************************************/

#endif /* __AS_PUSH_MAN_H_ */
