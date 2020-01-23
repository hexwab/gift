/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 G�ran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian H�ggstr�m <chm@c00.info>
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
 * $Id: ui_draw.c,v 1.115 2003/11/16 16:15:36 weinholt Exp $
 */
#include "giftcurs.h"

#include <wchar.h>
#include <string.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui_draw.h"
#include "ui.h"
#include "tree.h"
#include "wcwidth.h"

void draw_box(int x, int y, int w, int h, const char *title, int attr)
{
	attrset(attr);

	/* Corners */
	if (fancy_utf8) {
#ifdef WIDE_NCURSES
		mvaddwstr(y, x, L"\x256D");
		mvaddwstr(y, x + w - 1, L"\x256E");
		mvaddwstr(y + h - 1, x, L"\x2570");
		mvaddwstr(y + h - 1, x + w - 1, L"\x256F");
#endif
	} else {
		mvaddch(y, x, ACS_ULCORNER);
		mvaddch(y, x + w - 1, ACS_URCORNER);
		mvaddch(y + h - 1, x, ACS_LLCORNER);
		mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
	}

	/* Edges */
	mvhline(y, x + 1, ACS_HLINE, w - 2);
	mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
	mvvline(y + 1, x, ACS_VLINE, h - 2);
	mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);

	/* Title */
	if (title) {
		move(y, x + 2);
		addch(ACS_RTEE);
		addstr(title);
		addch(ACS_LTEE);
	}

	clear_area(x + 1, y + 1, w - 2, h - 2);
}

void draw_printfmt(int w, const char *fmt, ...)
{
	char *s;
	int x, y;
	va_list ap;

	getyx(stdscr, y, x);

	va_start(ap, fmt);
	s = g_strdup_vprintf(fmt, ap);
	draw_fmt_str(x, y, w, FALSE, s);
	g_free(s);
}

G_INLINE_FUNC int selection_color(int selected, int color)
{
	if (INVERT_SEL)
		return selected ? COLOR_PAIR(color) | A_REVERSE : COLOR_PAIR(color);
	else
		return selected ? COLOR_PAIR(COLOR_SELECTION) | A_BOLD : COLOR_PAIR(color);
}

G_INLINE_FUNC int selection_color_special(int selected, int color)
{
	if (INVERT_SEL)
		return selected ? COLOR_PAIR(color) | A_REVERSE : COLOR_PAIR(color);
	else
		return selected ? COLOR_PAIR(color) : COLOR_PAIR(color);
}

/* This function takes a formatted string and draws it on the screen.
   The formatting of the string happens when a '\v' is encountered.
   After the '\v' is a char
   The switch in the code figures out what to do based on this.  */
void draw_fmt_str(int x, int y, int w, gboolean selected, const char *str)
{
	int printed = 0;
	const char *u;

	move(y, x);
	attrset(selection_color(selected, COLOR_STANDARD));

	for (u = str; *u && printed < w; u++) {
		if (*u == COLOR_SELECT_CHAR) {
			u++;
			if (*u & COLOR_SPECIAL) {
				attrset(selection_color_special(selected, *u & COLOR_MASK));
			} else if (*u & COLOR_BOLD) {
				attrset(selection_color(selected, *u & COLOR_MASK));
				attron(A_BOLD);
			} else {
				attrset(selection_color(selected, *u & COLOR_MASK));
			}
			continue;
		}

		if (((unsigned char) *u >= 0x7f && (unsigned char) *u < 0xa0)) {
			/* Filter out C1 (8-bit) control characters */
			addch('?');
			printed++;
		} else {
			if (utf8) {
#ifdef WIDE_NCURSES
				/* Get a unicode character from the next few bytes. */
				wchar_t wch[2];
				cchar_t cc;

				wch[0] = g_utf8_get_char_validated(u, -1);
				wch[1] = L'\0';
				if (wch[0] < 0)
					continue;	/* invalid utf-8 sequence */
				if (wch[0] < 32)
					wch[0] = 0x2400 + wch[0];	/* control char symbols */
				setcchar(&cc, wch, A_NORMAL, 0, NULL);
				add_wch(&cc);
				printed += mk_wcwidth(wch[0]);
				u = g_utf8_next_char(u) - 1;
#endif
			} else {
				if ((unsigned char) *u < 32) {
					/* Represent control chars as e.g. ^G */
					addch('^');
					addch(*u + 64);
					printed++;
				} else
					addnstr(u, 1);
				printed++;
			}
		}
	}
	hline(' ', w - printed);
}

