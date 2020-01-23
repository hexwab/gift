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

#ifndef WIN32
#include <unistd.h>
#endif /* !WIN32 */

#include "config.h"

#ifdef HAVE_GETOPT_H
#include "getopt.h"
#endif /* HAVE_GETOPT_H */

#include "gift.h"
#include "hook.h"

#include "plugin.h"
#include "perlc.h"

#include "if_port.h"

/*
 * Allow me to take this opportunity to express how retarded I think the
 * ImageMagick developers are for defining a type called Timer.  Nice job
 * assholes.
 */
#define IMAGE_MAGICK_SUCKS
#include "event.h"

#include "share_file.h"
#include "download.h"
#include "upload.h"
#include "conf.h"
#include "parse.h"
#include "network.h"
#include "protocol.h"

#include "share_cache.h"

#include "file.h"

/*****************************************************************************/

/* for version() */
#ifdef USE_ZLIB
#include <zlib.h>
#endif

#ifdef USE_ID3LIB
#include <id3.h>
#endif

#ifdef USE_IMAGEMAGICK
# include <magick/api.h>
# include <magick/version.h>
#endif

#ifdef USE_LIBDB
# ifdef HAVE_DB4_DB_H
#  include <db4/db.h>
# endif
# ifdef HAVE_DB_H
#  include <db.h>
# endif
# ifdef HAVE_DB3_DB_H
#  include <db3/db.h>
# endif
#endif /* USE_LIBDB */

#ifdef USE_PERL
# include <EXTERN.h>
# ifndef _SEM_SEMUN_UNDEFINED
#  define HAS_UNION_SEMUN
# endif
# undef MIN
# undef MAX
# undef DEBUG
# include <perl.h>
# include <XSUB.h>
#endif /* USE_PERL */

#ifndef USE_DLOPEN
# include "ft_openft.h"
#endif

/*****************************************************************************/

Config         *gift_conf;        /* main configuration file     */
static Dataset *options;          /* command-line parsed options */
static List    *p_fnames  = NULL; /* plugin filenames to load    */

#define INTERFACE_PORT 1213

/*****************************************************************************/

static void message (char *str)
{
#ifndef _WINDOWS
	fprintf (stdout, str);
#else
	MessageBox (0, str, "giFT", MB_OK | MB_ICONINFORMATION);
#endif
}

static void error (char *str)
{
#ifndef _WINDOWS
	fprintf (stderr, str);
#else
	MessageBox (0, str, "giFT Error", MB_OK | MB_ICONERROR);
#endif
}

static void usage (char *argv0, int exit_val)
{
	static char *msg =
		"Usage: %s [options] [protocol-library]...\n"
		"\n"
		"  -d   run as a daemon in the background\n"
		"  -h   display this help message and exit\n"
		"  -v   output version information and exit\n"
		"\n"
		"You may specify initial protocol plugins to load via the "
		"command line or by editing the appropriate configuration files.\n"
		"See etc/ in the source distribution for more information.\n";

	message (stringf (msg, argv0));
	exit (exit_val);
}

