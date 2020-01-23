/*
 * fe_daemon.c
 *
 * communication with the giFT daemon
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

#include "network.h"
#include "nb.h"
#include "fe_connect.h"

#include "fe_daemon.h"
#include "parse.h"

#include "fe_transfer.h"
#include "fe_download.h"
#include "fe_upload.h"
#include "fe_fifo.h"

static FEConnection   *daemon_conn = NULL;

static char           *daemon_host = NULL;
static unsigned short  daemon_port = 0;

/* currently active events */
static GHashTable     *event_table   = NULL;

/*****************************************************************************/

typedef int (*FEEventHandler) (FEConnection *c, char *head, GData *datalist);
#define FE_EV_HANDLER(func) \
    static int fe_handler_##func (FEConnection *c, char *head, GData *datalist)

FE_EV_HANDLER (event);
FE_EV_HANDLER (transfer);

struct _fe_ev_handler
{
	char           *name;
	FEEventHandler  cb;
}
ev_handlers[] =
{
	{ "event",    fe_handler_event    },
	{ "transfer", fe_handler_transfer },
	{ NULL,       NULL                },
};

/*****************************************************************************/

static DaemonEvent *daemon_event_new (ParsePacketFunc cb, void *obj,
                                      void *udata, int expect_event)
{
	DaemonEvent *event;

	event = malloc (sizeof (DaemonEvent));
	assert (event);

	event->id   = -1;
	event->cb   = cb;
	event->obj  = obj;
	event->data = udata;

	if (expect_event)
		set_last_event (event);

	gift_fe_debug ("last_event_size: %d\n", get_last_event_size ());

	return event;
}

static void daemon_event_add (DaemonEvent *event)
{
	assert (event);

	if (!event_table)
		event_table = g_hash_table_new (NULL, NULL);

	g_hash_table_insert (event_table, GUINT_TO_POINTER (event->id), event);
}

void daemon_event_remove (unsigned long id, char *head)
{
	DaemonEvent *event;

	assert (event_table);
	event = g_hash_table_lookup (event_table, GUINT_TO_POINTER (id));

	if (!event)
	{
		gift_fe_debug ("*** unregistered event %lu removed by %s\n", id, head);
	}
	else
	{
		g_hash_table_remove (event_table, GUINT_TO_POINTER (id));

		/* help the calling interface cleanup after itself */
		(*event->cb) (NULL, 0, NULL, event);

		free (event);
	}
}

/*****************************************************************************/

static int daemon_handle_packet (FEConnection *c, char *head, int keys,
                                 GData *datalist)
{
	struct _fe_ev_handler *ptr;
	DaemonEvent *event;
	unsigned long id;

	for (ptr = ev_handlers; ptr->name; ptr++)
	{
		if (!strcasecmp (ptr->name, head))
		{
			int ret;

			ret = (*ptr->cb) (c, head, datalist);

			/* in the case of <transfer/>, the data may or not really be meant
			 * to be handled in this way, but only the callback will be able
			 * to determine that for us
			 */
			if (!ret)
				break;

			return TRUE;
		}
	}

	id = ATOI (g_datalist_get_data (&datalist, "id"));

	/* <EVENT_NAME id=ID/> means the termination of an event */
	if (keys == 1)
	{
		if (!id)
		{
			gift_fe_debug ("unhandled event %s?!\n", head);
			return FALSE;
		}

		daemon_event_remove (id, "daemon_handle_packet");

		return FALSE;
	}

	/* otherwise, deliver this event message */
	assert (event_table);
	event = g_hash_table_lookup (event_table, GUINT_TO_POINTER (id));

	if (event && event->cb)
		(*event->cb) (head, keys, datalist, event);

	return TRUE;
}

/*****************************************************************************/

FE_EV_HANDLER (event)
{
	DaemonEvent *last_event;

	last_event = get_last_event (TRUE);

	if (!last_event)
	{
		trace ();
		return TRUE;
	}

	assert (datalist);

	last_event->id = ATOI (g_datalist_get_data (&datalist, "id"));

	daemon_event_add (last_event);

	if (last_event->cb)
		(*last_event->cb) (head, 1, datalist, last_event);

	return TRUE;
}

FE_EV_HANDLER (transfer)
{
	DaemonEvent    *event;
	FTApp         *ft_app;
	unsigned long  size;
	unsigned long  id;
	char          *action;
	char          *href;
	char          *hash;

	assert (c);

	ft_app = c->data;

	assert (datalist);

	id     = ATOI (g_datalist_get_data (&datalist, "id"));
	action =       g_datalist_get_data (&datalist, "action");
	size   = ATOI (g_datalist_get_data (&datalist, "size"));
	href   =       g_datalist_get_data (&datalist, "href");
	hash   =       g_datalist_get_data (&datalist, "hash");

	/* This is not an rootnode packet let the callback
	   (upload/download_response) handle it */

	if (!action)
		return FALSE;

	/* action=upload */
	if (!strcasecmp (action, "upload"))
	{
		Transfer *transfer;

		assert (ft_app);

		/* create the upload object */
		transfer     = upload_insert (ft_app->ul_list, datalist);
		assert (transfer);
		transfer->id = id;
		transfer->active = TRANSFER_ACTIVE;
		/* attach the event id */
		event = daemon_event_new ((ParsePacketFunc) upload_response,
		                          transfer, NULL, FALSE);
		assert (event);

		event->id = id;
		daemon_event_add (event);
	}
	else if (!strcasecmp (action, "download"))
	{
		Transfer *transfer;

		if (hash)
		{
			transfer = OBJ_NEW (Transfer);
			assert (transfer);

			obj_set_data (OBJ_DATA (transfer), "tree", ft_app->dl_list);
			share_fill_data (SHARE (transfer), datalist);
			transfer = download_insert (ft_app->dl_list, NULL, SHARE (transfer));

			transfer->total = SHARE (transfer)->filesize;
			event = daemon_event_new ((ParsePacketFunc) download_response,
			                          transfer, NULL, FALSE);

			event->id = id;
			daemon_event_add (event);
		}
	}

	return TRUE;
}

