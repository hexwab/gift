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
#include "fe_ui_utils.h"

#include "platform.h"
#include "log.h"

/*****************************************************************************/

Config        *gift_fe_conf = NULL;

/* kludgy hack to get FE to compile with *everything* from src/ */
Config         *gift_conf;        /* main configuration file     */
#include "list.h"
List           *protocols = NULL; /* all fully loaded protocols  */

/*****************************************************************************/

void gift_fe_debug (char *fmt, ...)
{
	va_list      args;

	if (gift_fe_conf && config_get_int(gift_fe_conf, "general/debug=0"))
	{
		va_start (args, fmt);
		GIFT_DEBUG ((fmt, args));
		va_end (args);
	}
}

static int send_attach (int sock, FEConnection *c)
{
	if (sock < 0)
	{
		gift_message_box ("Connection lost",
		                  "Unexpected loss of connection from the giFT "
		                  "daemon\n"
		                  "Please verify that you are indeed running the "
		                  "appropriate giFT version.");

		return FALSE;
	}

	gift_fe_debug ("sending attach to %i\n", c->fd);
	net_send (c->fd,  "<attach "
			  "client=\"" GIFT_FE_PACKAGE "\" "
			  "version=\"" GIFT_FE_VERSION "\"/>\n", 0);
	c->attached=TRUE;
	return TRUE;
}

#ifndef _WINDOWS
int main (int argc, char **argv)
{
#else
int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
					LPSTR cmd_line, int cmd_show)
{
	int    argc = 0;
	char **argv = NULL;
#endif

	FTApp main_dialog;

	/* initialize platform specific stuff */
	platform_init ();

	log_init (GLOG_DEFAULT, GIFT_FE_PACKAGE, 0, 0, 
			  gift_conf_path ("giFTfe.log"));

	GIFT_INFO (("%s %s (%s %s) started\n", GIFT_FE_PACKAGE, GIFT_FE_VERSION, 
			  __DATE__, __TIME__));

	gtk_init (&argc, &argv);

	/* parse the configuration table */
	gift_fe_conf = gift_config_new ("ui");
	config_set_int (gift_fe_conf,"general/debug", 1);

	daemon_set_primary (config_get_str (gift_fe_conf, "daemon/host=127.0.0.1"),
	                    (unsigned short) config_get_int (gift_fe_conf,
	                    "daemon/port=1213"));

	/*
	 * and the fun begins ... ;)
	 */
	construct_main (&main_dialog, "giFT", "gift");

	/* make the initial daemon connection */
	daemon_connect ((FEConnectionCallback) send_attach, &main_dialog);

	gtk_main ();

	if (gift_fe_conf)
		config_free (gift_fe_conf);

	platform_cleanup ();

	return 0;
}
