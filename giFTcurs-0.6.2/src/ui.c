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
 * $Id: ui.c,v 1.117 2003/11/16 16:15:36 weinholt Exp $
 */
#include "giftcurs.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "settings.h"
#include "misc.h"
#include "gift.h"				/* FIXME */
#include "wcwidth.h"

static void ui_status_loghook(GLogLevelFlags log_level, const char *);
static void ui_draw_buttonbar(void);

/* FIXME: This should be a private variable */
#ifndef NDEBUG
int active_screen;				/* 0-based, so that F3 is screen 2 */
#else
static int active_screen;
#endif

static int nr_screens = 0;
int curs_x = 0, curs_y = 0;
gboolean show_buttonbar = 0;

/* These variables tells if an update is scheduled */
static gint pending_tag = 0;
static gboolean pending_timer = 0;
static int pending_flags;

static struct {
	const ui_methods *methods;
	unsigned hilight:1;			/* If TRUE, button should be highlighted. */
} screens[9];

/* Screen initializers. XXX Can we use __attribute__((constructor)) instead? */
void help_screen_init(void);
void main_screen_init(void);
void transfer_screen_init(void);
void console_screen_init(void);
void settings_screen_init(void);

void ui_init(void)
{
	show_buttonbar = atoi(get_config("set", "buttonbar", "1"));

	log_add_hook(ui_status_loghook);

	help_screen_init();
	main_screen_init();
	transfer_screen_init();
	console_screen_init();
	settings_screen_init();

	list_set_scrolloff(atoi(get_config("set", "scrolloff", "0")));

	g_assert(nr_screens >= 2);
	active_screen = 1;			/* start with the search screen */
}

void ui_destroy(void)
{
	int i;

	for (i = 0; i < nr_screens; i++)
		screens[i].methods->destroy();
	mouse_deinit();
}

static void ui_status_loghook(GLogLevelFlags log_level, const char *msg)
{
	if (!(log_level & G_LOG_LEVEL_DEBUG) && !(log_level & G_LOG_LEVEL_INFO)) {
		show_status(msg);
		refresh();
	}
}

int register_screen(const ui_methods *methods)
{
	g_assert(nr_screens < G_N_ELEMENTS(screens));
	screens[nr_screens].methods = methods;
	return nr_screens++;
}

static void new_screen(int new_screen)
{
	if (active_screen != new_screen) {
		/* cancel pending updates of the previous screen */
		if (pending_tag) {
			g_source_remove(pending_tag);
			pending_tag = 0;
		}
		if (screens[active_screen].methods->hide)
			screens[active_screen].methods->hide();
		active_screen = new_screen;
		screens[active_screen].methods->draw();
		screens[active_screen].hilight = FALSE;
		clrscr();
		ui_draw();
		refresh();
	}
}

#ifdef MOUSE
/* TODO: don't hardcode the numbers */
void mouse_handler(MEVENT event)
{
	if (!mouse_check(&event))
		if (event.bstate & BUTTON2_PRESSED)
			new_screen(active_screen == 1 ? 2 : 1);
}
#endif

void ui_handler(int key)
{
	if (key == '\t' && shift_pressed())
		key = KEY_BTAB;

	switch (key) {
#ifdef MOUSE
	case KEY_MOUSE_DRAG:
	case KEY_MOUSE:
		if (use_mouse)
			handle_key_mouse(key);
		return;
#endif
	case KEY_F(1)...KEY_F(9):
		if (key <= KEY_F(nr_screens))
			new_screen(key - KEY_F(1));
		return;
#ifdef KEY_RESIZE
	case KEY_RESIZE:
		clrscr();
		wrefresh(curscr);
		getmaxyx(stdscr, max_y, max_x);
		ui_draw();
		refresh();
		return;
#endif
	case ERR:
	case KEY_F(10):
		graceful_death();
		return;
	case XCTRL('l'):
	case XCTRL('r'):
		wrefresh(curscr);
		return;
	}

	/* pass through to the handler for the active screen */
	if (screens[active_screen].methods->key_handler(key))
		return;

	/* check for keys not catched by the active screen */
	switch (key) {
	case 'q':
	case 'Q':
		graceful_death();
		return;
	}
}

