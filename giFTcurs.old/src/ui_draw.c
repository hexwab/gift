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
 * $Id: ui_draw.c,v 1.97 2003/05/14 09:09:36 chnix Exp $
 */
#include "giftcurs.h"

#include <string.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui_draw.h"
#include "tree.h"
#include "ui.h"

void draw_box(int x, int y, int w, int h, char *title, int attr)
{
	attrset(attr);

	/* Corners */
	mvaddch(y, x, ACS_ULCORNER);
	mvaddch(y, x + w - 1, ACS_URCORNER);
	mvaddch(y + h - 1, x, ACS_LLCORNER);
	mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

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

void draw_header(int x, int y, const char *header)
{
	move(y, x);
	attrset(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
	addstr(header);
	addstr(": ");
	attrset(COLOR_PAIR(COLOR_STANDARD));
}

int draw_printfmt(int w, const char *fmt, ...)
{
	char *s;
	int i;
	int x, y;
	va_list ap;

	getyx(stdscr, y, x);

	va_start(ap, fmt);
	s = g_strdup_vprintf(fmt, ap);
	draw_fmt_str(x, y, w, 1 == 0, s);
	i = strnlen(s, w);
	g_free(s);
	return i;
}

/* Returns the x-position of the end of the printed value */
int draw_input(int x, int y, int w, const char *header, const char *value, int attr)
{
	int p = 3;

	if (header)
		draw_header(x, y, header);

	attrset(attr);
	addch('[');

	/* Value */
	attrset(COLOR_PAIR(COLOR_STAT_DATA));
	if (header)
		getyx(stdscr, y, p);
	addnstr(value, w - p);
	getyx(stdscr, y, p);
	hline('_', x + w - p - 1);	/* Trailing '_' */

	attrset(attr);
	move(y, x + w - 1);
	addch(']');

	return p;
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
void draw_fmt_str(int x, int y, int w, int selected, const char *str)
{
	int printed = 0;
	const char *u;

	move(y, x);
	attrset(selection_color(selected, COLOR_STANDARD));

	for (u = str; *u && printed < w; u++) {
		if (*u == '\v') {
			u++;
			if (*u == '4')
				attrset(selection_color_special(selected, COLOR_PROGRESS));
			else if (*u == '5')
				attrset(selection_color_special(selected, COLOR_TOTAL_PROGRESS));
			else if ('a' <= *u && *u < 'a' + ITEMS_NUM)
				attrset(selection_color(selected, *u - 'a' + 1));
			else if ('A' <= *u && *u < 'A' + ITEMS_NUM) {
				attrset(selection_color(selected, *u - 'A' + 1));
				attron(A_BOLD);
			}
			continue;
		}

		/* No printing control-chars here */
		if ((unsigned char) *u < 32 || ((unsigned char) *u >= 0x7f && (unsigned char) *u < 0xa0))
			addch(' ');
		else
			addnstr(u, 1);
		printed++;
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

void draw_list_pretty(int x, int y, int w, int h, int draw_sel, list * snafu)
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
			s = node->pretty = node->methods->pretty_line(node);
		draw_fmt_str(x, y - snafu->start + i, w, i == snafu->sel && draw_sel, s);
	}

	if (h > 1 && snafu->num > h) {
		attrset(COLOR_PAIR(COLOR_DIAMOND));
		mvaddch(y + scale(snafu->sel, snafu->num, h), x + w, ACS_DIAMOND);
	}
}

void draw_list(int x, int y, int w, int h, int draw_sel, list * snafu)
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
	x -= (strlen(str) + 4) / 2;

	move(y, x);
	attrset(attr);
	addstr("[ ");
	addstr(str);
	addstr(" ]");

#ifdef MOUSE
	if (callback)
		mouse_register(x, y, strlen(str) + 4, 1, BUTTON1_PRESSED, callback, data, 0);
#endif
}

void show_status(char *status)
{
	/* This leaks on exit. */
	static char *current_msg = NULL;
	int y, x;

	if (status) {
		g_free(current_msg);
		current_msg = g_strdup(status);
	} else if (!current_msg) {
		return;
	}

	getyx(stdscr, y, x);
	attrset(COLOR_PAIR(COLOR_STAT_DATA));
	mvaddnstr(max_y - 1 - show_buttonbar, 0, current_msg, max_x - 1);
	hline(' ', max_x);
	move(y, x);
}

/* Clear the screen - but avoid the statusline */
void clrscr(void)
{
	erase();
}
