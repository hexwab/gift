/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: main.c,v 1.110 2002/11/28 19:52:23 chnix Exp $
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
#include "poll.h"
#include "settings.h"
#include "format.h"

static char *program_name;
char *server_host = "localhost";
char *server_port = "1213";
char *profile_name = NULL;
char *download_dir = NULL;
int verbose = 0;
static int do_remap_linechars = 0;

static void usage(int status) __attribute__ ((noreturn));
static int decode_switches(int argc, char **argv);
static void load_uiconfig(void);
static void load_dirconfig(void);

static void stdin_handler(int fd)
{
	/* Stuff from stdin */
	ui_handler(getch());
}

void graceful_death()
{
	poll_del_fd(STDIN_FILENO);
	ui_destroy();
	gift_cleanup();
	save_configuration();
	/* this warns if this is not a clean exit */
	timer_destroy();
	ui_show_messages();
	format_clear();
	config_cleanup();
}

int main(int argc, char *argv[])
{
#if ENABLE_NLS
	/* Initialize gettext */
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	/* Start timer subsystem */
	timer_init();

	/* Parse configuration file(s) */
	load_uiconfig();
	load_configuration();

	/* Parse arguments */
	program_name = argv[0];
	if (!(profile_name = getenv("LOGNAME")))
		profile_name = getenv("USER");
	if (decode_switches(argc, argv) != argc)
		usage(EXIT_FAILURE);

	signal(SIGPIPE, SIG_IGN);

	/* Initalize giFT interface */
	gift_init();

	/* Setup ncurses */
	screen_init();
	atexit(screen_deinit);
	if (do_remap_linechars)
		remap_linechars();

	ui_init();
	ui_draw();
	message(_("%s v%s - This is Free Software. Press F1 or M-1 for more info."), PACKAGE, VERSION);

	poll_add_fd(STDIN_FILENO, stdin_handler);

	poll_forever();

	return 0;
}

static void load_uiconfig(void)
{
	char *file = "ui/ui.conf";
	char *opt = read_gift_config(file, "[daemon]", "port");

	if (opt)
		server_port = opt;

	if ((opt = read_gift_config(file, "[daemon]", "host")))
		server_host = opt;
}

static void load_dirconfig(void)
{
	char *opt = read_gift_config("gift.conf", "[download]", "completed");

	if (opt) {
		download_dir = opt;
		DEBUG("got download dir from gift.conf: %s", download_dir);
	}
}

static int decode_switches(int argc, char **argv)
{
	int c;

#ifdef MOUSE
	char *optstring = "hVvdas:f:p:P:";
#else
	char *optstring = "hVvas:f:p:P:";
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
			use_mouse = 0;
			continue;
#endif
		case 'a':
			do_remap_linechars = 1;
			continue;
		case 's':
			colon = strchr(optarg, ':');
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
	/* If server_host is localhost, read download_dir from gift.conf */
	/* This seems like an ugly hack, and it really is :) */
	if (!strcmp(server_host, "localhost") || !strcmp(server_host, "127.0.0.1")
		|| server_host[0] == '/')
		load_dirconfig();
	return optind;
}

static void usage(int status)
{
	printf(_("%s - curses frontend to giFT\n"), basename(program_name));
	printf(_("Usage: %s [OPTION]...\n"), basename(program_name));
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
