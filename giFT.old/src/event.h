/*
 * event.h
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

#ifndef __EVENT_H
#define __EVENT_H

#include <time.h>

#include "protocol.h"
#include "connection.h"

/*****************************************************************************/

typedef void (*InputCallback) (Protocol *p, Connection *c);

typedef enum
{
	INPUT_READ      = 0x01,
	INPUT_WRITE     = 0x02,
	INPUT_EXCEPTION = 0x04
} InputState;

typedef struct
{
	int           fd;
	InputState    state;

	InputCallback callback;
	Protocol     *protocol;
	Connection   *data;

	int           complete; /* complete is FALSE until first state change */
	unsigned long validate; /* timeout for incomplete sockets */
} Input;

/*****************************************************************************/

typedef int (*TimerCallback) (void *data);

typedef struct
{
	int           id;

	int           expiration;
	int           interval;

	TimerCallback callback;
	void         *data;
} Timer;

/*****************************************************************************/

void event_quit ();
void event_loop ();

/*****************************************************************************/

int  timer_add    (time_t interval, TimerCallback callback, void *data);
void timer_remove (unsigned long id);

void input_add         (Protocol *p, Connection *c, InputState state,
                        InputCallback callback, int timeout);
void input_remove_full (Connection *c, InputState state, InputCallback cb);
void input_remove      (Connection *c);

/*****************************************************************************/

#endif /* __EVENT_H */
