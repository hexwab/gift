/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * $Id: screen.c,v 1.114 2003/11/04 23:40:44 weinholt Exp $
 */
#include "giftcurs.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if HAVE_TERMIOS_H
# include <termios.h>
#else
# include <sgtty.h>
#endif

#if !defined(sun) || !HAVE_TERMIOS_H
# if HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif
#endif

#if defined(SIGWINCH) && defined(TIOCGWINSZ) && defined(HAVE_RESIZETERM)
# define CAN_RESIZE 1
#else
# define CAN_RESIZE 0
#endif

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "settings.h"

static void do_log(const char *log_domain, GLogLevelFlags log_level, const char *message,
				   gpointer user_data);
static void load_color_config(void);
static void screen_deinit(void);

int max_x = -1, max_y = -1;
int INVERT_SEL;

/* If you change/extend this list, reflect on the enum in screen.h */
/* *INDENT-OFF* */
static const struct {
	const char *color;
	const char *name;
	short fg, bg;
} items[] = {
	{"standard", N_("standard"), COLOR_WHITE, COLOR_BLACK},
	{"header", N_("header"), COLOR_WHITE, COLOR_BLACK},
	{"search-box", N_("search box"), COLOR_RED, COLOR_BLACK},
	{"result-box", N_("result box"), COLOR_GREEN, COLOR_BLACK},
	{"stat-box", N_("statistics box"), COLOR_BLUE, COLOR_BLACK},
	{"stat-data", N_("statistics data"), COLOR_WHITE, COLOR_BLACK},
	{"stat-bad", N_("bad statistics"), COLOR_RED, COLOR_BLACK},
	{"info-box", N_("info box"), COLOR_MAGENTA, COLOR_BLACK},
	{"download-box", N_("download box"), COLOR_GREEN, COLOR_BLACK},
	{"upload-box", N_("upload box"), COLOR_CYAN, COLOR_BLACK},
	{"help-box", N_("help box"), COLOR_BLUE, COLOR_BLACK},
	{"console-box", N_("console box"), COLOR_BLUE, COLOR_BLACK},
	{"hit-good", N_("good hits"), COLOR_GREEN, COLOR_BLACK},
	{"hit-bad", N_("bad hits"), COLOR_RED, COLOR_BLACK},
	{"progress", N_("progress bar"), COLOR_BLUE, COLOR_GREEN},
	{"tot-progress", N_("total progress"), COLOR_BLUE, COLOR_GREEN},
	{"diamond", N_("scroll indicator"), COLOR_GREEN, COLOR_BLACK},
	{"selection", N_("selection"), COLOR_YELLOW, COLOR_BLUE},
	{"buttonbar", N_("button bar"), COLOR_BLACK, COLOR_GREEN},
	{"buttonbar-sel", N_("button bar selection"), COLOR_BLACK, COLOR_CYAN},
	{"buttonbar-alert", N_("button bar alert"), COLOR_BLACK, COLOR_RED},
};
/* *INDENT-ON* */

const char *colornames[] = {
	N_("black"),
	N_("red"),
	N_("green"),
	N_("yellow"),
	N_("blue"),
	N_("magenta"),
	N_("cyan"),
	N_("white"),
#ifdef HAVE_USE_DEFAULT_COLORS
	N_("default"),
#endif
};

/* Make the line drawing characters into 7-bit chars */
void remap_linechars(void)
{
#ifndef __CYGWIN__
	ACS_ULCORNER = ',';
	ACS_URCORNER = '.';
	ACS_LLCORNER = '`';
	ACS_LRCORNER = '\'';
	ACS_HLINE = '-';
	ACS_VLINE = '|';
	ACS_RTEE = '(';
	ACS_LTEE = ')';
	ACS_DIAMOND = '*';
#endif
}

