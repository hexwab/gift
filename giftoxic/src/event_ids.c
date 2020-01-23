/* giFToxic, a GTK2 based GUI for giFT
 * Copyright (C) 2002, 2003 giFToxic team (see AUTHORS)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include "common.h"
#include "event_ids.h"

static GSList *event_ids = NULL;

EventID *event_id_add(gint i, EventType type)
{
	EventID *id;

	id = g_new0(EventID, 1);
	id->num = i;
	id->type = type;

	event_ids = g_slist_prepend(event_ids, id);

	return id;
}

void event_id_remove(EventID *id)
{
	event_ids = g_slist_remove(event_ids, id);
	g_free(id);
}

static gint find_id_in_list(gconstpointer a, gconstpointer b)
{
	EventID *id;

	id = (EventID *) a;

	if (id->num == (gint) b)
		return 0;
	else if (id->num < (gint) b)
		return -1;
	else
		return 1;
}

EventID *event_id_lookup(gint i)
{
	EventID	*id;
	GSList	*item;

	if (!event_ids)
		return NULL;

	item = g_slist_find_custom(event_ids, (gpointer) i, (GCompareFunc) find_id_in_list);

	if (item) {
		id = (EventID *) item->data;
		return id;
	} else
		return NULL;
}

gint event_id_get_new(EventType type)
{
	EventID *id = NULL;
	GSList	*item;
	gint	i;

	if (event_ids)
		for (i = 1; i < G_MAXINT; i++) {
			item = g_slist_find_custom(event_ids, (gpointer) i,	(GCompareFunc) find_id_in_list);

			if (!item) {
				id = event_id_add(i, type);
				break;
			}
		}

	if (!id)
		id = event_id_add(1, type);

	return id->num;
}

void event_id_foreach(GFunc func)
{
	if (event_ids && func)
		g_slist_foreach(event_ids, func, NULL);
}
