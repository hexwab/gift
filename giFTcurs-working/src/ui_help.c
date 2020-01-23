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
 * $Id: ui_help.c,v 1.67 2002/11/08 17:54:06 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "ui_help.h"

static struct {
	list help;
	int build_width;
} scr = {
LIST_INITIALIZER, -1,};

/* Rules for the help text, so that line split works:
	- Don't use tab
	- Always highlight one word at a time only, or use non-breaking spaces */
const char helptext[] = N_("\
Welcome to giFTcurs!\n\
\n\
Key list:\n\
\vBup\va/\vBdown\va        select item in list\n\
\vBPgUp\va/\vBPgDown\va    scroll list one screen at a time\n\
\vBtab\va            move to the next input field\n\
\vBF1\va             this help screen\n\
\vBF2\va             the main screen\n\
    \vBenter\va      download the highlighted file\n\
    \vBleft\va/\vBright\va change sorting method\n\
    \vBe\va          expand/collapse search results\n\
    \vBj\va/\vBk\va        scroll information box\n\
\vBF3\va             the transfer screen\n\
    \vBenter\va/\vBe\va    expand/collapse transfer\n\
    \vBj\va/\vBk\va        move window splitter\n\
\vBF4\va             the settings screen\n\
\vBF10\va            exit the program\n\
\n\
See giFTcurs(1) and giFTcurs.conf(5) for more information.\n\
\n");

/* TRANSLATORS: The authors' names are spellt Göran Weinholt and
   Christian Häggström, so if your charset has support for 'a' and 'o'
   with two dots over then please use those characters in their names. */
const char copyright[] = N_("\
Copyright (C) 2001, 2002  Goran Weinholt.\n\
Co-developed by Christian Haggstrom.\n\
Our homepage is at http://giFTcurs.sourceforge.net/.\n\
\n");

const char license[] = "\
This program is free software; you can redistribute it and/or modify\
 it under the terms of version 2 of the GNU General Public License as\
 published by the Free Software Foundation.\n\
\n\
This program is distributed in the hope that it will be useful,\
 but WITHOUT ANY WARRANTY; without even the implied warranty of\
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\
 GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\
 along with this program; if not, write to the Free Software Foundation,\
 Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.";

static void help_screen_resize(void);
static void help_screen_update(void);

#ifdef MOUSE
/* Mouse callback */
static void mouse_helpclick(int rx, int ry, void *data);
#endif

void help_screen_init(void)
{
	list_initialize(&scr.help);
	help_screen_resize();
}

static void help_screen_resize(void)
{
	char *text, *fnord;
	int width = max_x - 4;

	asprintf(&fnord, "%s%s%s", _(helptext), _(copyright), license);

	list_free_entries(&scr.help);
	text = fnord;
	for (;;) {
		char *pos = strchr(text, '\n');

		if (pos && pos <= text + width) {
			list_append(&scr.help, strndup(text, pos - text));
			text = pos + 1;
			continue;
		}
		pos = strchr(text, '\0');
		if (pos && pos <= text + width) {
			list_append(&scr.help, strdup(text));
			break;
		}
		for (pos = text + width; pos > text; pos--) {
			if (*pos == ' ') {
				list_append(&scr.help, strndup(text, pos - text));
				text = pos + 1;
				break;			/* continue the outer loop */
			}
		}
		if (text == pos + 1)
			continue;
		list_append(&scr.help, strndup(text, width));
		text += width;
	}
	scr.build_width = max_x;
	free(fnord);
}

void help_screen_destroy(void)
{
	list_free_entries(&scr.help);
}

static void help_screen_draw(void)
{
	clrscr();
	curs_set(0);
	leaveok(stdscr, TRUE);
	draw_box(0, 0, max_x, max_y - 1 - show_buttonbar, _("Help"),
			 COLOR_PAIR(COLOR_HELP_BOX) | A_BOLD);
	mouse_clear(0);
	mouse_register(0, 0, max_x, max_y - 1 - show_buttonbar, BUTTON1_PRESSED, mouse_helpclick, NULL,
				   0);
	help_screen_update();
}

static void help_screen_update(void)
{
	int height = max_y - 3 - show_buttonbar;

	if (max_x != scr.build_width)
		help_screen_resize();
	list_check_values_simple(&scr.help, height);
	/* This list is special in the aspect that it has no selected item.
	 * negative scr.help.sel tells draw_list to place the diamond right. */
	scr.help.sel = -1;
	draw_list(2, 1, max_x - 4, height, FALSE, &scr.help);
	refresh();
}

static int help_screen_handler(int key)
{
	scr.help.sel = scr.help.start;
	ui_list_handler(&scr.help, key, max_y - 3 - show_buttonbar);
	scr.help.start = scr.help.sel;
	help_screen_update();

	return 0;
}

#ifdef MOUSE
static void mouse_helpclick(int rx, int ry, void *data)
{
	if (active_screen != &help_screen_methods)
		return;
	if (ry < max_y / 2)
		scr.help.start -= 3;
	else
		scr.help.start += 3;
	help_screen_update();
}
#endif

ui_methods help_screen_methods = {
	help_screen_draw, help_screen_handler
};
