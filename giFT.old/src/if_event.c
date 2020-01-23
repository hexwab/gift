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
#include "watch.h"

#include "if_event.h"

#include "download.h"
#include "upload.h"

/*****************************************************************************/

static IFEventID  id_counter = 0;
static HashTable *events     = NULL;

/*****************************************************************************/

/* TODO - opt */
static IFEventID uniq_id ()
{
	id_counter++;

	if (!id_counter)
		id_counter++;

	/* this could be dangerous, verify later */
	while (hash_table_lookup (events, id_counter))
		id_counter++;

	return id_counter;
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

	/* c == NULL is a special condition indicating the origin is unknown, but
	 * should be reported to all attached connections */
	if (!c)
		event->connections = watch_copy ();
	else
		event->connections = list_append (NULL, c);

	event->id   = id;
	event->c    = c;
	event->type = type;
	event->func = func;
	event->data = udata;

	if (type == IFEVENT_TRANSFER)
		event->persist = TRUE;

	if (!events)
		events = hash_table_new ();

	hash_table_insert (events, id, event);

	/* attached, send the id */
	if (c && c->data)
		interface_send (c, "event", "id=i", id, NULL);

	return id;
}

static void if_event_free (IFEvent *event)
{
	list_free (event->connections);
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
	event->connections = list_remove (event->connections, c);

	if (event->c == c)
	{
		/* TODO - we must assume all events persist for this logic to
		 * work */
#if 0
		if (!event->persist)
			if_event_remove (event->id);
		else
#endif
			event->c = NULL;
	}

	return TRUE;
}

/* called when a connection is closed so that we may properly cleanup */
void if_event_close (Connection *c)
{
	assert (c != NULL);

	/* if this is an attached connection, remove it */
	watch_remove (c);

	/* input will be changed to the returned Connection if it was found */
	hash_table_foreach (events, (HashFunc) close_conn, c);
}

/*****************************************************************************/

static int add_conn (unsigned long key, IFEvent *event, Connection *c)
{
	if (event->persist)
		event->connections = list_append (event->connections, c);

	return TRUE;
}

void if_event_attach (Connection *c)
{
	assert (c != NULL);

	/* we already know this is an attached connection */
	watch_add (c);

	/* TODO -- this should be genralized */
	download_report_attached (c);
	upload_report_attached   (c);

	/* add this to the input list for persisting events */
	hash_table_foreach (events, (HashFunc) add_conn, c);
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

	if (!event->connections)
		return;

	va_start (args, event_name);
	data = interface_construct_packet (&len, event_name, args);
	va_end (args);

	/* loop through all reply connections listed for this event */
	for (ptr = event->connections; ptr; ptr = list_next (ptr))
	{
		queue_add_single (ptr->data, NULL, NULL, STRDUP (data), (void *) len);
	}
}

/*****************************************************************************/

#if 0
unsigned long interface_event_new (Connection *c, InterfaceCloseFunc func,
                                   void *ev_data)
{
	InterfaceEvent *event;

	/* locate the id */
	if (!id_counter)
		id_counter++;

	if (!id_table)
		id_table = hash_table_new ();

	while (hash_table_lookup (id_table, id_counter))
		id_counter++;

	/* construct the event */
	event = malloc (sizeof (InterfaceEvent));
	event->c    = c;
	event->func = func;
	event->data = ev_data;

	c->ev_id = id_counter;
	hash_table_insert (id_table, c->ev_id, event);

	return c->ev_id;
}

void interface_event_remove (Connection *c)
{
	if (c->ev_id)
		hash_table_remove (id_table, c->ev_id);
}

void *interface_event_data (Connection *c)
{
	InterfaceEvent *event;

	event = hash_table_lookup (id_table, c->ev_id);

	return (event ? event->data : event);
}

/*****************************************************************************/

static int locate_conn (unsigned long key, InterfaceEvent *event, void **udata)
{
	if (event->data == *udata)
	{
		*udata = event->c;
		return FALSE;
	}

	return TRUE;
}

Connection *interface_event_conn (void *data)
{
	void *input;

	if (!data)
		return NULL;

	input = data;

	/* input will be changed to the returned Connection if it was found */
	hash_table_foreach (id_table, (HashFunc) locate_conn, &input);

	if (input != data)
		return input;

	return NULL;
}

/*****************************************************************************/

void if_event_remove (Connection *c)
{
	InterfaceEvent *event;

	event = hash_table_lookup (id_table, c->ev_id);

	if (event && event->func)
		(*event->func) (c, event->data);
}

#endif
