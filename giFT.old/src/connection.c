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

/*****************************************************************************/

Connection *connection_new (Protocol *p)
{
	Connection *c;

	if (!(c = malloc (sizeof (Connection))))
		GIFT_FATAL (("Couldn't create a new connection, out of memory\n"));

	memset (c, 0, sizeof (Connection));

	/* merely for tracking/debugging...we don't _really_ need to know this */
	c->protocol = p;

	return c;
}

void connection_destroy (Connection *c)
{
	if (!c)
		return;

	if_event_close (c);

	free (c);
}

void connection_free (Connection *c)
{
	free (c);
}
