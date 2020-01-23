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
 * $Id: ui_draw.c,v 1.85 2002/11/08 17:54:06 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui_draw.h"
#include "tree.h"
#include "poll.h"
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

void draw_header(int x, int y, char *header)
{
	move(y, x);
	attrset(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
	addstr(header);
	addstr(": ");
	attrset(COLOR_PAIR(COLOR_STANDARD));
}

int draw_printf(int w, char *fmt, ...)
{
	char *s;
	int i;
	va_list ap;

	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	addnstr(s, w);
	i = strnlen(s, w);
	free(s);
	return i;
}

int draw_printfmt(int w, char *fmt, ...)
{
	char *s;
	int i;
	int x, y;
	va_list ap;

	getyx(stdscr, y, x);

	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	draw_fmt_str(x, y, w, 1 == 0, s);
	i = strnlen(s, w);
	free(s);
	return i;
}

/* Returns the x-position of the end of the printed value */
int draw_input(int x, int y, int w, char *header, char *value, int attr)
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

#define SELECTION_COLOR(s, c) \
		INVERT_SEL ? \
		((s) ? COLOR_PAIR(c) | A_REVERSE : COLOR_PAIR(c)) : \
		((s) ? COLOR_PAIR(COLOR_SELECTION) | A_BOLD : COLOR_PAIR(c))

#define SELECTION_COLOR_SPECIAL(s, c) \
		INVERT_SEL ? \
		((s) ? COLOR_PAIR(c) | A_REVERSE : COLOR_PAIR(c)) : \
		((s) ? COLOR_PAIR(c) : COLOR_PAIR(c))

/* This function takes a formatted string and draws it on the screen.
   The formatting of the string happens when a '\v' is encountered.
   After the '\v' is a char
   The switch in the code figures out what to do based on this.  */
void draw_fmt_str(int x, int y, int w, int selected, char *str)
{
	int printed = 0;
	char *u;

	move(y, x);
	attrset(SELECTION_COLOR(selected, COLOR_STANDARD));

	for (u = str; *u && printed < w; u++) {
		if (*u == '\v') {
			u++;
			if (*u == '4')
				attrset(SELECTION_COLOR_SPECIAL(selected, COLOR_PROGRESS));
			else if (*u == '5')
				attrset(SELECTION_COLOR_SPECIAL(selected, COLOR_TOTAL_PROGRESS));
			else if ('a' <= *u && *u < 'a' + ITEMS_NUM)
				attrset(SELECTION_COLOR(selected, *u - 'a' + 1));
			else if ('A' <= *u && *u < 'A' + ITEMS_NUM) {
				attrset(SELECTION_COLOR(selected, *u - 'A' + 1));
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

void draw_list_pretty(int x, int y, int w, int h, int draw_sel, list * snafu)
{
	int i;

	attrset(COLOR_PAIR(COLOR_STANDARD));
	clear_area(x, y, w + 1, h);

	if (snafu->num == 0)
		return;

	for (i = snafu->start; i < snafu->start + h && i < snafu->num; i++) {
		rendering **r = snafu->entries[i];
		char *s = (*r)->pretty_line(r);

		draw_fmt_str(x, y - snafu->start + i, w, i == snafu->sel && draw_sel, s);
	}

	attrset(COLOR_PAIR(COLOR_DIAMOND));
	if (snafu->num > h)
		mvaddch(y + h * (float) snafu->sel / snafu->num, x + w, ACS_DIAMOND);
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

	attrset(COLOR_PAIR(COLOR_DIAMOND));
	if (snafu->num > h) {
		float off;

		if (snafu->sel < 0)
			off = (float) snafu->start / (snafu->num - h);
		else
			off = (float) snafu->sel / snafu->num;
		mvaddch(y + (h - 1) * off, x + w, ACS_DIAMOND);
	}
}

#undef draw_button

void draw_button(int x, int y, char *str, int attr
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

/* This leaks on exit. */
static char *current_msg = NULL;

void show_status(char *status)
{
	int y, x;

	if (!status) {
		if (!current_msg)
			return;
		status = current_msg;
	} else {
		free(current_msg);
		current_msg = strdup(status);
	}

	getyx(stdscr, y, x);
	attrset(COLOR_PAIR(COLOR_STAT_DATA));
	mvaddnstr(max_y - 1 - show_buttonbar, 0, status, max_x - 1);
	hline(' ', max_x);
	move(y, x);
}

/* Clear the screen - but avoid the statusline */
void clrscr(void)
{
	erase();
}
