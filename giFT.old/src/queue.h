/*
 * queue.h
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

#ifndef __QUEUE_H
#define __QUEUE_H

/*****************************************************************************/

typedef void (*QueueWriteFunc) (Connection *c, void *data, void *udata);

typedef struct _write_queue
{
	Connection    *c;
	QueueWriteFunc func;
	QueueWriteFunc destroy_func;
	List          *buffer;
	void          *udata;
} WriteQueue;

/*****************************************************************************/

void queue_add_single (Connection *c, QueueWriteFunc func,
					   QueueWriteFunc destroy_func, void *write_data,
					   void *data);
void queue_add        (Connection *c, QueueWriteFunc func,
					   QueueWriteFunc destroy_func, List *buffer, void *data);
void queue_flush      (Connection *c);
void queue_free       (Connection *c);

/*****************************************************************************/

#endif /* __QUEUE_H */
