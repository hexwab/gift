/*
 * main.c
 *
 * TODO - better option handling
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

#include "gift.h"
#include "event.h"

#include "download.h"
#include "conf.h"
#include "parse.h"
#include "protocol.h"

#include "sharing.h"

/*****************************************************************************/

#define INTERFACE_PORT 1213

Config         *gift_conf;        /* main configuration file     */
static Dataset *options;          /* command-line parsed options */
List           *protocols = NULL; /* all fully loaded protocols  */

/*****************************************************************************/

static void usage (char *argv0, int exit_val)
{
	char *u = "Usage: %s [options] [protocol-lib...]\n"
              "\n"
              "    -h                  Help\n"
		      "    -v                  Version\n"
			  "    -d                  Fork to background\n"
              "<protocol-lib>          The file locations of the protocols to use\n";

	printf (u, argv0);
	exit (exit_val);
}

static void version ()
{
	/* simplified format (scriptability damnit) - jasta */
	printf ("%s-%s\n", PACKAGE, VERSION);
	exit (0);
}

static void gift_setup_msg ()
{
	fprintf (stderr, "\n");
	fprintf (stderr, "*** Error: your setup is incomplete.  Please run giFT-setup.\n");
	fprintf (stderr, "\n");
}

/*****************************************************************************/

static List *parse (int argc, char **argv)
{
	List *plugin_fnames = NULL;

#ifndef WIN32
	/* use getopt to parse the command line */
	while (1)
	{
		int c;

		/* parse the command line */
		c = getopt (argc, argv, "hvVds");

		if (c == -1) break;

		switch (c)
		{
		 case 0:
			usage (argv[0], 1);
			break;
		 case 'h': /* help */
			usage (argv[0], 0);
			break;
		 case 'v': /* version */
		 case 'V':
			version (argv[0]);
			break;
		 case 'd': /* daemon */
			{
				pid_t cpid;

				if ((cpid = fork ()) != 0)
				{
					printf ("giFT: %u\n", cpid);
					_exit (0);
				}

				setsid ();

				close (STDIN_FILENO);
				close (STDOUT_FILENO);
				close (STDERR_FILENO);
			}
			break;
		 default:  { usage (argv[0], 1); } break;
		}
	}

	while (optind < argc)
		plugin_fnames = list_append (plugin_fnames, argv[optind++]);
#endif /* !WIN32 */

	return plugin_fnames;
}

static List *pre_load (List *fnames)
{
	char *pstr, *p, *p0;

	if (!(p = config_get_str (gift_conf, "main/plugins")))
		return fnames;

	p0 = pstr = STRDUP (p);

	while ((p = string_sep (&pstr, ":")))
	{
		fnames = list_append (fnames, STRDUP (p));
	}

	free (p0);

	return fnames;
}

static int load_protocols (List *fnames)
{
	Protocol *p;
	List *temp;

	GIFT_DEBUG (("Loading protocols... "));

#if 0
	if (!(p = protocol_new ()))
		GIFT_FATAL (("Couldn't create new protocol, out of memory\n"));

	if (!(p = dc_init (p)))
		GIFT_FATAL (("Couldn't load client-daemon protocol\n"));

	protocols = list_append (protocols, p);
#endif

	/* load each plugin in the list */
	for (temp = fnames; temp; temp = temp->next)
	{
		if (!(p = protocol_load ((char*)(temp->data))))
			GIFT_ERROR (("\nFailed to load protocol: %s\n", (char *)temp->data));

		protocols = list_append (protocols, p);
	}

	if (!fnames)
		GIFT_WARN (("no protocols loaded"));

	return TRUE;
}

static int config (const char *module)
{
	if (!config_get_int (gift_conf, "main/setup=0"))
	{
		gift_setup_msg ();
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

#ifdef GC_LEAK_DETECT
static void report_leak ()
{
	CHECK_LEAKS ();
}
#endif /* GC_LEAK_DETECT */

/* TODO: this needs to cleanup better */
static void shutdown ()
{
	List *temp;

	TRACE_FUNC ();

#ifdef WIN32
	win32_cleanup ();
#endif

	dataset_clear (options);

	/* cleanup protocols */
	for (temp = protocols; temp; temp = temp->next)
		protocol_unload (temp->data);

	protocols = list_free (protocols);

	/* clear shares */
	share_clear_index ();

	/* unload conf */
	config_free (gift_conf);

	exit (0);
}

/*****************************************************************************/
int main (int argc, char **argv)
{
	List *protocol_fnames = NULL;

#ifdef GC_LEAK_DETECT
	GC_find_leak = 1;
#endif

#ifdef WIN32
	win32_init (argc, argv);
#endif

	if (!(gift_conf = gift_config_new ("giFT")))
	{
		gift_setup_msg ();
		return 1;
	}

#ifndef WIN32
	signal (SIGPIPE, SIG_IGN);
#endif
	signal (SIGINT,  shutdown);
#ifdef GC_LEAK_DETECT
	signal (SIGUSR1, report_leak);
#endif

	share_update_index ();

	/* atexit (shutdown); */

	protocol_fnames = parse (argc, argv);
	protocol_fnames = pre_load (protocol_fnames);

	if (!protocol_fnames)
		usage (argv[0], 1);

	if (!(interface_init (INTERFACE_PORT)))
	{
		GIFT_ERROR (("failed to load interface subsystem\n\n"
		             "NOTE: there may be another giFT daemon running on this "
		             "host.  If not, please verify that the socket has been "
		             "successfully removed by using netstat -tn\n"));
		return 1;
	}

	if (!(load_protocols (protocol_fnames)))
		GIFT_FATAL (("load_protocols failed"));

	if (!config ("giFT"))
		GIFT_FATAL (("couldn't parse configuration file"));

	/* before we enter the main event loop, go ahead and re-read all state
	 * files to check for possible resumes to be started */
	download_recover_state ();

	GIFT_DEBUG (("initialization complete"));

	/* main event loop */
	event_loop ();

	shutdown ();

	return 0;
}
