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
 * $Id: ui_input.c,v 1.1 2003/11/16 16:15:37 weinholt Exp $
 */
#include "giftcurs.h"

#include <string.h>

#include "ui.h"
#include "ui_input.h"
#include "ui_draw.h"
#include "parse.h"
#include "wcwidth.h"

void ui_input_init(ui_input *input)
{
	input->str = g_string_new("");
	input->use_utf8 = utf8;
	input->start = input->pos = input->vpos = 0;
}

void ui_input_deinit(ui_input *input)
{
	g_string_free(input->str, TRUE);
	/* deinit more here, if we care */
}

/* This handles keys written into a inputbox. */
int ui_input_handler(ui_input *input, int key)
{
	int ret = 1;
	GString *foo = input->str;

/*	input->use_utf8 = utf8 && g_utf8_validate(input->str.str, input->str.len, NULL); */

	switch (key) {
	case 0:
		break;
	case KEY_HOME:
	case XCTRL('a'):
		input->pos = 0;
		break;
	case KEY_END:
	case XCTRL('e'):
		input->pos = foo->len;
		break;
	case XCTRL('k'):
		g_string_truncate(foo, input->pos);
		break;
	case XCTRL('u'):
		g_string_erase(foo, 0, input->pos);
		input->pos = 0;
		break;
	case XCTRL('w'):
		remove_word(foo, &input->pos);
		break;
	case KEY_LEFT:
		if (input->use_utf8) {
			char *prevc = utf8_find_prev_char(foo->str, foo->str + input->pos);

			if (!prevc)
				input->pos = 0;
			else
				input->pos = prevc - foo->str;
		} else {
			if (input->pos > 0)
				input->pos--;
		}
		break;
	case KEY_RIGHT:
		if (input->use_utf8) {
			char *nextc = utf8_find_next_char(foo->str + input->pos, NULL);

			if (!nextc)
				input->pos = foo->len;
			else
				input->pos = nextc - foo->str;
		} else {
			input->pos++;
		}
		break;
	case '\177':
	case '\b':
	case KEY_BACKSPACE:
	case KEY_DC:				/* delete char */
		if ((input->pos == 0 && key != KEY_DC) || (input->pos == foo->len && key == KEY_DC))
			break;
		if (input->use_utf8) {
			if (key == KEY_DC) {
				char *nextc = utf8_find_next_char(foo->str + input->pos, NULL);
				int npos = nextc - foo->str;

				g_string_erase(foo, input->pos, npos - input->pos);
			} else {
				char *prevc = utf8_find_prev_char(foo->str, foo->str + input->pos);
				int npos = prevc - foo->str;

				g_string_erase(foo, npos, input->pos - npos);
				input->pos = npos;
			}
		} else {
			if (key != KEY_DC)
				input->pos--;
			g_string_erase(foo, input->pos, 1);
		}
		break;
	default:
		if (key > 255 || key < ' ') {
			ret = 0;
			break;
		}
		g_string_insert_c(foo, input->pos, key);
		input->pos++;
		break;
	}

	return ret;
}

void ui_input_validate(ui_input *input)
{
	input->pos = CLAMP(input->pos, 0, input->str->len);

	input->use_utf8 = utf8 && g_utf8_validate(input->str->str, input->str->len, NULL);

	if (input->use_utf8) {
#ifdef WIDE_NCURSES
		char *p;

		input->vpos = 0;
		for (p = input->str->str; *p && p < input->str->str + input->pos; p = g_utf8_next_char(p))
			input->vpos += mk_wcwidth(g_utf8_get_char(p));
#endif
	} else {
		input->vpos = input->pos;
	}
}

void ui_input_setvpos(ui_input *input, gint vpos)
{
	/* FIXME: use input->start. */
	input->pos = str_occupy(input->str->str, vpos, TRUE);
	ui_input_validate(input);
}

void ui_input_assign(ui_input *input, const gchar * str)
{
	g_string_assign(input->str, str);
	input->pos = input->str->len;	/* place cursor at the end */
	input->start = 0;
	ui_input_validate(input);
}

void ui_input_draw(int x, int y, int w, int attr, const char *header, ui_input *input,
				   gboolean active)
{
	int vx, print, tmpx;
	const char *value = input->str->str;

	if (!value)
		value = "";

	/* Header */
	move(y, x);
	if (header) {
		attrset(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
		addstr(header);
		addstr(": ");
	}

	/* Decoration */
	attrset(attr);
	addch('[');
	getyx(stdscr, y, vx);		/* vx = beginning of room for the string */
	move(y, x + w - 1);
	addch(']');

	/* How many chars in the string we should try to skip. */
	input->start = input->vpos - (x + w - vx - 2);
	input->start = CLAMP(input->start, 0, vstrlen(value));

	/* Value */
#ifdef WIDE_NCURSES
	if (input->use_utf8) {
		int i;

		for (i = 0; i < input->start;) {
			i += mk_wcwidth(g_utf8_get_char(value));
			value = g_utf8_next_char(value);
		}
	} else
#endif
		value += input->start;
	attrset(COLOR_PAIR(COLOR_STAT_DATA));
	print = str_occupy(value, w - vx, TRUE);
	mvaddnstr(y, vx, value, print);

	/* Trailing '_' */
	getyx(stdscr, y, tmpx);
	hline('_', w - tmpx + 1);

	/* Move the cursor */
	if (active) {
		ui_cursor_move(vx + input->vpos - input->start, y);
		curs_set(1);
		leaveok(stdscr, FALSE);
	}
}