static void define_keys(void)
{
#if defined(NCURSES_VERSION) && !defined(NCURSES_970530)
	char buf[] = "\0330";
	int i;

	/* Define the meta-n keys */
	define_key(buf, KEY_F(10));
	for (i = 1; i <= 9; i++) {
		buf[1] = '0' + i;
		define_key(buf, KEY_F(i));
	}

	/* Old ncurses versions have wrong type on define_key function header */
	/* You can safely ignore warnings about 'discards qualifiers' */
	define_key("\033OH", KEY_HOME);
	define_key("\033OF", KEY_END);
	define_key("\n", KEY_ENTER);
	define_key("\r", KEY_ENTER);
#endif
}

/* is_xterm is stolen from links */
static gboolean is_xterm(void)
{
#if defined(__EMX__)			/* OS2 */
	return !!g_getenv("WINDOWID");
#elif defined(__BEOS__)
	return FALSE;
#elif defined(__riscos__)
	return TRUE;
#else
	return g_getenv("DISPLAY") && *g_getenv("DISPLAY");
#endif
}

int get_item_number(char *color)
{
	int i;

	for (i = 0; i < G_N_ELEMENTS(items); i++) {
		if (!strcmp(items[i].color, color))
			return i;
	}
	return -1;
}

#if CAN_RESIZE
# include <unistd.h>

static gboolean sigwinch_received = FALSE;
static void sigwinch_handler(int signum G_GNUC_UNUSED)
{
	sigwinch_received = TRUE;
}
static gboolean sigwinch_test(void)
{
	return sigwinch_received;
}

/* Stolen from ncurses' test/view.c, slightly modified: */
/*
 * This uses functions that are "unsafe", but it seems to work on SunOS, GNU
 * and Linux.  Usually:  the "unsafe" refers to the functions that POSIX lists
 * which may be called from a signal handler.  Those do not include buffered
 * I/O, which is used for instance in wrefresh().  To be really portable, we
 * should use the KEY_RESIZE return (which relies on ncurses' sigwinch
 * handler). But that can't be done since we use poll().
 *
 * The 'wrefresh(curscr)' is needed to force the refresh to start from the top
 * of the screen -- some xterms mangle the bitmap while resizing.
 */
static gboolean sigwinch_dispatch(void)
{
	struct winsize size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
# if HAVE_RESIZE_TERM
		resize_term(size.ws_row, size.ws_col);
#  else
		resizeterm(size.ws_row, size.ws_col);
# endif
		getmaxyx(stdscr, max_y, max_x);
		wrefresh(curscr);		/* Linux needs this */
		ui_draw();
		refresh();
	}
	sigwinch_received = FALSE;
	signal(SIGWINCH, sigwinch_handler);
	return TRUE;
}
static GSourceFuncs sigwinch_funcs = {
	(void *) sigwinch_test,
	(void *) sigwinch_test,
	(void *) sigwinch_dispatch,
};
#endif

void screen_init(void)
{
#if CAN_RESIZE
	/* Do the SIGWINCH hack. */
	GSource *source = g_source_new(&sigwinch_funcs, sizeof(GSource));

	g_source_attach(source, NULL);
	signal(SIGWINCH, sigwinch_handler);
#endif

	initscr();
	cbreak();
	noecho();

	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	if (has_colors()) {
		start_color();
#ifdef HAVE_USE_DEFAULT_COLORS
		use_default_colors();
#endif
	}
	mouse_init();

	getmaxyx(stdscr, max_y, max_x);

	define_keys();

	load_color_config();

	if (is_xterm()) {
		/* Set xterm title and icon */
		printf("\033]1;%s\a", PACKAGE);
		printf("\033]2;%s %s\a", PACKAGE, VERSION);
		fflush(stdout);
	}
	INVERT_SEL = atoi(get_config("set", "invert-selection", "0"));

	g_atexit(screen_deinit);
}

static void screen_deinit(void)
{
	if (max_x != -1) {
		max_x = max_y = -1;
		mouse_deinit();
		endwin();
	}
}

