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
 * $Id: ui.c,v 1.87 2002/10/22 13:18:10 weinholt Exp $
 */
#include "giftcurs.h"

#include <string.h>
#include <stdlib.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "ui_main.h"
#include "ui_transfer.h"
#include "ui_help.h"
#include "ui_settings.h"
#include "settings.h"
#include "misc.h"

ui_methods *active_screen = &main_screen_methods;
int show_buttonbar = 0;
static int selected_button = 2;

void ui_init(void)
{
	show_buttonbar = atoi(get_config("set", "buttonbar", "1"));

	main_screen_init();
	help_screen_init();
	transfer_screen_init();
	settings_screen_init();
}

void ui_destroy(void)
{
	main_screen_destroy();
	help_screen_destroy();
	transfer_screen_destroy();
	settings_screen_destroy();
	mouse_deinit();
}

/* This routine shows all MESSAGE's we got from giFT.
   Runs in full screen, called from main.c. */
void ui_show_messages(void)
{
	int i;

	clear();
	for (i = 0; i < messages.num; i++) {
		char *foo;

		asprintf(&foo, _("Message %i of %i."), i + 1, messages.num);

		erase();
		attrset(COLOR_PAIR(COLOR_HEADER));
		mvaddstr(0, 0, _("And now a few words from our sponsor:"));

		attrset(A_BOLD | COLOR_PAIR(COLOR_SEARCH_BOX));
		mvaddstr(2, 0, list_index(&messages, i));


		attrset(COLOR_PAIR(COLOR_STANDARD));
		mvaddstr(max_y - 2, 0, foo);
		mvaddstr(max_y - 1, 0, _("Press any key to continue..."));

		refresh();
		getch();
		free(foo);
	}
	clear();
	refresh();
}

static void new_screen(ui_methods * new_methods)
{
	if (active_screen != new_methods) {
		active_screen = new_methods;
		clrscr();
		ui_draw();
		refresh();
	}
}

#ifdef MOUSE
void mouse_handler(MEVENT event)
{
	if (!mouse_check(&event))
		if (event.bstate & BUTTON2_PRESSED) {
			if (active_screen == &main_screen_methods) {
				selected_button = 3;
				new_screen(&transfer_screen_methods);
			} else {
				selected_button = 2;
				new_screen(&main_screen_methods);
			}
		}
}
#endif

void ui_handler(int key)
{
	if (key == '\t' && shift_pressed())
		key = KEY_BTAB;

	switch (key) {
#ifdef MOUSE
	case KEY_MOUSE:
		if (use_mouse)
			handle_key_mouse();
		break;
#endif
	case KEY_F(1):
		selected_button = 1;
		new_screen(&help_screen_methods);
		break;
	case KEY_F(2):
		selected_button = 2;
		new_screen(&main_screen_methods);
		break;
	case KEY_F(3):
		selected_button = 3;
		new_screen(&transfer_screen_methods);
		break;
	case KEY_F(4):
		selected_button = 4;
		new_screen(&settings_screen_methods);
		break;
#ifdef KEY_RESIZE
	case KEY_RESIZE:
		clrscr();
		wrefresh(curscr);
		getmaxyx(stdscr, max_y, max_x);
		ui_draw();
		refresh();
		break;
#endif
	case ERR:
	case KEY_F(10):
		graceful_death();
		return;
	case XCTRL('l'):
	case XCTRL('r'):
		wrefresh(curscr);
		break;
	default:
		/* pass through to the handler for the active screen */
		if (active_screen->key_handler(key))
			graceful_death();
		return;
	}
	return;
}

static void ui_draw_buttonbar(void)
{
	/* *INDENT-OFF* */
	static struct {
		char *label;
		int fkey;
	} const buttonbar[] = {
		{N_("Help"), 1},
		{N_("Searches"), 2},
		{N_("Transfers"), 3},
		{N_("Settings"), 4},
		{N_("Exit"), 10},
	};
	/* *INDENT-ON* */
	int i, y, x;
	char buf[15];

	mouse_clear(1);
	getyx(stdscr, y, x);
	move(max_y - 1, 0);
	for (i = 0; i < buflen(buttonbar); i++) {
		int x1, x2, slack;

		getyx(stdscr, slack, x1);

		attrset(COLOR_PAIR(COLOR_STANDARD));
		snprintf(buf, sizeof buf, " %i", buttonbar[i].fkey);
		addstr(buf);
		attrset(COLOR_PAIR
				(buttonbar[i].fkey == selected_button ? COLOR_BUTTON_BAR_SEL : COLOR_BUTTON_BAR));
		addstr(_(buttonbar[i].label));
		addstr("  ");

		getyx(stdscr, slack, x2);

		mouse_register(x1, slack, x2 - x1, 1, BUTTON1_PRESSED, ui_mouse_simulate_keypress,
					   (void *) KEY_F(buttonbar[i].fkey), 1);
	}
	attrset(COLOR_PAIR(COLOR_STANDARD));
	hline(' ', max_x);
	move(y, x);
}

void ui_draw(void)
{
	active_screen->draw();
	if (show_buttonbar)
		ui_draw_buttonbar();
	show_status(NULL);
}

/* This changes position in a list. */
/* returns true if position changes */
int ui_list_handler(list * foo, int key, int page_len)
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

/* This handles keys written into a inputbox. */
int ui_input_handler(char *foo, int *posp, int key, int max_len)
{
	int i, ret = 1, pos = *posp;

	switch (key) {
	case 0:
		break;
	case KEY_HOME:
	case XCTRL('a'):
		pos = 0;
		break;
	case KEY_END:
	case XCTRL('e'):
		pos = strlen(foo);
		break;
	case XCTRL('k'):
		foo[pos] = '\0';
		break;
	case XCTRL('u'):
		i = strlen(foo);
		memmove(foo, foo + pos, i - pos + 1);
		pos = 0;
		break;
	case XCTRL('w'):
		foo = remove_word(foo, &pos);
		break;
	case KEY_LEFT:
		pos--;
		break;
	case KEY_RIGHT:
		pos++;
		break;
	case '\177':
	case '\b':
	case KEY_BACKSPACE:
	case KEY_DC:				/* delete char */
		i = strlen(foo);
		if ((pos == 0 && key != KEY_DC) || (i == pos && key == KEY_DC))
			break;
		if (key != KEY_DC)
			pos--;
		memmove(foo + pos, foo + pos + 1, i - pos + 1);
		break;
	default:
		i = strlen(foo);
		if (i + 1 >= max_len || key > 255 || key < ' ') {
			ret = 0;
			break;
		}
		memmove(foo + pos + 1, foo + pos, i - pos + 2);
		foo[pos] = key;
		pos++;
		break;
	}
	if (pos < 0)
		pos = 0;
	if (pos > (i = strlen(foo)))
		pos = i;

	*posp = pos;

	return ret;
}
