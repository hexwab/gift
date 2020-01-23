/*
 * if_event.h
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

#ifndef __IF_EVENT_H
#define __IF_EVENT_H

/*****************************************************************************/

typedef unsigned long IFEventID;

typedef enum
{
	IFEVENT_OPT,
	IFEVENT_SEARCH,
	IFEVENT_TRANSFER,
	IFEVENT_STATS
} IFEventType;

struct _if_event;
typedef void (*IFEventFunc) (struct _if_event *event);

typedef struct _if_event
{
	/* unique identifer associated w/ this event */
	IFEventID id;

	/* list of connections to report status of this event back to.  if
	 * persist is TRUE, this may be NULL */
	List *connections;

	/* connection that initiated this event */
	Connection *c;

	IFEventType type;
	IFEventFunc func;

	/* user data */
	void *data;

	/* if the connection that spawned this event closes, should the event be
	 * closed as well? */
	int persist;
} IFEvent;

/*****************************************************************************/

IFEventID if_event_new    (Connection *c, IFEventType type, IFEventFunc func,
                           void *udata);
void      if_event_remove (IFEventID id);
void      if_event_close  (Connection *c);
void      if_event_attach (Connection *c);
void     *if_event_data   (IFEventID id);
void      if_event_reply  (IFEventID id, char *event, ...);

/*****************************************************************************/

#endif /* __IF_EVENT_H */
