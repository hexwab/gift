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

static IFEventID  id_counter = 0;      /* uniq_id */
static Dataset   *events     = NULL;   /* table of all events by id */
static List      *attached   = NULL;   /* list of all attached connections */

static int add_reply (IFEvent *event, Connection *c, IFEventID session_id);

/*****************************************************************************/

static IFConnection *if_connection_new ()
{
	IFConnection *ifc;

	/* create the IFConnection */
	if (!(ifc = malloc (sizeof (IFConnection))))
		return NULL;

	memset (ifc, 0, sizeof (IFConnection));

	return ifc;
}

static void if_connection_free (IFConnection *ifc)
{
	dataset_clear (ifc->events);
	free (ifc);
}

static int add_attached (Dataset *d, DatasetNode *node, Connection *c)
{
	IFEvent *event = node->value;

	/* register this connection as if it was present at the time of the events
	 * creation, allowing if_connection_set_id to be used appropriately */
	if (event->flags & IFEVENT_BROADCAST)
	{
		add_reply (event, c, 0);

		/* new attached connection, call the appropriate func */
		if (event->attached)
			(*event->attached) (c, event, event->attached_data);
	}

	return FALSE;
}

static void if_connection_attach (Connection *c)
{
	assert (c != NULL);

	/* we already know this is an attached connection */
	if (list_find (attached, c))
		return;

	if (!(c->data = if_connection_new ()))
		return;

	/* track all attached connections for all _future_ broadcast events */
	attached = list_append (attached, c);

	/* notify all broadcast events that there is a new attached connection */
	dataset_foreach (events, DATASET_FOREACH (add_attached), c);
}

static int remove_attach (Dataset *d, DatasetNode *node, Connection *c)
{
	IFEvent *event = node->value;

	event->reply = list_remove (event->reply, c);
	return FALSE;
}

static int remove_initiate (Dataset *d, DatasetNode *node, Connection *c)
{
	IFEvent *event = node->value;

	if (event->initiate == c)
		event->initiate = NULL;

	return FALSE;
}

static int remove_nonpersist (Dataset *d, DatasetNode *node, List **list)
{
	IFEvent *event = node->value;

	if (event->reply || event->flags & IFEVENT_PERSIST)
		return FALSE;

	/* this event is ready to go */
	*list = list_prepend (*list, event);
	return FALSE;
}

static int finish_nonpersist (IFEvent *event, void *udata)
{
	if_event_finish (event);
	return TRUE;
}

/* TODO -- we need to add code to remove a non-persistsing even it this
 * was the last connection associated w/ it */
static void if_connection_detach (Connection *c)
{
	List         *rm_events = NULL;
	IFConnection *ifc;

	/* if_event_finish (called from finish_nonpersist) might free c, be
	 * careful */
	ifc = c->data;

	/* poorly named function */
	dataset_foreach (events, DATASET_FOREACH (remove_attach), c);

	/* build a list of all events which have nowhere to reply to and do
	 * NOT have IFEVENT_PERSIST set.  the reason we are building a list
	 * rather than using hash_table_foreach_remove is because if_event_finish
	 * is very dangerous to use while "in" the events tables */
	dataset_foreach (events, DATASET_FOREACH (remove_nonpersist), &rm_events);
	list_foreach_remove (rm_events, (ListForeachFunc) finish_nonpersist, NULL);

	/* now make sure all persisting events which originated from this
	 * connection lose their initiate */
	dataset_foreach (events, DATASET_FOREACH (remove_initiate), c);

	/* handle the attached cleanup */
	if (ifc)
	{
		c->data = NULL;

		attached = list_remove (attached, c);

		/* cleanup */
		if_connection_free (ifc);
	}
}

/*
 * this function effectively sets up the translation table for this event on
 * the current interface connection.  session_id is directly translated if
 * non-zero, otherwise a new event identifier will be created
 */
static IFEventID if_connection_set_id (IFConnection *ifc, IFEvent *event,
                                       IFEventID session_id)
{
	if (!ifc || event->flags & IFEVENT_NOID)
		return 0;

	if (session_id == 0)
		session_id = dataset_uniq32 (ifc->events, &id_counter);
	else
	{
		/* a session id hint was given but it was already defined, fail
		 * miserably */
		if (dataset_lookup (ifc->events, &session_id, sizeof (session_id)))
		{
			GIFT_ERROR (("overlapping session id %lu supplied",
			             (unsigned long)session_id));
			return 0;
		}
	}

	if (!ifc->events)
		ifc->events = dataset_new (DATASET_HASH);

	dataset_insert (&ifc->events, &session_id, sizeof (session_id), event, 0);

	return session_id;
}

static int find_id (Dataset *d, DatasetNode *node, IFEvent *match)
{
	IFEvent *event = node->value;

	if (event->id == match->id)
		return TRUE;

	return FALSE;
}

/*
 * returns the session-specific identifier when given the actual event
 * to lookup
 */
