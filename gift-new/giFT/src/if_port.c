/*
 * if_port.c
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
#include "interface.h"

#include "network.h"
#include "event.h"
#include "nb.h"

/*****************************************************************************/

extern Config     *gift_conf;
static Connection *if_port_listen = NULL;

/*****************************************************************************/

/* main reading loop */
static void if_connection (Protocol *p, Connection *c)
{
	Dataset *dataset = NULL;
	NBRead *nb;
	int n;

	nb = nb_active (c->fd);

	if ((n = nb_read (nb, 0, "\n")) <= 0)
	{
		interface_close (c);
		return;
	}

	if (!nb->term)
		return;

	interface_parse_packet (&dataset, nb->data);

	if (!dataset)
		TRACE (("invalid data '%s'", nb->data));

	if (!dataset)
	{
		interface_close (c);
		return;
	}

	dc_handle_event (p, c, dataset);

	dataset_clear_free (dataset);
}

/*****************************************************************************/

static int if_auth (Connection *c)
{
	unsigned long ip;
	char *allow, *allow0;
	char *host;
	int ret = FALSE;

	if (!c || c->fd <= 0)
		return FALSE;

	allow0 = allow =
	    STRDUP (config_get_str (gift_conf, "main/hosts_allow=LOCAL"));

	ip = inet_addr (net_peer_ip (c->fd));

	/* loop all available host masks */
	while ((host = string_sep (&allow, " ")))
	{
		if ((ret = net_match_host (ip, host)))
			break;
	}

	free (allow0);

	return ret;
}

static void handle_incoming (Protocol *p, Connection *c)
{
	Connection *new_c;

	new_c     = connection_new (p);
	new_c->fd = net_accept (c->fd, FALSE);

	/* make sure they are allowed to connect ;) */
	if (!if_auth (new_c))
	{
		net_close (new_c->fd);
		connection_free (new_c);
		return;
	}

	input_add (p, new_c, INPUT_READ,
	           (InputCallback) if_connection, FALSE);
}

/*****************************************************************************/

int if_port_init (unsigned short port)
{
	Connection *listen;

	listen = connection_new (NULL);

	if ((listen->fd = net_bind (port, FALSE)) < 0)
	{
		connection_free (listen);
		return FALSE;
	}

	if_port_listen = listen;

	input_add (NULL, listen, INPUT_READ,
	           (InputCallback) handle_incoming, FALSE);

	return TRUE;
}

void if_port_cleanup ()
{
	connection_close (if_port_listen);
	if_port_listen = NULL;
}
