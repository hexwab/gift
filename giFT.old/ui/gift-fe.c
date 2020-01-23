/*
 * gift-fe.c
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

#include "conf.h"
#include "ui_utils.h"

/*****************************************************************************/

Config       *gift_fe_conf = NULL;

/*****************************************************************************/

static int send_attach (int sock, FEConnection *c)
{
	if (sock < 0)
	{
		gift_message_box ("Connection lost",
		                  "Unexpected loss of connection from the giFT "
		                  "daemon.  Please verify that you are indeed "
		                  "running the appropriate giFT version.");

		return FALSE;
	}

	printf ("sending attach to %i\n", c->fd);
	net_send (c->fd,
			  "<attach "
			  "client=\"" GIFT_FE_PACKAGE "\" "
			  "version=\"" GIFT_FE_VERSION "\"/>\n");

	return TRUE;
}

int main (int argc, char **argv)
{
	FTApp main_dialog;

	gtk_init (&argc, &argv);

	/* parse the configuration table */
	gift_fe_conf = gift_config_new ("ui");

	daemon_set_primary (config_get_str (gift_fe_conf, "daemon/host=127.0.0.1"),
	                    config_get_int (gift_fe_conf, "daemon/port=1213"));

	/*
	 * and the fun begins ... ;)
	 */
	construct_main (&main_dialog, "giFT", "gift");

	/* make the initial daemon connection */
	daemon_connect ((FEConnectionCallback) send_attach, &main_dialog);

	gtk_main ();

	config_free (gift_fe_conf);

	return 0;
}
