/*
 * fe_fifo.c
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

#include "gift-fe.h"

#include "fe_fifo.h"

#include "network.h"
#include "nb.h"
#include "fe_connect.h"

/*****************************************************************************/

static GSList *event_list = NULL;

void set_last_event (DaemonEvent *new)
{
	event_list = g_slist_append (event_list, new);
}

int get_last_event_size ()
{
	if (!event_list)
		return 0;

	return g_slist_length (event_list);
}

DaemonEvent *get_last_event (int rm)
{
	GSList *temp;
	DaemonEvent *lde_temp;

	if (rm)
	{
		assert (event_list);
		temp = event_list->next;
		lde_temp = (DaemonEvent *) event_list->data;
		g_slist_free_1 (event_list);
		event_list = temp;
		return lde_temp;
	}
	else
	{
		assert (event_list);
		return (DaemonEvent *) event_list->data;
	}
}
