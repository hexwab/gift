/*
 * $Id: as_push_reply.h,v 1.2 2004/12/19 18:54:59 mkern Exp $
 *
 * Copyright (C) 2004 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

#ifndef __AS_PUSH_REPLY_H
#define __AS_PUSH_REPLY_H

/*****************************************************************************/

/* Note: In order to simplify things a bit we do not separate the push reply
 *       object from its manager. It is only used internally and not exposed.
 */

/*****************************************************************************/

typedef struct
{
	/* List of push replies so we can clean them up when manager is freed. */
	List *replies;

} ASPushReplyMan;

/*****************************************************************************/

/* Allocate and init push reply manager. */
ASPushReplyMan *as_pushreplyman_create ();

/* Free manager. */
void as_pushreplyman_free (ASPushReplyMan *man);

/*****************************************************************************/

/* Handle push reply request by creating internal reply object and starting
 * connection to host. If the connections succeeds it will be handed over to
 * the http server.
 */
void as_pushreplyman_handle (ASPushReplyMan *man, ASPacket *packet);

/*****************************************************************************/

#endif /* __AS_PUSH_REPLY_H */