/* Simply highlights a button. */
void ui_hilight_button(int button)
{
	if (active_screen == button)
		return;

	screens[button].hilight = TRUE;
	ui_draw_buttonbar();
	refresh();
}

static void ui_draw_buttonbar(void)
{
	int i, y, x;

	if (!show_buttonbar)
		return;

	mouse_clear(1);
	getyx(stdscr, y, x);
	move(max_y - 1, 0);
	for (i = 0; i <= nr_screens; i++) {
		int x1, x2, slack, fkey, color;
		const char *label;

		if (i < nr_screens) {
			label = screens[i].methods->name;
			fkey = i + 1;
			if (i == active_screen)
				color = COLOR_BUTTON_BAR_SEL;
			else if (screens[i].hilight)
				color = COLOR_BUTTON_BAR_ALERT;
			else
				color = COLOR_BUTTON_BAR;
		} else {
			label = N_("Exit");
			fkey = 10;
			color = COLOR_BUTTON_BAR;
		}
		getyx(stdscr, slack, x1);

		attrset(COLOR_PAIR(COLOR_STANDARD));
		addstr(" ");
		addstr(itoa(fkey));
		attrset(COLOR_PAIR(color));
		addstr(_(label));
		addstr("  ");

		getyx(stdscr, slack, x2);

		mouse_register(x1, slack, x2 - x1, 1, BUTTON1_PRESSED, ui_mouse_simulate_keypress,
					   GINT_TO_POINTER(KEY_F(fkey)), 1);
	}
	attrset(COLOR_PAIR(COLOR_STANDARD));
	hline(' ', max_x);
	move(y, x);
}

static gboolean ui_update_now(void)
{
	g_assert((pending_flags & 15) == active_screen);
	screens[active_screen].methods->update(pending_flags);
	pending_tag = 0;
	return FALSE;
}

void ui_update(int screen_and_flags)
{
	int screen = screen_and_flags & 15;

	if (screen >= nr_screens)
		return;

	if (screen_and_flags & UPDATE_ATTENTION)
		ui_hilight_button(screen);

	if (screen != active_screen)
		return;

	/* schedule an update of this screen as soon as possible */
	if (pending_tag) {
		if (pending_timer) {
			/* change the timer to an idle source */
			g_source_remove(pending_tag);
			pending_tag = g_idle_add((GSourceFunc) ui_update_now, NULL);
		}
		pending_flags |= screen_and_flags;
	} else {
		pending_tag = g_idle_add((GSourceFunc) ui_update_now, NULL);
		pending_flags = screen_and_flags;
	}
	pending_timer = FALSE;
}

void ui_update_delayed(int screen_and_flags)
{
	int screen = screen_and_flags & 15;

	if (screen >= nr_screens)
		return;

	if (screen_and_flags & UPDATE_ATTENTION)
		ui_hilight_button(screen);

	if (screen != active_screen)
		return;

	/* schedule an update of this screen */
	if (pending_tag)
		pending_flags |= screen_and_flags;
	else {
		/* delay for some time before updating */
		pending_tag = g_timeout_add(200, (GSourceFunc) ui_update_now, NULL);
		pending_flags = screen_and_flags;
		pending_timer = TRUE;
	}
}

void ui_draw(void)
{
	screens[active_screen].methods->draw();
	ui_draw_buttonbar();
	show_status(NULL);
	move(curs_y, curs_x);
}

/* This changes position in a list. */
/* returns true if position changes */
int ui_list_handler(list *foo, int key, int page_len)
{
	int old_sel = foo->sel;

	switch (key) {
	case KEY_DOWN:
		foo->sel++;
		break;
	case KEY_UP:
		foo->sel--;
		break;
	case KEY_NPAGE:
		foo->sel += page_len;
		break;
	case KEY_PPAGE:
		foo->sel -= page_len;
		break;
	case KEY_HOME:
		foo->sel = 0;
		break;
	case KEY_END:
		foo->sel = foo->num - 1;
		break;
	default:
		break;
	}
	if (foo->sel < 0)
		foo->sel = 0;
	if (foo->sel >= foo->num)
		foo->sel = foo->num - 1;

	return foo->sel != old_sel;
}

void ui_cursor_move(gint x, gint y)
{
	curs_x = x;
	curs_y = y;
	move(curs_y, curs_x);
}
