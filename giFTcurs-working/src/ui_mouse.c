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
 * $Id: ui_mouse.c,v 1.32 2002/10/19 23:27:06 chnix Exp $
 */
#include "giftcurs.h"
#include "screen.h"

#ifdef MOUSE

#include <assert.h>
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

list areas = LIST_INITIALIZER;
int use_mouse = 1;

#ifdef HAVE_LIBGPM
# include <gpm.h>
# include "poll.h"
static void gpm_mouse_event(int fd)
{
	MEVENT event;
	Gpm_Event gev;

	if (Gpm_GetEvent(&gev) <= 0) {
		if (errno != EINTR)
			ERROR("Gpm_GetEvent");
		return;
	}
	if ((gev.type & GPM_MFLAG) && !gev.dy) {
		GPM_DRAWPOINTER(&gev);
		return;
	}
	event.x = gev.x - 1;
	event.y = gev.y - 1;
	if (gev.type & GPM_MFLAG)
		event.bstate = gev.dy < 0 ? BUTTON_UP : BUTTON_DOWN;
	else if (gev.buttons & GPM_B_LEFT)
		event.bstate = BUTTON1_PRESSED;
	else if (gev.buttons & GPM_B_MIDDLE)
		event.bstate = BUTTON2_PRESSED;
	else
		event.bstate = BUTTON3_PRESSED;
	mouse_handler(event);
	GPM_DRAWPOINTER(&gev);
}
#endif

void mouse_init(void)
{
	if (use_mouse) {
#ifdef HAVE_LIBGPM
		Gpm_Connect conn = { GPM_DOWN | GPM_DRAG, GPM_MOVE, 0, 0 };

		if (Gpm_Open(&conn, 0) < 0)
			DEBUG("Cannot connect to gpm");
		else
			poll_add_fd(gpm_fd, gpm_mouse_event);
#endif
#ifdef MOUSE_INTERNAL
		mousemask(0, NULL);
		printf("\033[?1000h");
		define_key("\033[M", KEY_MOUSE);
#else
		mouseinterval(0);
		if (mousemask(ALL_MOUSE_EVENTS, NULL) == 0) {
			DEBUG("No mouse was found. Disabling mouse.");
			use_mouse = 0;
		}
#endif
	}
}

void mouse_deinit(void)
{
	if (use_mouse) {
#ifdef HAVE_LIBGPM
		if (gpm_fd > 0) {
			poll_del_fd(gpm_fd);
			Gpm_Close();
		}
#endif
#ifdef MOUSE_INTERNAL
		printf("\033[?1000l");
#else
		mousemask(0, NULL);
#endif
		list_free_entries(&areas);
	}
}

static int level_filter(marea * kalle, int level)
{
	return kalle->level != level;
}

void mouse_clear(int level)
{
	list_filter(&areas, (LFilter) level_filter, (void *) level, free);
}

void mouse_register(int x, int y, int w, int h, unsigned int bstate, MFunc callback, void *data,
					int level)
{
	marea *kalle;
	int i;

	assert(callback);

	for (i = 0; i < areas.num; i++) {
		kalle = list_index(&areas, i);

		if (kalle->x == x && kalle->y == y && kalle->w == w && kalle->h == h
			&& kalle->bstate == bstate) {
			return;
		}
	}

	kalle = calloc(1, sizeof *kalle);
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
	marea *foo;
	int i;

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

void handle_key_mouse(void)
{
	MEVENT event;

# ifdef MOUSE_INTERNAL
	event.bstate = mouse_translate(getch());
	event.x = getch() - 041;
	event.y = getch() - 041;
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

void ui_mouse_simulate_keypress(int x, int y, void *data)
{
	ui_handler((int) data);
}
#endif
