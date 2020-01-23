/*
 * daemon.h
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

#ifndef __DAEMON_H
#define __DAEMON_H

/*****************************************************************************/

struct _daemon_event;

typedef int (*ParsePacketFunc) (char *head, int keys,
                                GData *dataset, struct _daemon_event *event);

/*****************************************************************************/

typedef struct _daemon_event
{
	/* event identifer */
	unsigned long id;

	/* handler callback */
	ParsePacketFunc cb;

	/* actual event data to supply to ParsePacketFunc */
	void *obj;
	void *data;
} DaemonEvent;

/*****************************************************************************/

FEConnection *daemon_connect      ();
FEConnection *daemon_interface    ();
void          daemon_event_remove (unsigned long id);
int           daemon_request      (ParsePacketFunc cb, void *event_obj,
                                   void *udata, char *fmt, ...);
void          daemon_set_primary  (char *host, unsigned short port);

/*****************************************************************************/

#endif /* __DAEMON_H */
