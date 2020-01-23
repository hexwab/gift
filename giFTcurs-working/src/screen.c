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
 * $Id: screen.c,v 1.78 2002/10/22 13:18:10 weinholt Exp $
 */
#include "giftcurs.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>

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

#if CAN_RESIZE
static void adjust(int sig);
#endif

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "settings.h"
#include "ui_draw.h"
#include "poll.h"
#include "ui.h"

int max_x = -1, max_y = -1;
int INVERT_SEL;

/* If you change/extend this list, reflect on the enum in screen.h */
/* *INDENT-OFF* */
static struct {
	char *color;
	char *name;
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
	{"hit-good", N_("good hits"), COLOR_GREEN, COLOR_BLACK},
	{"hit-bad", N_("bad hits"), COLOR_RED, COLOR_BLACK},
	{"progress", N_("progress bar"), COLOR_BLUE, COLOR_GREEN},
	{"tot-progress", N_("total progress"), COLOR_BLUE, COLOR_GREEN},
	{"diamond", N_("scroll indicator"), COLOR_GREEN, COLOR_BLACK},
	{"selection", N_("selection"), COLOR_YELLOW, COLOR_BLUE},
	{"buttonbar", N_("button bar"), COLOR_BLACK, COLOR_GREEN},
	{"buttonbar-sel", N_("button bar selection"), COLOR_BLACK, COLOR_CYAN},
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

#ifdef NCURSES_VERSION
static void define_keys(void)
{
	char buf[10];
	int i;

	/* Define the meta-n keys */
	define_key("\0330", KEY_F(10));
	for (i = 1; i <= 9; i++) {
		snprintf(buf, sizeof buf, "\033%i", i);
		define_key(buf, KEY_F(i));
	}

	define_key("\033OH", KEY_HOME);
	define_key("\033OF", KEY_END);
	define_key("\n", KEY_ENTER);
	define_key("\r", KEY_ENTER);
}
#endif

/* is_xterm is stolen from links */
static int is_xterm(void)
{
#if defined(__EMX__)			/* OS2 */
	return !!getenv("WINDOWID");
#elif defined(__BEOS__)
	return 0;
#elif defined(__riscos__)
	return 1;
#else
	return getenv("DISPLAY") && *getenv("DISPLAY");
#endif
}

int get_item_number(char *color)
{
	int i;

	for (i = 0; i < buflen(items); i++) {
		if (!strcmp(items[i].color, color))
			return i;
	}
	return -1;
}

void screen_init(void)
{
#if CAN_RESIZE
	/* Do the SIGWINCH hack. */
	signal(SIGWINCH, adjust);
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

#ifdef NCURSES_VERSION
	define_keys();
#endif

	load_color_config();

	if (is_xterm()) {
		/* Set xterm title and icon */
		printf("\033]1;%s\a", PACKAGE);
		printf("\033]2;%s %s\a", PACKAGE, VERSION);
		fflush(stdout);
	}
	INVERT_SEL = atoi(get_config("set", "invert-selection", "0"));
}

void screen_deinit(void)
{
	max_x = max_y = 0;
	mouse_deinit();
	endwin();
}

void set_default_colors(void)
{
	int i;

	for (i = 0; i < ITEMS_NUM; i++) {
		char foo[256];

		init_pair(i + 1, items[i].fg, items[i].bg);
		snprintf(foo, sizeof foo, "%s %s", colornames[items[i].fg], colornames[items[i].bg]);
		set_config("color", items[i].color, foo, 0);
	}
}

void load_color_config(void)
{
	int i;

	for (i = 0; i < ITEMS_NUM; i++) {
		char *foo, *p;
		char standard[256];
		short fg, bg;

		snprintf(standard, sizeof standard, "%s %s", colornames[items[i].fg],
				 colornames[items[i].bg]);
		foo = get_config("color", items[i].color, standard);

		if (!(p = strtok(foo, " ")))
			continue;
		if ((fg = lookup(p, colornames, buflen(colornames))) < 0)
			continue;

		if (!(p = strtok(NULL, " ")))
			continue;
		if ((bg = lookup(p, colornames, buflen(colornames))) < 0)
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
		snprintf(foo, sizeof foo, "%s %s", colornames[fg], colornames[bg]);
		set_config("color", items[i].color, foo, 1);
	}

	return save_configuration();
}

char *item_name(int i)
{
	return _(items[i].name);
}

#ifdef __linux__
# include <unistd.h>
# include <sys/ioctl.h>

int shift_pressed(void)
{
	char modifiers = 6;

	if (ioctl(STDIN_FILENO, TIOCLINUX, &modifiers) < 0)
		return 0;
	return modifiers & 1;
}
#endif


void do_log(int important, const char *fmt, ...)
{
	char *p;
	va_list ap;

	if (!verbose && !important)
		return;

	va_start(ap, fmt);
	if (vasprintf(&p, fmt, ap) < 0)
		return;

	if (important & 2) {
		/* more info is available in errno */
		char *tmp = NULL;

		asprintf(&tmp, "%s: %s (%d)", p, strerror(errno), errno);
		free(p);
		if (!tmp)
			return;
		p = tmp;
	}

	/* max_x == -1 may not be good.. */
	if (verbose || ((important & 5) && max_x == -1))
		fprintf(stderr, "%s\n", p);
	if ((important & 5) && max_x != -1) {
		show_status(p);
		refresh();
	}
	free(p);
	if (important & 4)
		/* die. */
		exit(1);
}

#if CAN_RESIZE
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
static void adjust(int sig)
{
	if (sig == 0) {
		struct winsize size;

		if (ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0) {
			resizeterm(size.ws_row, size.ws_col);
			getmaxyx(stdscr, max_y, max_x);
			wrefresh(curscr);	/* Linux needs this */
			ui_draw();
			refresh();
		}
		ugly_hack = NULL;
	} else {
		ugly_hack = adjust;
	}
	signal(SIGWINCH, adjust);	/* some systems need this */
}
#endif							/* CAN_RESIZE */
