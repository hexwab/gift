/*
 * if_event.c
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

#include "if_event.h"

/*****************************************************************************/

static IFEventID  id_counter = 0;
static HashTable *events     = NULL;

/* list of all currently <attach/>'d connections */
static List      *attached   = NULL;

/*****************************************************************************/

/* TODO - opt */
static IFEventID uniq_id ()
{
	id_counter++;

	while (!id_counter || hash_table_lookup (events, id_counter))
		id_counter++;

	return id_counter;
}

static char *event_action (IFEvent *event)
{
	char *type = NULL;

	/* this method is temporary ;) */
	switch (event->type)
	{
	 case IFEVENT_OPT:
		type = "opt";
		break;
	 case IFEVENT_SEARCH:
		type = "search";
		break;
	 case IFEVENT_TRANSFER:
		type = "transfer";
		break;
	 case IFEVENT_STATS:
		type = "stats";
		break;
	 default:
		TRACE (("warning: unknown type %i", event->type));
		break;
	}

	return type;
}

/*****************************************************************************/

IFEventID if_event_new (Connection *c, IFEventType type, IFEventFunc func,
                        void *udata)
{
	IFEventID  id;
	IFEvent   *event;

	id = uniq_id ();

	event = malloc (sizeof (IFEvent));
	memset (event, 0, sizeof (IFEvent));

	event->id   = id;
	event->c    = c;
	event->type = type;
	event->func = func;
	event->data = udata;

	/* only one persisting type :) */
	if (type == IFEVENT_TRANSFER)
		event->persist = TRUE;

	if (!events)
		events = hash_table_new ();

	hash_table_insert (events, id, event);

	/* attached, send the id */
	if (c && c->data)
	{
		interface_send (c, "event",
		                "action=s", event_action (event),
		                "id=i",     event->id,
		                NULL);
	}

	return id;
}

static void if_event_free (IFEvent *event)
{
	free (event);
}

/*****************************************************************************/

void if_event_remove (IFEventID id)
{
	IFEvent *event;
	Connection *c;

	if (!(event = hash_table_lookup (events, id)))
		return;

	/* notify the usage that this event was just cancelled */
	if (event->func)
		(*event->func) (event);

	/* it is simply cleaner to call interface_close after all this memory is
	 * taken care of */
	c = event->c;

	if_event_free (event);
	hash_table_remove (events, id);

	/* if the connection that initiated this event is not attached, close the
	 * socket */
	if (c && !c->data)
		interface_close (c);
}

/*****************************************************************************/

static int close_conn (unsigned long key, IFEvent *event, Connection *c)
{
	if (event->c == c)
	{
		/* TODO - we must assume all events persist for this logic to
		 * work */
		event->c = NULL;
	}

	return TRUE;
}

/* called when a connection is closed so that we may properly cleanup */
void if_event_close (Connection *c)
{
	assert (c != NULL);

	/* if this is an attached connection, remove it */
	attached = list_remove (attached, c);

	/* some event somewhere is possibly hooked to this connection as its
	 * parent...make sure that it loses that parent */
	hash_table_foreach (events, (HashFunc) close_conn, c);
}

/*****************************************************************************/

#if 0
static int add_conn (unsigned long key, IFEvent *event, Connection *c)
{
	if (event->persist)
	{
		/* add to the list of responded connections for this event */
		if (!list_find (event->connections, c))
			event->connections = list_append (event->connections, c);
	}

	return TRUE;
}
#endif

void if_event_attach (Connection *c)
{
	assert (c != NULL);

	/* we already know this is an attached connection */
	if (list_find (attached, c))
		return;

	attached = list_append (attached, c);

	/* TODO -- better way of handling this */
	c->data = "attached";

#if 0
	watch_add (c);

	/* add this to the input list for persisting events */
	hash_table_foreach (events, (HashFunc) add_conn, c);
#endif
}

/*****************************************************************************/

void *if_event_data (IFEventID id)
{
	IFEvent *event;

	event = hash_table_lookup (events, id);

	return (event ? event->data : NULL);
}

/*****************************************************************************/

/* mostly duplicated from interface.c:interface_send */
void if_event_reply (IFEventID id, char *event_name, ...)
{
	IFEvent *event;
	List    *ptr;
	va_list  args;
	char    *data;
	size_t   len;

	if (!(event = hash_table_lookup (events, id)) || !event_name)
		return;

	/* uhh, no one to send this too, sigh */
	if (!event->c && !attached)
		return;

	va_start (args, event_name);
	data = interface_construct_packet (&len, event_name, args);
	va_end (args);

	if (event->c)
		queue_add_single (event->c, NULL, NULL, STRDUP (data), I_PTR (len));

	/* TODO -- i'm really not sure if this logic is good, but it fixes a
	 * pretty major bug that went unnoticed */
	if (!event->persist)
		return;

	/* loop through all reply connections listed for this event */
	for (ptr = attached; ptr; ptr = list_next (ptr))
	{
		/* do not send twice to the host that started this tag */
		if (ptr->data == event->c)
			continue;

		queue_add_single (ptr->data, NULL, NULL, STRDUP (data), I_PTR (len));
	}
}
