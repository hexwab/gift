/*
 * watch.c - handles all attached client connections
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

#include "event.h"
#include "upload.h"

#include "watch.h"

/*****************************************************************************/

/* TODO - this code is seriously flawed...adding or removing attached
 * connections arent propagated to the if_event system! */

static List *attached = NULL;

/*****************************************************************************/

void watch_add (Connection *c)
{
	attached = list_append (attached, c);

	c->data = "attached";
}

void watch_remove (Connection *c)
{
	attached = list_remove (attached, c);
}

List *watch_copy ()
{
	return list_copy (attached);
}

#if 0
void watch_activate (Connection *c)
{
	DaemonWatch *dw;

	dw = malloc (sizeof (DaemonWatch));
	dw->timer = timer_add (1, (TimerCallback) watch_dump, c);

	interface_event_new (c, (InterfaceCloseFunc) watch_close, dw);

	c->data = dw;
}

void watch_close (Connection *c, DaemonWatch *dw)
{
	TRACE_FUNC ();

	timer_remove (dw->timer);
	free (dw);
}

int watch_dump (Connection *c)
{
	List *ptr;

	assert (c != NULL);

	for (ptr = upload_list (); ptr; ptr = list_next (ptr))
	{
		Transfer *transfer = ptr->data;
		Chunk *chunk0;

		/* validate that this upload is at least active */
		if (!transfer->chunks)
			continue;

		if (!(chunk0 = transfer->chunks->data))
			continue;

		interface_send (c, "TRANSFER",
						"TYPE=s",   "UPLOAD",
						"SOURCE=s", chunk0->source,
						"NAME=s",   transfer->filename,
						"SENT=i",   transfer->transmit,
						"TOTAL=i",  transfer->total, NULL);
	}

	interface_send (c, "TRANSFER", NULL);

	return TRUE;
}
#endif
