/* giFTcurs - curses interface to giFT
 * Copyright (C) 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
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
 * $Id: ui_console.c,v 1.16 2003/11/04 23:40:44 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "settings.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "gift.h"

static list messages = LIST_INITIALIZER;
static int my_screen_nr;
static int build_width = -1;
static gboolean do_wrap = TRUE;

static void console_screen_resize(int new_width);
static void console_screen_update(void);
static void console_screen_loghook(GLogLevelFlags log_level, const char *);

/* Maximum number of entries we keep in the log. */
static int max_entries = 200;
static dynarray log_entries = { 0 };
static const ui_methods console_screen_methods;

#define CONSOLE_HEIGHT (max_y - 3 - show_buttonbar)

#ifdef MOUSE
/* Mouse callback */
static void mouse_consoleclick(int rx, int ry, void *data);
#endif

void console_screen_init(void)
{
	log_add_hook(console_screen_loghook);
	/* DESCRIPTION: This specifies how many lines should be saved
	   in the console backlog. */
	max_entries = atoi(get_config("set", "max-console", "200"));
	/* DESCRIPTION: If this value is non-zero, lines are wrapped in
	   the console. */
	do_wrap = atoi(get_config("set", "wrap-console", "1"));

	my_screen_nr = register_screen(&console_screen_methods);
}

static void console_screen_resize(int width)
{
	int i, start;
	gboolean follow;

	if (width == build_width)
		return;

	start = messages.start;
	follow = start >= messages.num - CONSOLE_HEIGHT;

	list_free_entries(&messages);
	for (i = 0; i < log_entries.num; i++)
		wrap_lines(&messages, list_index(&log_entries, i), width);
	build_width = width;

	messages.start = follow ? messages.num : start;
}

static void console_screen_destroy(void)
{
	list_free_entries(&messages);
	dynarray_removeall(&log_entries);

	log_remove_hook(console_screen_loghook);
}

static void console_screen_draw(void)
{
	clrscr();
	curs_set(0);
	leaveok(stdscr, TRUE);
	draw_box(0, 0, max_x, max_y - 1 - show_buttonbar, _("Console"),
			 COLOR_PAIR(COLOR_CONSOLE_BOX) | A_BOLD);
	mouse_clear(0);
	mouse_register(0, 0, max_x, max_y - 1 - show_buttonbar, BUTTON1_PRESSED,
				   mouse_consoleclick, NULL, 0);
	console_screen_update();
}

static void console_screen_update(void)
{
	console_screen_resize(do_wrap ? max_x - 4 : -1);
	list_check_values_simple(&messages, CONSOLE_HEIGHT);
	/* This list is special in the aspect that it has no selected item.
	 * negative messages.sel tells draw_list to place the diamond right. */
	messages.sel = -1;
	draw_list(2, 1, max_x - 4, CONSOLE_HEIGHT, FALSE, &messages);
	refresh();
}

static int console_screen_handler(int key)
{
	int ret = 1;

	switch (key) {
	case 'V':
	case 'v':
		verbose = !verbose;
		g_message(verbose ? _("Verbose mode turned on.") : _("Verbose mode turned off."));
		break;
	case 'W':
	case 'w':
		do_wrap = !do_wrap;
		g_message(do_wrap ? _("Wrapping lines.") : _("Line wrapping turned off."));
		set_config_int("set", "wrap-console", do_wrap, TRUE);
		console_screen_update();
		break;
	default:
		messages.sel = messages.start;
		ret = ui_list_handler(&messages, key, max_y - 3 - show_buttonbar);
		messages.start = messages.sel;
		console_screen_update();
		break;
	}

	return ret;
}

static void console_screen_append(const char *msg)
{
	gboolean follow = messages.start >= messages.num - CONSOLE_HEIGHT;

	/* FIXME: evil code follows. If the user wraps lines, max_entries wont
	   be upheld really. This is because "messages" had more than one line
	   per log line. */
	/* Make sure we don't have more than max_entries log entries. */
	if (log_entries.num >= max_entries) {
		while (log_entries.num >= max_entries) {
			g_free(list_index(&log_entries, 0));
			dynarray_remove_index(&log_entries, 0);
		}
		/* This is the hack basically, so that we don't increase our memory
		   usage until the OOM gets us. It is good enough for now. */
		while (messages.num >= max_entries) {
			g_free(list_index(&messages, 0));
			list_remove_entry(&messages, 0);
		}
	}
	dynarray_append(&log_entries, g_strdup(msg));

	wrap_lines(&messages, msg, build_width);

	/* Scroll the console automatically if the last entry was selected. */
	if (follow)
		messages.start = messages.num;

	/* Update the console if we're on that screen */
	ui_update(my_screen_nr);
}

static void console_screen_loghook(GLogLevelFlags log_level, const char *msg)
{
	if ((log_level & G_LOG_LEVEL_DEBUG) && !verbose)
		return;

	if (log_level & G_LOG_LEVEL_INFO)
		ui_hilight_button(my_screen_nr);

	console_screen_append(msg);
}

#ifdef MOUSE
static void mouse_consoleclick(int rx, int ry, void *data)
{
	g_assert(active_screen == my_screen_nr);

	if (ry < max_y / 2)
		messages.start -= CONSOLE_HEIGHT / 2;
	else
		messages.start += CONSOLE_HEIGHT / 2;
	console_screen_update();
}
#endif

static const ui_methods console_screen_methods = {
	console_screen_draw,
	NULL,						/* hide */
	(void *) console_screen_update,
	console_screen_handler,
	console_screen_destroy,
	N_("Console"),
};