void clear_area(int x, int y, int w, int h)
{
	int i;

	attrset(COLOR_PAIR(COLOR_STANDARD));
	for (i = 0; i < h; i++)
		mvhline(y++, x, ' ', w);
}

static int scale(int x, int oldscale, int newscale)
{
	/* It took me several hours to come up with this formula :) */
	return (2 * x * (newscale - 1) / (oldscale - 1) + 1) / 2;
}

void draw_list_pretty(int x, int y, int w, int h, int draw_sel, list *snafu)
{
	int i;

	attrset(COLOR_PAIR(COLOR_STANDARD));
	clear_area(x, y, w + 1, h);

	if (snafu->num == 0)
		return;

	for (i = snafu->start; i < snafu->start + h && i < snafu->num; i++) {
		tree_node *node = snafu->entries[i];
		char *s = node->pretty;

		if (s == NULL)
			s = node->pretty = node->klass->pretty_line(node);
		draw_fmt_str(x, y - snafu->start + i, w, i == snafu->sel && draw_sel, s);
	}

	if (h > 1 && snafu->num > h) {
		attrset(COLOR_PAIR(COLOR_DIAMOND));
		mvaddch(y + scale(snafu->sel, snafu->num, h), x + w, ACS_DIAMOND);
	}
}

void draw_list(int x, int y, int w, int h, int draw_sel, list *snafu)
{
	int i;

	attrset(COLOR_PAIR(COLOR_STANDARD));
	clear_area(x, y, w + 1, h);

	if (snafu->num == 0)
		return;

	for (i = snafu->start; i < snafu->start + h && i < snafu->num; i++) {
		char *s = snafu->entries[i];

		draw_fmt_str(x, y - snafu->start + i, w, i == snafu->sel && draw_sel, s);
	}

	if (h > 1 && snafu->num > h) {
		int off;

		if (snafu->sel < 0)
			off = scale(snafu->start, snafu->num - h + 1, h);
		else
			off = scale(snafu->sel, snafu->num, h);
		attrset(COLOR_PAIR(COLOR_DIAMOND));
		mvaddch(y + off, x + w, ACS_DIAMOND);
	}
}

#undef draw_button

void draw_button(int x, int y, const char *str, int attr
#ifdef MOUSE
				 , MFunc callback, void *data
#endif
	)
{
	int len = vstrlen(str) + 4;

	x -= len / 2;

	move(y, x);
	attrset(attr);
	addstr("[ ");
	addstr(str);
	addstr(" ]");

#ifdef MOUSE
	if (callback)
		mouse_register(x, y, len, 1, BUTTON1_PRESSED, callback, data, 0);
#endif
}

void show_status(const char *status)
{
	/* This leaks on exit. */
	static char *current_msg = NULL;

	/* The number of bytes we can use to display max_x chars. */
	int width = max_x - 1;
	int oy, ox, y = max_y - 1 - show_buttonbar;

	if (status) {
		g_free(current_msg);
		current_msg = g_strdup(status);

	} else if (!current_msg) {
		return;
	}

	if (utf8) {
		char *p = current_msg;
		int len = 0;

		for (; *p && len < max_x - 1; p = g_utf8_next_char(p))
			len += g_unichar_iswide(g_utf8_get_char(p)) ? 2 : 1;
		width = p - current_msg;
	}

	getyx(stdscr, oy, ox);
	attrset(COLOR_PAIR(COLOR_STAT_DATA));
	mvhline(y, 0, ' ', max_x);
	mvaddnstr(y, 0, current_msg, width);
	move(oy, ox);
}

/* Clear the screen - but avoid the statusline */
void clrscr(void)
{
	erase();
}