void set_default_colors(void)
{
	int i;

	for (i = 0; i < ITEMS_NUM; i++) {
		char foo[256];

		init_pair(i + 1, items[i].fg, items[i].bg);
		g_snprintf(foo, sizeof foo, "%s %s", colornames[items[i].fg], colornames[items[i].bg]);
		set_config("color", items[i].color, foo, 0);
	}
}

static void load_color_config(void)
{
	int i;

	for (i = 0; i < ITEMS_NUM; i++) {
		const char *foo;
		char standard[256], *p;
		short fg, bg;

		g_snprintf(standard, sizeof standard, "%s %s", colornames[items[i].fg],
				   colornames[items[i].bg]);
		foo = get_config("color", items[i].color, standard);
		g_strlcpy(standard, foo, sizeof standard);

		if (!(p = strtok(standard, " ")))
			continue;
		if ((fg = lookup(p, colornames, G_N_ELEMENTS(colornames))) < 0)
			continue;

		if (!(p = strtok(NULL, " ")))
			continue;
		if ((bg = lookup(p, colornames, G_N_ELEMENTS(colornames))) < 0)
			continue;

		init_pair(i + 1, fg, bg);
	}
	return;
}

int save_color_config(void)
{
	int i;

	for (i = 0; i < ITEMS_NUM; i++) {
		short fg, bg;
		char foo[256];

		pair_content(i + 1, &fg, &bg);
		g_snprintf(foo, sizeof foo, "%s %s", colornames[fg], colornames[bg]);
		set_config("color", items[i].color, foo, 1);
	}

	return save_configuration();
}

const char *item_name(int i)
{
	return _(items[i].name);
}

#ifdef __linux__
# include <unistd.h>
# include <sys/ioctl.h>

gboolean shift_pressed(void)
{
	static gboolean is_console = TRUE;

	if (is_console) {
		char modifiers = 6;

		if (ioctl(STDIN_FILENO, TIOCLINUX, &modifiers) == 0)
			return modifiers & 1;
		is_console = FALSE;
	}
	return FALSE;
}
#endif

struct log_data {
	GLogLevelFlags log_level;
	char *message;
};

static GSList *log_hooks = NULL;

static void log_marshal(LogFunc func, struct log_data *nisse)
{
	func(nisse->log_level, nisse->message);
}

void log_init(void)
{
	g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, do_log, NULL);
}

void log_add_hook(LogFunc func)
{
	log_hooks = g_slist_prepend(log_hooks, func);
}

void log_remove_hook(LogFunc func)
{
	log_hooks = g_slist_remove(log_hooks, func);
}

static void do_log(const char *log_domain, GLogLevelFlags log_level, const char *message,
				   gpointer user_data)
{
	struct log_data data;

	if (!verbose && (log_level & G_LOG_LEVEL_DEBUG))
		return;

	/* Strip eventual trailing newline */
	data.message = g_strdup(message);
	g_strchomp(data.message);

	data.log_level = log_level;
	g_slist_foreach(log_hooks, (GFunc) log_marshal, &data);

	g_free(data.message);
	if (log_level & G_LOG_LEVEL_CRITICAL)
		/* die. */
		exit(1);
}

#ifdef HAVE_USE_DEFAULT_COLORS
# undef init_pair
# undef pair_content
/* translate default color from and to 255 <-> 8 */
int my_init_pair(short pair, short fg, short bg)
{
	return init_pair(pair, fg == 8 ? -1 : fg, bg == 8 ? -1 : bg);
}

int my_pair_content(short pair, short *fgp, short *bgp)
{
	if (has_colors()) {
		if (pair_content(pair, fgp, bgp) == ERR)
			return ERR;
		if (*(gint8 *) fgp == -1)
			*fgp = 8;
		if (*(gint8 *) bgp == -1)
			*bgp = 8;
	} else {
		*fgp = COLOR_WHITE;
		*bgp = COLOR_BLACK;
	}
	return OK;
}
#endif
