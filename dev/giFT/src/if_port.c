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
	Interface *cmd;
	NBRead    *nb;
	int        n;

	nb = nb_active (c->fd);

	if ((n = nb_read (nb, 0, ";")) <= 0)
	{
		if_event_detach (c);
		connection_close (c);
		return;
	}

	if (!nb->term)
		return;

	/*
	 * This can occur because our nb_read loop is actually pretty awful:
	 * search query(foo\;bar); is perfectly valid, however the first ; will
	 * be matched and interface_unserialize should fail.  There is obviously
	 * a much better way of handling this, but for the moment I'm being lazy
	 * so we'll chalk this up as a TODO.
	 */
	if (!(cmd = interface_unserialize (nb->data, nb->len)))
	{
		TRACE (("packet '%s' appears incomplete, waiting for more input...",
		        nb->data));
		nb->term = FALSE;
		return;
	}

	daemon_command_handle (c, cmd);
	interface_free (cmd);
}

/*****************************************************************************/

static int if_auth (Connection *c)
{
	in_addr_t ip;
	char     *allow, *allow0;
	char     *host;
	int       ret = FALSE;

	if (!c || c->fd <= 0)
		return FALSE;

	allow0 = allow =
	    STRDUP (config_get_str (gift_conf, "main/hosts_allow=LOCAL"));

	ip = net_ip (net_peer_ip (c->fd));

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
