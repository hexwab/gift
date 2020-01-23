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

#include "plugin.h"

#include "if_port.h"
#include "event.h"

#include "download.h"
#include "conf.h"
#include "parse.h"
#include "network.h"
#include "protocol.h"

#include "sharing.h"

#include "file.h"

/*****************************************************************************/

#define INTERFACE_PORT 1213

Config         *gift_conf;        /* main configuration file     */
static Dataset *options;          /* command-line parsed options */
static List    *p_fnames  = NULL; /* plugin filenames to load    */

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
	printf ("%s %s\n", PACKAGE, VERSION);
	exit (0);
}

static void gift_setup_msg ()
{
	fprintf (stderr, "\n");
	fprintf (stderr, "*** Error: your setup is incomplete.  You are expected "
			 "to either run giFT-setup or manually review the "
	         "configuration switches in %s\n",
	         gift_conf_path ("gift.conf"));
	fprintf (stderr, "\n");
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
					printf ("giFT: %u\n", cpid);
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
	while ((p = string_sep (&pstr, PATH_SEP_STR)))
	{
<<<<<<< main.c
#if 0
#ifndef WIN32
=======
>>>>>>> 1.113
		/* non-absolute paths need to be constructed */
		if (*p != '/')
		{
			snprintf (abs_path, sizeof (abs_path) - 1, "%s/%s",
			          platform_plugin_dir (), p);

			TRACE (("using %s", abs_path));

			/* this is a stupid idea */
			p = abs_path;
		}
<<<<<<< main.c
#else /* WIN32 */
		/* TODO -- this code being OS specific is moronic.  Windows needs to
		 * come up to par here with defining these variables ASAP */
#endif /* !WIN32 */
#endif
=======

>>>>>>> 1.113
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

	exit (0);
}

void gift_exit ()
{
	gift_shutdown (2 /* SIGINT */);
}

/*****************************************************************************/

static int child_update_index (SubprocessData *sdata)
{
	static char *eof = "EOF";
	static unsigned int eof_length = 4;

	share_write_index ();

	/* write anything...call parent_update_index ;) */
	if (sdata)
	{
		net_send (sdata->fd, eof, eof_length);
		free (sdata);
	}

	return TRUE;
}

static int parent_update_index (int read_fd, void *udata)
{
	share_read_index ();

	return FALSE;
}

static void update_index ()
{
	if (!(platform_child_proc ((ChildFunc) child_update_index,
							   (ParentFunc) parent_update_index, NULL)))
		TRACE (("UNABLE TO FORK!"));
}

/*****************************************************************************/

static int gift_main (int argc, char **argv)
{
#ifdef GC_LEAK_DETECT
	GC_find_leak = 1;
#endif /* GC_LEAK_DETECT */

	/* initialize platform specific stuff */
	platform_init (argv[0]);

	/* signal handlers */
	signal (SIGINT,  gift_shutdown);
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
	if (!(if_port_init (INTERFACE_PORT)))
	{
		GIFT_ERROR (("failed to load interface subsystem\n\n"
		             "NOTE: there may be another giFT daemon running on this "
		             "host.  If not, please verify that the socket has been "
		             "successfully removed by using netstat -tn\n"));
		return 1;
	}

	/* actually initialize the protocols */
	if (!(load_protocols ()))
		GIFT_FATAL (("load_protocols failed"));

	/* update index for protocols */
	update_index ();

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

int main (int argc, char **argv)
{
	int ret;

	ret = gift_main (argc, argv);

	return ret;
}
