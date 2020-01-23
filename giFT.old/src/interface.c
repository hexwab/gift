/*
 * interface.c
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

#include "daemon.h"
#include "queue.h"
#include "parse.h"
#include "network.h"
#include "event.h"
#include "nb.h"

/*****************************************************************************/

/* Getopt::Long.pm like. */
char *interface_construct_packet (int *len, char *event, va_list args)
{
	char       *tag;
	char       *value;
	static char string[2048];
	int         string_len = 0;

	string_len += sprintf (string, "<%s", event);

	for (;;)
	{
		char *type;

		if (!(tag = va_arg (args, char *)))
			break;

		tag = strdup (tag);

		/* parse TAG=string */
		if (!(type = strchr (tag, '=')))
		{
			free (tag);
			break;
		}

		*type++ = 0;

		switch (*type)
		{
		 case 's':
			value = va_arg (args, char *);
			break;
		 case 'i':
			value = ITOA (va_arg (args, long)); /* promote */
			break;
		 default:
			value = NULL;
			break;
		}

		if (!value)
			value = "";

		if (string_len + strlen (tag) + strlen (value) < sizeof (string) - 12)
		{
			string_len += sprintf (string + string_len,
			                       " %s=\"%s\"", tag, value);
		}

		free (tag);
	}

	string_len += sprintf (string + string_len, "/>\r\n");

	if (len)
		*len = string_len;

	return string;
}

/* constructs a xml-like packet resembling <HEAD TAG=VALUE.../>\n and sends
 * it to the supplied socket via the queue subsystem */
void interface_send (Connection *c, char *event, ...)
{
	va_list args;
	char   *data;
	int     len;

	if (!c || !event)
		return;

	va_start (args, event);
	data = interface_construct_packet (&len, event, args);
	va_end (args);

	queue_add_single (c, NULL, NULL, STRDUP (data), (void *)len);
}

/* stupid helper function */
void interface_send_err (Connection *c, char *error)
{
	interface_send (c, "error", "text=s", error, NULL);
}

/* called any time a dc connection was closed.  handles registering with the
 * protocol, flushing the queue, and closing the socket as well as any
 * arbitrary event specific cleanups that must be handled */
void interface_close (Connection *c)
{
	/* dc_close may call this function, clean up the logic in the protocol
	 * by simply avoiding that condition */
	if (!c || c->closing)
		return;

	c->closing = 1;

	if (c->protocol && c->protocol->dc_close)
		c->protocol->dc_close (c);

	/* if_event_close_func (c); */

	queue_flush (c);

	input_remove (c);
	net_close (c->fd);
	connection_destroy (c);
}

/*****************************************************************************/

/* sure as hell beats anubis' parsing nightmare :) */
int interface_parse_packet (Dataset **dataset, char *packet)
{
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

	dataset_insert (*dataset, "head", STRDUP (tag));

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
			GIFT_WARN (("'head' is an invalid tag!"));
			continue;
		}

		dataset_insert (*dataset, tag, STRDUP (value));
	}

	/* (null) */

	return TRUE;
}

/* main reading loop */
static void interface_connection (Protocol *p, Connection *c)
{
	Dataset *dataset = NULL;
	NBRead *nb;
	int n;

	nb = nb_active (c->fd);

	n = nb_read (nb, c->fd, 0, "\n");
	if (n <= 0)
	{
		nb_finish (nb);
		interface_close (c);
		return;
	}

	if (!nb->term)
		return;

	interface_parse_packet (&dataset, nb->data);

	nb_finish (nb);

	if (!dataset)
	{
		TRACE (("invalid data '%s'", nb->data));
		interface_close (c);
		return;
	}

	dc_handle_event (p, c, dataset);

	dataset_clear_free (dataset);
}

static void interface_handle_incoming (Protocol *p, Connection *c)
{
	Connection *new_c;

	new_c = connection_new (p);
	new_c->fd = net_accept (c->fd);

	input_add (p, new_c, INPUT_READ,
	           (InputCallback) interface_connection, FALSE);
}

/*****************************************************************************/

int interface_init (unsigned short port)
{
	Connection *listen;

	listen = connection_new (NULL);

	if ((listen->fd = net_bind (port)) < 0)
	{
		connection_destroy (listen);
		return FALSE;
	}

	input_add (NULL, listen, INPUT_READ,
	           (InputCallback) interface_handle_incoming, FALSE);

	return TRUE;
}
