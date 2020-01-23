/*
 * interface.h
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

#ifndef __INTERFACE_H
#define __INTERFACE_H

/*****************************************************************************/

typedef void (*InterfaceCloseFunc) (Connection *c, void *data);

typedef struct _interface_event
{
	Connection        *c;

	InterfaceCloseFunc func;
	void              *data;
} InterfaceEvent;

/*****************************************************************************/

char *interface_construct_packet (int *len, char *event, va_list args);
void  interface_send     (Connection *c, char *event, ...);
void  interface_send_err (Connection *c, char *error);
void  interface_close    (Connection *c);

unsigned long interface_event_new    (Connection *c, InterfaceCloseFunc func,
								      void *ev_data);
void          interface_event_remove (Connection *c);
void         *interface_event_data   (Connection *c);
Connection   *interface_event_conn   (void *data);
void          interface_close_func   (Connection *c);

int  interface_init     (unsigned short port);

/*****************************************************************************/

#endif /* __INTERFACE_H */
