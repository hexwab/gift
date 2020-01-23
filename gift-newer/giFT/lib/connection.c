/*
 * connection.c
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

#include "gift.h"

#include "network.h"
#include "queue.h"
#include "event.h"

#include "connection.h"

/*****************************************************************************/

/* opens a new TCP connection to a remote host (p is optional, use only if
 * you are implementing a giFT plugin)
 * NOTE: connection_open will return NULL if connect failed and blocking
 * is set to TRUE */
Connection *connection_open (Protocol *p, char *host, unsigned short port,
							 int blocking)
{
	Connection *c;
	int         fd;

	if ((fd = net_connect (host, port, blocking)) < 0)
		return NULL;

	/* create the structure */
	if (!(c = connection_new (p)))
		return NULL;

	c->fd = fd;

	return c;
}

/* close the established connection
 * NOTE: this function flushes the queue and removes all inputs */
void connection_close (Connection *c)
{
	/* flush pending writes */
	queue_flush (c);

	/* remove all event inputs */
	input_remove (c);

	/* close the socket */
	net_close (c->fd);

	/* destroy the data */
	connection_free (c);
}

/*****************************************************************************/

Connection *connection_new (Protocol *p)
{
	Connection *c;

	if (!(c = malloc (sizeof (Connection))))
		return NULL;

	memset (c, 0, sizeof (Connection));

	/* merely for tracking/debugging...we don't _really_ need to know this */
	c->protocol = p;

	return c;
}

void connection_free (Connection *c)
{
	if (!c)
		return;

	free (c);
}