IFEventID if_connection_get_id (IFConnection *ifc, IFEvent *event)
{
	IFEventID    session_id;
	DatasetNode *node;

	if (!ifc || !event)
		return 0;

	node = dataset_find_node (ifc->events, DATASET_FOREACH (find_id), event);

	if (!node || !node->key || node->key_len != sizeof (session_id))
		return 0;

	memcpy (&session_id, node->key, node->key_len);
	return session_id;
}

/*
 * returns the gift-specific event when given the session-specific
 * identifier
 */
IFEvent *if_connection_get_event (IFConnection *ifc, IFEventID id)
{
	if (!ifc || !id)
		return NULL;

	return dataset_lookup (ifc->events, &id, sizeof (id));
}

/*****************************************************************************/

static int add_reply (IFEvent *event, Connection *c, IFEventID session_id)
{
	/* handle the session specific identifer */
	if (!c)
		return FALSE;

	if_connection_set_id (c->data, event, session_id);

	if (!list_find (event->reply, c))
		event->reply = list_prepend (event->reply, c);

	return TRUE;
}

static int add_new (Connection *c, IFEvent *event)
{
	add_reply (event, c, 0);
	return TRUE;
}

IFEvent *if_event_new (Connection *c, IFEventID session_id, IFEventFlags flags,
                       IFEventCB attached_cb, void *attached_data,
                       IFEventCB finished_cb, void *finished_data,
                       char *data_name, void *data)
{
	IFEvent  *event;
	IFEventID id;

	if (!(event = malloc (sizeof (IFEvent))))
		return NULL;

	memset (event, 0, sizeof (IFEvent));

	if (!(id = dataset_uniq32 (events, &id_counter)))
	{
		free (event);
		return NULL;
	}

	event->initiate      = c;
	event->id            = id;
	event->flags         = flags;
	event->attached      = attached_cb;
	event->attached_data = attached_data;
	event->finished      = finished_cb;
	event->finished_data = finished_data;
	event->data_name     = STRDUP (data_name);
	event->data          = data;

	/* create the reply list for this event */
	add_reply (event, c, session_id);

	if (flags & IFEVENT_BROADCAST)
		list_foreach (attached, (ListForeachFunc) add_new, event);

	/* add the event to the reference table */
	if (!events)
		events = dataset_new (DATASET_HASH);

	dataset_insert (&events, &id, sizeof (id), event, 0);

	return event;
}

static void if_event_free (IFEvent *event)
{
	list_free (event->reply);

	free (event->data_name);
	free (event);
}

static int remove_event (Connection *c, IFEvent *event)
{
	IFConnection *ifc;

	if (!(ifc = c->data))
		return TRUE;

	/* find and remove the id at the same time ;) */
	dataset_foreach (ifc->events, DATASET_FOREACH (find_id), event);
	return TRUE;
}

void if_event_finish (IFEvent *event)
{
	Connection *initiate;

	if (!event)
		return;

	initiate = event->initiate;

	if (event->finished)
		(*event->finished) (event->initiate, event, event->finished_data);

	list_foreach (event->reply, (ListForeachFunc) remove_event, event);

	dataset_remove (events, &event->id, sizeof (event->id));
	if_event_free (event);

	/* if the initiate's connection data is non-null, it is an attached
	 * connection and therefore should NOT be closed when this event
	 * completes */
	if (initiate && !initiate->data)
	{
		if_event_detach (initiate);
		connection_close (initiate);
	}
}

/*
 * sends the given message to all sockets which are on the reply list for
 * this event.  also handles session-specific identifier translation
 */
void if_event_send (IFEvent *event, Interface *cmd)
{
	IFEventID session_id;
	List     *ptr;

	if (!event || !cmd)
		return;

	/* uhh, no one to send this too, sigh */
	if (!event->reply)
		return;

	/* loop through all reply connections listed for this event */
	for (ptr = event->reply; ptr; ptr = list_next (ptr))
	{
		Connection *c;

		if (!(c = ptr->data))
			continue;

		/* we may not actually use session_id but we'll look it up anyway
		 * just to catch errors */
		if ((session_id = if_connection_get_id (c->data, event)) &&
			!(event->flags & IFEVENT_NOID))
		{
			/* hack the command value */
			interface_set_value (cmd, stringf ("%lu", session_id));
		}

		/* interface_construct returns allocated storage */
		interface_send (cmd, ptr->data);
	}
}

/*****************************************************************************/

void *if_event_data (IFEvent *event, char *data_name)
{
	if (!event)
		return NULL;

	/* trying to access inappropriate data */
	if (STRCMP (event->data_name, data_name))
		return NULL;

	return event->data;
}

/*****************************************************************************/

IFEvent *if_event_lookup (IFEventID id)
{
	return dataset_lookup (events, &id, sizeof (id));
}

/*****************************************************************************/

void if_event_attach (Connection *c)
{
	TRACE_SOCK (("%p", c));
	if_connection_attach (c);
}

void if_event_detach (Connection *c)
{
	TRACE_SOCK (("%p", c));
	if_connection_detach (c);
}
