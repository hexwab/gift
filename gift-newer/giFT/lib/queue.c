/*
 * queue.c
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
#include "event.h"
#include "queue.h"

/*****************************************************************************/

static int queue_write_default (Connection *c, char *data, void *udata)
{
	unsigned long len;

	if (!data)
		return FALSE;

	len = (udata ? P_INT (udata) : strlen (data));

	net_send (c->fd, data, len);

	return FALSE;
}

static int queue_destroy_default (Connection *c, char *data, void *udata)
{
	free (data);

	return FALSE;
}

/*****************************************************************************/

static void queue_write (Protocol *p, Connection *c)
{
	WriteQueue *queue;
	void       *data;
	int         redo = FALSE;

	if (!c->write_queue)
	{
		queue_free (c);
		/* input_remove_full (c, INPUT_WRITE, queue_write); */
		return;
	}

	queue = c->write_queue->data;

#if 0
	if (c->last_write)
	{
		if (c->last_write->func != queue->func)
			(*(c->last_write->func)) (c, NULL, queue->udata);
	}
#endif

	queue->buffer = list_shift (queue->buffer, &data);

	/* TODO -- should destroy_func be called if func asked to repeat? */
	redo  = (*queue->func) (c, data, queue->udata);
	redo |= (*queue->destroy_func) (c, data, queue->udata);

	/* refuse to redo the sentinel element */
	if (redo && data)
		queue->buffer = list_unshift (queue->buffer, data);

	/* no more data left with this WriteQueue element, free it and move to
	 * the next */
	if (!queue->buffer)
	{
		/* EOF notify */
		(*queue->func) (c, NULL, queue->udata);

		free (queue);
		c->write_queue = list_shift (c->write_queue, NULL);

		/* no more queues for this connection at all, so get rid of the queue
		 * subsystem */
		if (!c->write_queue)
			queue_free (c);
	}
}

/*****************************************************************************/

void queue_add_single (Connection *c, QueueWriteFunc func,
                       QueueWriteFunc destroy_func, void *write_data,
                       void *data)
{
	List *list;

	list = list_prepend (NULL, write_data);

	queue_add (c, func, destroy_func, list, data);
}

void queue_add (Connection *c, QueueWriteFunc func, QueueWriteFunc destroy_func,
                List *buffer, void *data)
{
	WriteQueue *queue;

	if (!func)
		func = (QueueWriteFunc) queue_write_default;

	if (!destroy_func)
		destroy_func = (QueueWriteFunc) queue_destroy_default;

	if (!buffer)
	{
		(*func) (c, NULL, data);
		return;
	}

	if (!c->write_queue)
		input_add (c->protocol, c, INPUT_WRITE, queue_write, FALSE);

	queue = malloc (sizeof (WriteQueue));

	queue->c            = c;
	queue->func         = func;
	queue->destroy_func = destroy_func;
	queue->buffer       = buffer;
	queue->udata        = data;

	c->write_queue = list_append (c->write_queue, queue);

	/* list_free (head); */
}

void queue_flush (Connection *c)
{
	if (!c->write_queue)
	{
		queue_free (c);
		return;
	}

	while (c->write_queue)
		queue_write (c->protocol, c);

	/* TODO ... calling queue_write once more will call queue_free...
	 * should we do this? */
}

void queue_free (Connection *c)
{
#if 0
	WriteQueue *queue;
	List       *ptr;
	List       *next;
#endif

	input_remove_full (NULL, c, INPUT_WRITE, queue_write);

	if (!c->write_queue)
		return;

	TRACE_FUNC ();

	/* i think something in here is causing the problems node.c was
	 experiencing... */
#if 0
	/* destroy all data associated w/ the queue...very messy */
	for (ptr = c->write_queue; ptr; )
	{
		List *elem;
		List *elem_next;

		next  = list_next (ptr);
		queue = ptr->data;

		for (elem = queue->buffer; elem; )
		{
			elem_next = list_next (elem);

			if (queue->destroy_func)
				(*queue->destroy_func) (queue->c, elem->data, queue->udata);

			free (elem);

			elem = elem_next;
		}

		(*queue->func) (c, NULL, queue->udata);
		free (ptr);

		ptr = next;
	}

	c->write_queue = NULL;
#endif
}