/*****************************************************************************/

/* parse the packet into a gdatalist
 * NOTE - this function is in src/interface.c as is ** BAD ** !!!!!!! */
static char *daemon_parse_packet (GData **datalist, char *packet, int *keys)
{
	char *head;
	char *tag;
	char *value;

	/*    <THIS TAG=VALUE TAG=VALUE/>    \r */

	trim_whitespace (packet);

	if (*packet != '<')
		return FALSE;

	packet++;

	/* THIS TAG=VALUE TAG=VALUE/> */

	if (!(tag = strrchr (packet, '/')))
		return FALSE;

	*tag = 0;

	/* THIS TAG=VALUE TAG=VALUE */

	if (!(tag = string_sep (&packet, " ")))
		return FALSE;

	head = tag;

	/* TAG=VALUE TAG=VALUE */

	while ((tag = string_sep (&packet, "=")))
	{
		if (!packet)
			return FALSE;

		/* support tag=value or tag="value with spaces" */
		if (*packet == '\"')
		{
			packet++;
			value = string_sep (&packet, "\"");
		}
		else
			value = string_sep (&packet, " ");

		if (packet && *packet == ' ')
			packet++;

		if (!strcmp (tag, "head"))
		{
			gift_fe_debug ("'head' is an invalid tag!\n");
			continue;
		}

		if (keys)
			(*keys)++;

		g_datalist_set_data (datalist, tag, value);
	}

	/* (null) */

	return head;
}

/* read data from the daemon, parse into a hash and call the handling
 * routines */
static int daemon_connection (int sock, FEConnection *c)
{
	GData *ft_data = NULL;
	NBRead *nb;
	char *head;
	int keys = 0;
	int n;

#if 0
	head = (sock >= 0 ? "attach" : NULL);

	if (c->data)
	{
		ParsePacketFunc cb;
		void *obj;
		void *data;

		ft_data = c->data;

		cb   = g_datalist_get_data (&ft_data, "cb");
		obj  = g_datalist_get_data (&ft_data, "obj");
		data = g_datalist_get_data (&ft_data, "data");

		if (cb)
			(*cb) (head, 0, NULL, obj, data);

		g_datalist_clear (&ft_data);
		ft_data = NULL;

		c->data = NULL;
	}
#endif

	if (sock < 0)
		return FALSE;

	/* retrieve a packet for parsing */
	nb = nb_active (sock);

	/* protocol is in the form:
	 *
	 *    <TYPE [[tag="value"] [tagn="value"] ...]/>
	 */
	n = nb_read (nb, sock, 0, "\n");
	if (n <= 0)
	{
		nb_finish (nb);
		return FALSE;
	}

	/* we didn't get a complete packet...keep trying */
	if (!nb->term)
		return TRUE;

	head = daemon_parse_packet (&ft_data, nb->data, &keys);

	if (!head)
	{
		gift_fe_debug ("parse error: data = '%s'\n", nb->data);
		g_datalist_clear (&ft_data);
		nb_finish (nb);
		return FALSE;
	}

	daemon_handle_packet (c, head, keys, ft_data);

	g_datalist_clear (&ft_data);
	nb_finish (nb);

	return TRUE;
}

/*****************************************************************************/

/* interface for the daemon functions... */
FEConnection *daemon_connect (FEConnectionCallback cb, void *data)
{
	if (daemon_conn)
	{
		gift_fe_debug ("*** disconnecting previous daemon connection %p\n",
		        daemon_conn);

		fe_connect_close (daemon_conn);
	}

	daemon_conn =
	    fe_connect_dispatch (daemon_host, daemon_port,
	                         (FEConnectionCallback) daemon_connection,
	                         GDK_INPUT_READ, cb, data);

	return daemon_conn;
}

/*****************************************************************************/

FEConnection *daemon_interface ()
{
	return daemon_conn;
}

/*****************************************************************************/

int daemon_request (ParsePacketFunc cb, void *event_obj, void *udata,
                    char *fmt, ...)
{
	va_list      args;
	char        *request;

	if (!fmt || !daemon_conn || !daemon_conn->attached)
		return FALSE;

	daemon_event_new (cb, event_obj, udata, TRUE);

	/* construct the actual data we're going to be sending */
	va_start (args, fmt);
	request = g_strdup_vprintf (fmt, args);
	va_end (args);

	net_send (daemon_conn->fd, request, 0);
	g_free (request);
	return TRUE;
}

/*****************************************************************************/

/*	This function should be used for daemon orders that don't
	generate an <event id=X/> response */

int daemon_send (char *fmt, ...)
{
	va_list      args;
	char        *request;

	if (!fmt || !daemon_conn || !daemon_conn->attached)
		return FALSE;

	/* construct the actual data we're going to be sending */
	va_start (args, fmt);
	request = g_strdup_vprintf (fmt, args);
	va_end (args);
	net_send (daemon_conn->fd, request, 0);
	g_free (request);
	return TRUE;
}

/*****************************************************************************/

/* set the daemon host and port */
void daemon_set_primary (char *host, unsigned short port)
{
	gift_fe_debug ("primary connection set to %s:%hu\n", host, port);
	daemon_host = STRDUP (host);
	daemon_port = port;
}

char* daemon_get_host()
{
	return daemon_host;
}

int daemon_get_port()
{
	return daemon_port;
}
