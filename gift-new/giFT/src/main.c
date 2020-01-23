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

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include "gift.h"
#include "hook.h"

#include "plugin.h"
#include "perlc.h"

#include "if_port.h"
#include "event.h"

#include "download.h"
#include "conf.h"
#include "parse.h"
#include "network.h"
#include "protocol.h"

#include "share_cache.h"

#include "file.h"

/*****************************************************************************/

Config         *gift_conf;        /* main configuration file     */
static Dataset *options;          /* command-line parsed options */
static List    *p_fnames  = NULL; /* plugin filenames to load    */

/*****************************************************************************/

static void usage (char *argv0, int exit_val)
{
	fprintf (stderr,
	         "Usage: %s [options] [protocol-lib...]\n"
             "\n"
             "    -h                  Help\n"
	         "    -v                  Version\n"
	         "    -d                  Fork to background\n"
             "<protocol-lib>          The file locations of the protocols to use\n",
             argv0);
	exit (exit_val);
}

static void version ()
{
	/* simplified format (scriptability damnit) - jasta */
	fprintf (stderr, "%s %s\n", PACKAGE, VERSION);
	exit (0);
}

static void gift_setup_msg ()
{
	fprintf (stderr,
	         "\n*** ERROR: Your setup is incomplete ***\n\n"
	         "You will need to copy the contents of the etc/ directory to %s "
	         "and hand edit the files.  This message will repeat until you "
	         "have effectively modified the default configuration.\n\n"
	         "If you are too lazy to do this, you may optionally use our "
	         "configuration script `giFT-setup`.\n\n",
	         gift_conf_path (""));
}

/*****************************************************************************/

static void parse_argv (int argc, char **argv)
{
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
					GIFT_DEBUG (("giFT: %u\n", cpid));
					_exit (0);
				}

				setsid ();

				close (STDIN_FILENO);
				close (STDOUT_FILENO);
				close (STDERR_FILENO);
			}
			break;
		 default:
			usage (argv[0], 1);
			break;
		}
	}

	while (optind < argc)
		p_fnames = list_append (p_fnames, STRDUP (argv[optind++]));
#endif /* !WIN32 */
}

static void build_fnames ()
{
	char *pstr;
	char *p0, *p;
	char  abs_path[PATH_MAX];

	/* TODO -- provide a default? */
	if (!(p = config_get_str (gift_conf, "main/plugins")))
		return;

	/* make sure we're working w/ our own local memory, not conf.c's */
	p0 = pstr = STRDUP (p);

	/* load all plugins supplied */
	while ((p = string_sep (&pstr, ":")))
	{
		/* non-absolute paths need to be constructed */
		if (*p != '/')
		{
			snprintf (abs_path, sizeof (abs_path) - 1, "%s/%s",
			          platform_plugin_dir (), p);

			TRACE (("using %s", abs_path));

			/* this is a stupid idea */
			p = abs_path;
		}

		p_fnames = list_append (p_fnames, STRDUP (p));
	}

	free (p0);
}

static int load_protocols ()
{
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
	for (temp = p_fnames; temp; temp = list_next (temp))
		plugin_load (temp->data);

#ifdef USE_DLOPEN
	if (!p_fnames)
		GIFT_WARN (("no protocols loaded"));
#endif

	return TRUE;
}

static int config (char *module)
{
	gift_conf = gift_config_new (module);

	if (!config_get_int (gift_conf, "main/setup=0"))
		return FALSE;

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
static void gift_shutdown (int signum)
{
	List *ptr;
	List *next;

	TRACE_FUNC ();

	/* close the interface protocol bind */
	if_port_cleanup ();

	/* wipe the options list */
	dataset_clear (options);

	/* cleanup plugin filenames */
	for (ptr = p_fnames; ptr; )
	{
		next = ptr->next;

		free (ptr->data);
		free (ptr);

		ptr = next;
	}

	p_fnames = NULL;

	/* clear shares */
	share_clear_index ();

	/* cleanup protocols */
	for (ptr = protocol_list (); ptr; )
	{
		next = ptr->next;

		plugin_unload (ptr->data);
		protocol_remove (ptr->data);

		ptr = next;
	}

	/* any arbitrary platform specific cleanups that need to occur */
	platform_cleanup ();

	/* unload conf */
	config_free (gift_conf);

#ifdef USE_PERL
	perl_cleanup ();
#endif

	log_cleanup ();

	exit (0);
}

void gift_exit ()
{
	gift_shutdown (2 /* SIGINT */);
}

/*****************************************************************************/

int gift_main (int argc, char **argv)
{
#ifdef GC_LEAK_DETECT
	GC_find_leak = 1;
#endif /* GC_LEAK_DETECT */

	/* log platform failures to stderr */
	log_init (GLOG_STDERR, PACKAGE, 0, 0, NULL);

	/* initialize platform specific stuff */
	platform_init ();

	/* log errors to gift.log... */
	log_init (GLOG_DEFAULT, PACKAGE, 0, 0, gift_conf_path ("gift.log"));

	/* signal handlers */
	signal (SIGINT, event_quit);
#ifdef GC_LEAK_DETECT
	signal (SIGUSR1, report_leak);
#endif /* GC_LEAK_DETECT */

	/* process command line arguments */
	parse_argv (argc, argv);

	/* load configuration and handle sanity checking */
	if (!config ("giFT"))
	{
		gift_setup_msg ();
		return 1;
	}

#ifdef USE_PERL
	perl_init ();
	perl_autoload ();
#endif /* USE_PERL */

	/* create the protocol list */
	build_fnames ();

#ifdef USE_DLOPEN
	/* nothing to do, bitch */
	if (!p_fnames)
	{
		usage (argv[0], 1);
		return 1;
	}
#endif

	/* bind the interface protocol port */
	if (!(if_port_init (GIFT_INTERFACE_PORT)))
	{
		GIFT_FATAL (("failed to load interface subsystem\n\n"
		             "NOTE: there may be another giFT daemon running on this "
		             "host.  If not, please verify that the socket has been "
		             "successfully removed by using netstat -tn\n"));
		return 1;
	}

	/* actually initialize the protocols */
	if (!(load_protocols ()))
		GIFT_FATAL (("load_protocols failed"));

	/* update index for protocols */
	share_update_index ();

	/* before we enter the main event loop, go ahead and re-read all state
	 * files to check for possible resumes to be started */
	download_recover_state ();

	GIFT_DEBUG (("parent initialization complete, child may still be "
	             "building shares"));

	/* main event loop */
	event_loop ();

	gift_shutdown (2); /* SIGINT */

	return 0;
}

#ifndef _WINDOWS

int main (int argc, char **argv)
{
	return gift_main (argc, argv);
}

#else /* _WINDOWS */

 /* _WINDOWS is defined when compiling as a "native" win32 app */

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
                    LPSTR cmd_line, int cmd_show)
{
	return gift_main (0, NULL);
}

#endif /* !_WINDOWS */
