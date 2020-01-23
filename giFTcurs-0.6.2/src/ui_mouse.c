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
 * $Id: ui_mouse.c,v 1.45 2003/08/01 14:00:00 weinholt Exp $
 */
#include "giftcurs.h"
#include "screen.h"

#ifdef MOUSE

#include <stdlib.h>
#include <errno.h>

#include "list.h"
#include "ui_mouse.h"
#include "ui.h"

typedef struct {
	int x, y, w, h;
	unsigned int bstate;
	MFunc callback;
	void *data;
	int level;
} marea;

static GTimer *last_click = NULL;
static list areas = LIST_INITIALIZER;
gboolean use_mouse = TRUE;

#ifdef HAVE_LIBGPM
# include <gpm.h>
# include <errno.h>
static gboolean gpm_mouse_event(void)
{
	MEVENT event;
	Gpm_Event gev;

	if (Gpm_GetEvent(&gev) <= 0) {
		if (errno != EINTR)
			DEBUG("Gpm_GetEvent: %s (%d)", g_strerror(errno), errno);
		return TRUE;
	}
	if (!(gev.type & GPM_MFLAG) || gev.dy) {
		event.x = gev.x;
		event.y = gev.y;
		if (gev.type & GPM_MFLAG)
			event.bstate = gev.dy < 0 ? BUTTON_UP : BUTTON_DOWN;
		else if (gev.buttons & GPM_B_LEFT)
			event.bstate = BUTTON1_PRESSED;
		else if (gev.buttons & GPM_B_MIDDLE)
			event.bstate = BUTTON2_PRESSED;
		else
			event.bstate = BUTTON3_PRESSED;
		mouse_handler(event);
	}
	GPM_DRAWPOINTER(&gev);
	return TRUE;
}
#endif

void mouse_init(void)
{
	if (use_mouse) {
#ifdef HAVE_LIBGPM
		Gpm_Connect conn = { GPM_DOWN | GPM_DRAG, GPM_MOVE, 0, 0 };

		if (Gpm_Open(&conn, 0) < 0)
			DEBUG("Cannot connect to gpm");
		else {
			g_io_add_watch(g_io_channel_unix_new(gpm_fd), G_IO_IN, (GIOFunc) gpm_mouse_event,
						   gpm_mouse_event);
			gpm_zerobased = 1;
		}
#endif
#ifdef MOUSE_INTERNAL
		mousemask(0, NULL);
		printf("\033[?1000h");
		define_key("\033[M", KEY_MOUSE);
		define_key("\033[5M", KEY_MOUSE_DRAG);
#else
		mouseinterval(0);
		if (mousemask(ALL_MOUSE_EVENTS, NULL) == 0) {
			DEBUG("No mouse was found. Disabling mouse.");
			use_mouse = 0;
		}
#endif
		last_click = g_timer_new();
	}
}

void mouse_deinit(void)
{
	if (use_mouse) {
#ifdef HAVE_LIBGPM
		if (gpm_fd > 0) {
			g_source_remove_by_user_data(gpm_mouse_event);
			Gpm_Close();
		}
#endif
#ifdef MOUSE_INTERNAL
		printf("\033[?1000l");
#else
		mousemask(0, NULL);
#endif
		list_free_entries(&areas);
		g_timer_destroy(last_click);

		use_mouse = FALSE;
	}
}

static int level_filter(marea * kalle, gpointer level)
{
	return kalle->level != GPOINTER_TO_INT(level);
}

void mouse_clear(int level)
{
	list_filter(&areas, (LFilter) level_filter, GINT_TO_POINTER(level), free);
}

