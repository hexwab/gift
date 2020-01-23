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

static int queue_write_default (Connection *c, char *data, size_t *data_len)
{
	size_t len;

	if (!data)
		return FALSE;

	len = (data_len && *data_len) ? *data_len : strlen (data);
	net_send (c->fd, data, len);

	return FALSE;
}

static int queue_destroy_default (Connection *c, char *data, size_t *data_len)
{
	free (data);
	free (data_len);

	return FALSE;
}

/*****************************************************************************/

static void queue_write (Protocol *p, Connection *c)
{
	WriteQueue *queue;
	void       *data;
	int         redo = FALSE;

	if (!c->write_queue->list)
	{
		queue_free (c);
		return;
	}

	queue = c->write_queue->list->data;

	data = list_queue_shift (queue->buffer);

	/* TODO -- should destroy_func be called if func asked to repeat? */
	redo  = (*queue->func) (c, data, queue->udata);
	redo |= (*queue->destroy_func) (c, data, queue->udata);

	/* refuse to redo the sentinel element */
	if (redo && data)
		list_queue_unshift (queue->buffer, data);

	/* no more data left with this WriteQueue element, free it and move to
	 * the next */
	if (!queue->buffer->list)
	{
		/* EOF notify */
		(*queue->func) (c, NULL, queue->udata);

		list_queue_free (queue->buffer);
		free (queue);

		list_queue_shift (c->write_queue);

		/* no more queues for this connection at all, so get rid of the queue
		 * subsystem */
		if (!c->write_queue->list)
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

	if (!(queue = malloc (sizeof (WriteQueue))))
		return;

	queue->c            = c;
	queue->func         = func;
	queue->destroy_func = destroy_func;
	queue->buffer       = list_queue_new (buffer);
	queue->udata        = data;

	if (!c->write_queue)
	{
		input_add (c->protocol, c, INPUT_WRITE, queue_write, FALSE);
		c->write_queue = list_queue_new (NULL);
	}

	list_queue_push (c->write_queue, queue);
}

void queue_flush (Connection *c)
{
	/* queue_write {may,will} call queue_free, which will set c->write_queue
	 * to NULL */
	while (c->write_queue && c->write_queue->list)
		queue_write (c->protocol, c);

	/* just in case */
	queue_free (c);
}

void queue_free (Connection *c)
{
	if (c->write_queue)
	{
		list_queue_free (c->write_queue);
		c->write_queue = NULL;
	}

	input_remove_full (NULL, c, INPUT_WRITE, queue_write);
}