static void version ()
{
	String *s;

	if (!(s = string_new (NULL, 0, 0, TRUE)))
	{
		GIFT_ERROR (("unable to allocate version buffer"));
		return;
	}

	string_appendf (s, "%s %s ", PACKAGE, VERSION);
	string_append (s, "(Build " BUILD_DATE ")\n\n");

	string_append (s,
	    "Copyright (c) 2001-2002 giFT project (http://giftproject.org/)\n"
	    "This is free software; see the source for copying conditions.  "
	    "There is NO\nwarranty; not even for MERCHANTABILITY or "
	    "FITNESS FOR A PARTICULAR PURPOSE.\n\n");

	string_append (s, "Compile-time support:\n");
#ifdef USE_LIBDB
	string_appendf (s, "  Berkeley DB %i.%i.%i\n",
	                DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
#endif
#ifdef USE_ID3LIB
#ifdef WIN32 /* fix for broken linker */
#define ID3LIB_NAME "id3lib"
#define ID3LIB_MAJOR_VERSION 3
#define ID3LIB_MINOR_VERSION 8
#define ID3LIB_PATCH_VERSION 0
#endif /* WIN32 */
	string_appendf (s, "  %s %i.%i.%i\n", ID3LIB_NAME, ID3LIB_MAJOR_VERSION,
	                ID3LIB_MINOR_VERSION, ID3LIB_PATCH_VERSION);
#endif /* USE_ID3LIB */
#ifdef USE_IMAGEMAGICK
	string_appendf (s, "  ImageMagick %d.%d.%d\n",
	                (MagickLibVersion >> 8) & 0xf,
	                (MagickLibVersion >> 4) & 0xf,
	                (MagickLibVersion) & 0xf);
#endif
#ifdef USE_LIBVORBIS
	string_appendf (s, "  Ogg Vorbis %i.%i\n", 1, 0);
#endif
#ifdef USE_PERL
	string_appendf (s, "  Perl %i.%i.%i scripting support\n",
	                PERL_REVISION, PERL_VERSION, PERL_SUBVERSION);
#endif
#ifdef USE_ZLIB
	string_appendf (s, "  ZLib " ZLIB_VERSION "\n");
#endif
#ifdef USE_DLOPEN
	string_appendf (s, "  Dynamic library loading (via libdl)\n");
#else
	string_appendf (s, "  OpenFT %i.%i.%i-%i\n", OPENFT_MAJOR, OPENFT_MINOR,
	                OPENFT_MICRO, OPENFT_REV);
#endif
#ifndef WIN32
	string_append (s,
	               "\n"
	               "Compile-time directory paths:\n"
	               "  DATA_DIR=" DATA_DIR "\n"
	               "  PLUGIN_DIR=" PLUGIN_DIR "\n");
#endif /* !WIN32 */

	message (s->str);
	string_free (s);

	exit (0);
}

static void gift_setup_msg ()
{
	static char *msg =
		"\n*** ERROR: Your setup is incomplete ***\n\n"
		"You will need to copy the contents of the etc/ directory to %s "
		"and hand edit the files.  This message will repeat until you "
		"have effectively modified the default configuration.\n\n"
		"If you are too lazy to do this, you may optionally use our "
		"configuration script `giFT-setup`.  If you are still having "
		"troubles with this error message, learn to read.\n\n";

	error (stringf (msg, gift_conf_path ("")));
}

/*****************************************************************************/

static void parse_argv (int argc, char **argv)
{
#ifdef USE_GETOPT
# ifndef WIN32
	pid_t cpid;
# else
	int   dopt = FALSE;
# endif

	/* use getopt to parse the command line */
	while (1)
	{
		int c;

		/* parse the command line */
		if ((c = getopt (argc, argv, "h?vVd")) == -1)
			break;

		switch (c)
		{
		 case 0:
			usage (argv[0], 1);
			break;
		 case 'h': /* help */
		 case '?':
			usage (argv[0], 0);
			break;
		 case 'v': /* version */
		 case 'V':
			version (argv[0]);
			break;
		 case 'd': /* daemon */
# ifndef WIN32
			if ((cpid = fork ()) != 0)
			{
				GIFT_DEBUG (("giFT: %u\n", cpid));
				_exit (0);
			}

			setsid ();

			close (STDIN_FILENO);
			close (STDOUT_FILENO);
			close (STDERR_FILENO);
# else /* WIN32 */
			dopt = TRUE;
# endif /* !WIN32 */
			break;
		 default:
			usage (argv[0], 1);
			break;
		}
	}

# ifdef _WINDOWS
	/* refuse to start if command line parameter is missing */
	if (!dopt)
	{
		error ("Please use giFTtray to start giFT or use the -d option.");
		usage (argv[0], 1);
	}
# endif

	while (optind < argc)
		p_fnames = list_append (p_fnames, STRDUP (argv[optind++]));
#endif /* USE_GETOPT */
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

static void cleanup_transfers (List *list_func (),
                               void stop_func (Transfer *, int))
{
	List *list;

	while ((list = (*list_func) ()) && list->data)
		(*stop_func) (list->data, FALSE);
}

static int unload_protocol (Dataset *d, DatasetNode *node, void *udata)
{
	plugin_unload (node->value);
	return FALSE;
}

/* TODO: this needs to cleanup better */
static void gift_shutdown (int signum)
{
	List *ptr;
	List *next;

	TRACE_FUNC ();

	cleanup_transfers (download_list, download_stop);
	cleanup_transfers (upload_list, upload_stop);

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
	protocol_foreachclear (DATASET_FOREACH (unload_protocol), NULL);

	/* any arbitrary platform specific cleanups that need to occur */
	platform_cleanup ();

	/* unload conf */
	config_free (gift_conf);

#ifdef USE_PERL
	perl_cleanup ();
#endif

	/* one more time before the log system goes */
	TRACE_FUNC ();

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
	signal (SIGINT,  event_quit);
	signal (SIGTERM, event_quit);
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
	if (!(if_port_init (INTERFACE_PORT)))
	{
		static char *msg =
		    "failed to load interface subsystem\n\n"
		    "NOTE: there may be another giFT daemon running on this "
		    "host.  If not, please verify that the socket has been "
		    "successfully removed by using netstat -n"
#ifndef WIN32
			"t"
#endif /* !WIN32 */
			"\n";

		GIFT_FATAL ((msg));
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

void __cdecl _setargv (void);

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
                    LPSTR cmd_line, int cmd_show)
{
	_setargv ();
	return gift_main (__argc, __argv);
}

#endif /* !_WINDOWS */
