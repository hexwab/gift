/*
 * connection.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

/*****************************************************************************/

struct _protocol;

typedef struct _connection
{
	int               fd;        /* the file discriptor of the connection */
	int               ev_id;     /* event associated w/ this connection */
	int               closing;   /* state when this socket is currently
	                              * being cleaned up (used to avoid a race
	                              * condition) */

	List             *write_queue;

	struct _protocol *protocol;  /* protocol this connection belongs to */
	void             *data;      /* the data that the connection needs (e.g.
	                              * cipher states, state of connection, etc.
	                              */
} Connection;

/*****************************************************************************/

Connection   *connection_new        (struct _protocol *p);
void          connection_destroy    (Connection *c);
void          connection_free       (Connection *c);

/*****************************************************************************/

#endif /* __CONNECTION_H__ */