void mouse_register(int x, int y, int w, int h, unsigned int bstate, MFunc callback, void *data,
					int level)
{
	marea *kalle;
	int i;

	g_assert(callback);

	for (i = 0; i < areas.num; i++) {
		kalle = list_index(&areas, i);

		if (kalle->x == x && kalle->y == y && kalle->w == w && kalle->h == h
			&& kalle->bstate == bstate) {
			return;
		}
	}

	kalle = g_new(marea, 1);
	kalle->x = x;
	kalle->y = y;
	kalle->w = w;
	kalle->h = h;
	kalle->bstate = bstate;
	kalle->callback = callback;
	kalle->data = data;
	kalle->level = level;
	list_append(&areas, kalle);
}

int mouse_check(MEVENT * m)
{
	static int last_x = -1, last_y = -1, last_state = 0;
	marea *foo;
	int i;

	/* Check if this was a doubleclick. */
	if (m->x == last_x && m->y == last_y && last_state == m->bstate
		&& g_timer_elapsed(last_click, NULL) < 0.2) {
		if (m->bstate & BUTTON1_PRESSED) {
			m->bstate &= ~BUTTON1_PRESSED;
			m->bstate |= BUTTON1_DOUBLE_CLICKED;
		} else if (m->bstate & BUTTON2_PRESSED) {
			m->bstate &= ~BUTTON2_PRESSED;
			m->bstate |= BUTTON2_DOUBLE_CLICKED;
		} else if (m->bstate & BUTTON3_PRESSED) {
			m->bstate &= ~BUTTON3_PRESSED;
			m->bstate |= BUTTON3_DOUBLE_CLICKED;
		}
	}
	/* Record this click for posterity. */
	if (m->bstate & BUTTON1_PRESSED || m->bstate & BUTTON2_PRESSED || m->bstate & BUTTON3_PRESSED) {
		last_x = m->x;
		last_y = m->y;
		last_state = m->bstate;
		g_timer_start(last_click);
	}

	/* Check if anyone cares about this event. */
	for (i = 0;; i++) {
		if (i >= areas.num)
			return 0;

		foo = list_index(&areas, i);

		if (m->x >= foo->x && m->y >= foo->y && m->x < foo->x + foo->w && m->y < foo->y + foo->h
			&& m->bstate & foo->bstate)
			break;
	}
	if (foo->callback) {
		foo->callback(m->x - foo->x, m->y - foo->y, foo->data);
		return 1;
	}

	return 0;
}

#ifdef MOUSE_INTERNAL
static mmask_t mouse_translate(int key)
{
	if (key == 040)
		return BUTTON1_PRESSED;
	if (key == 041)
		return BUTTON2_PRESSED;
	if (key == 042)
		return BUTTON3_PRESSED;
	if (key == 0140)
		return BUTTON_UP;		/* mouse wheel up */
	if (key == 0141)
		return BUTTON_DOWN;		/* mouse wheel down */
	return 0;
}
#endif

void handle_key_mouse(int key)
{
	MEVENT event;

# ifdef MOUSE_INTERNAL
	switch (key) {
	case KEY_MOUSE:
		/* Example: ^[[M ]) */
		event.bstate = mouse_translate(getch());
		event.x = getch() - 041;
		event.y = getch() - 041;
		break;
	case KEY_MOUSE_DRAG:
		{						/* Example: ^[[5M!<!&! */
			int tmp, button, state, x, y;

			tmp = getch() - 040;
			button = tmp & 3;
			state = tmp & ~3;

			x = getch() - 041;
			getch();			/* ignore the extra bits */
			y = getch() - 041;
			getch();			/* ignore the extra bits */
			return				/* XXX: we ignore these.. might be used for dragging the
								   splitter between downloads/uploads maybe. */ ;
		}
		break;
	default:
		g_assert_not_reached();
	}
	/* Yozik@linuxmail.org reported that his mouse wheel sends more
	 * characters, for example 't' which caused unwanted cancellations.
	 * However, he was not able to reproduce it. */
# else
	if (getmouse(&event) != OK) {
		DEBUG("mouse error");
		return;
	}
# endif
	mouse_handler(event);
}

void ui_mouse_simulate_keypress(int x, int y, gpointer data)
{
	ui_handler(GPOINTER_TO_INT(data));
}
#endif
