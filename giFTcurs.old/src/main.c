/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: main.c,v 1.126 2003/05/14 09:09:34 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#if ENABLE_NLS
# include <locale.h>
#endif

#include "screen.h"
#include "ui.h"
#include "gift.h"
#include "settings.h"
#include "format.h"

static GMainLoop *loop;
static GPollFunc poll_func = NULL;
const char *server_host = "localhost";
const char *server_port = "1213";
const char *profile_name = NULL;
int verbose = 0;
static gboolean do_remap_linechars = 0;

static void usage(int status) G_GNUC_NORETURN;
static int decode_switches(int argc, char **argv);
static void load_uiconfig(void);

gboolean stdin_handler(GIOChannel * source, GIOCondition condition, gpointer data)
{
	/* Stuff from stdin */
	ui_handler(getch());
	return TRUE;
}

void graceful_death()
{
	g_main_loop_quit(loop);
}

/* We have our own wrapper to catch SIGWINCH. */
static gint poll_wrapper(GPollFD * ufds, guint nfsd, gint timeout)
{
	/* got_sigwinch is set by the SIGWINCH handler in screen.c */
	if (got_sigwinch)
		got_sigwinch(0);

	return poll_func(ufds, nfsd, timeout);
}

int main(int argc, char *argv[])
{
#if ENABLE_NLS
	/* Initialize gettext */
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	/* Initialize error reporting */
	g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
					  do_log, NULL);

	/* Parse arguments */
	g_set_prgname(g_path_get_basename(argv[0]));
	profile_name = g_get_user_name();
	argc -= decode_switches(argc, argv);

	/* Initialize the main event loop */
	loop = g_main_loop_new(NULL, TRUE);
	poll_func = g_main_context_get_poll_func(NULL);
	g_main_context_set_poll_func(NULL, poll_wrapper);

	/* Parse configuration file(s) */
	load_uiconfig();
	load_configuration();

	/* Delay usage until now to be able to print the default server:port */
	if (argc)
		usage(EXIT_FAILURE);

	/* Ignore bugus signal */
	signal(SIGPIPE, SIG_IGN);

	/* Initalize giFT interface */
	gift_init();

	/* Setup ncurses */
	screen_init();
	if (do_remap_linechars)
		remap_linechars();

	ui_init();
	ui_draw();
	g_message(_("%s v%s - This is Free Software. Press F1 or M-1 for more info."), PACKAGE,
			  VERSION);

	g_io_add_watch_full(g_io_channel_unix_new(STDIN_FILENO), G_PRIORITY_HIGH,
						G_IO_IN, stdin_handler, NULL, NULL);

	g_main_loop_run(loop);

	ui_destroy();
	gift_cleanup();
	save_configuration();
	ui_show_messages();
	format_clear();
	config_cleanup();

	DEBUG("Life support systems disconnected.");
	return 0;
}

static void load_uiconfig(void)
{
	char *opt = read_gift_config("ui.conf", "[daemon]", "port");

	if (opt)
		server_port = opt;

	if ((opt = read_gift_config("ui.conf", "[daemon]", "host")))
		server_host = opt;
}

static int decode_switches(int argc, char **argv)
{
	int c;

#ifdef MOUSE
	const char *optstring = "hVvdas:f:p:P:";
#else
	const char *optstring = "hVvas:f:p:P:";
#endif

#if HAVE_GETOPT_LONG
	static struct option const long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'V'},
# ifdef MOUSE
		{"nomouse", no_argument, 0, 'd'},
# endif
		{"stickchars", no_argument, 0, 'a'},
		{"server", required_argument, 0, 's'},
		{"profile", required_argument, 0, 'p'},
		{"verbose", no_argument, 0, 'v'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
#else
	while ((c = getopt(argc, argv, optstring)) != -1) {
#endif
		char *colon;

		switch (c) {
		case 'V':
			printf(_("%s version %s, compiled %s %s\n"), PACKAGE, VERSION, __DATE__, __TIME__);
			exit(0);
		case 'h':
			usage(0);
#ifdef MOUSE
		case 'd':
			use_mouse = FALSE;
			continue;
#endif
		case 'a':
			do_remap_linechars = TRUE;
			continue;
		case 's':
			colon = strrchr(optarg, ':');
			if (colon) {
				*colon = '\0';
				server_port = colon + 1;
			}
			if (*optarg)
				server_host = optarg;
			continue;
		case 'p':
			profile_name = optarg;
			continue;
		case 'v':
			verbose++;
			continue;
		}
		usage(EXIT_FAILURE);
	}
	return optind;
}

static void usage(int status)
{
	printf(_("%s - curses frontend to giFT\n"), g_get_prgname());
	printf(_("Usage: %s [OPTION]...\n"), g_get_prgname());
	printf(_("Options:\n"
			 "  -a, --stickchars        use 7-bit chars for line drawing\n"
			 "  -h, --help              display this help and exit\n"
			 "  -V, --version           output version information and exit\n"
			 "  -v, --verbose           flood stderr with debugging info\n"
			 "  -s, --server=host:port  set server address [%s:%s]\n"
			 "  -p, --profile=name      select what profile to use [%s]\n")
		   , server_host, server_port, profile_name);
#ifdef MOUSE
	printf(_("  -d, --nomouse           don't use the mouse\n"));
#endif
	exit(status);
}
