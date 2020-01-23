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
 * $Id: ui_help.c,v 1.82 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"

static list help = LIST_INITIALIZER;
static int build_width = -1;
static int my_screen_number;
static const ui_methods help_screen_methods;

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
    \vBp\va          change network in statistics box\n\
    \vBu\va          browse selected user\n\
    \vBi\va          ignore selected user, this session\n\
    \vBshift\va-\vBi\va    ignore selected user, all sessions\n\
\vBF3\va             the transfer screen\n\
    \vBenter\va/\vBe\va    expand/collapse transfer\n\
    \vBleft\va/\vBright\va change sorting method\n\
    \vBj\va/\vBk\va        move window splitter\n\
\vBF4\va             the console screen\n\
    \vBv\va          toggle verbose mode\n\
    \vBw\va          toggle line wrap\n\
\vBF5\va             the settings screen\n\
\vBF10\va/\vBq\va          exit the program\n\
\n\
See giFTcurs(1) and giFTcurs.conf(5) for more information.");

/* TRANSLATORS: The authors' names are G"oran Weinholt and
   Christian H"aggstr"om, so if your charset has support for 'a' and 'o'
   with two dots over then please use those characters in their names. */
const char copyright[] = N_("\
Copyright (C) 2001, 2002, 2003 Goran Weinholt <weinholt@dtek.chalmers.se>.\n\
Copyright (C) 2003 Christian Haggstrom <chm@c00.info>.\n\
Our homepage is at http://giFTcurs.sourceforge.net/.");

const char license[] = "\
giFTcurs is free software; you can redistribute it and/or modify\
 it under the terms of the GNU General Public License as published by\
 the Free Software Foundation; either version 2 of the License, or\
 (at your option) any later version.\n\
\n\
giFTcurs is distributed in the hope that it will be useful,\
 but WITHOUT ANY WARRANTY; without even the implied warranty of\
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\
 GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\
 along with giFTcurs; if not, write to the Free Software Foundation,\
 Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.";

static void help_screen_resize(void);
static void help_screen_update(void);

#ifdef MOUSE
/* Mouse callback */
static void mouse_helpclick(int rx, int ry, void *data);
#endif

void help_screen_init(void)
{
	help_screen_resize();

	my_screen_number = register_screen(&help_screen_methods);
}

static void help_screen_resize(void)
{
	char *fnord;

	fnord = g_strjoin("\n\n", _(helptext), _(copyright), license, NULL);
	list_free_entries(&help);
	wrap_lines(&help, fnord, max_x - 4);
	g_free(fnord);
	build_width = max_x;
}

static void help_screen_destroy(void)
{
	list_free_entries(&help);
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

	if (max_x != build_width)
		help_screen_resize();
	list_check_values_simple(&help, height);
	/* This list is special in the aspect that it has no selected item.
	 * negative help.sel tells draw_list to place the diamond right. */
	help.sel = -1;
	draw_list(2, 1, max_x - 4, height, FALSE, &help);
	refresh();
}

static int help_screen_handler(int key)
{
	int ret;

	help.sel = help.start;
	ret = ui_list_handler(&help, key, max_y - 3 - show_buttonbar);
	help.start = help.sel;
	help_screen_update();

	return ret;
}

#ifdef MOUSE
static void mouse_helpclick(int rx, int ry, void *data)
{
	g_assert(active_screen == my_screen_number);

	if (ry < max_y / 2)
		help.start -= 3;
	else
		help.start += 3;
	help_screen_update();
}
#endif

static const ui_methods help_screen_methods = {
	help_screen_draw,
	NULL,						/* hide */
	NULL,						/* update */
	help_screen_handler,
	help_screen_destroy,
	N_("Help"),
};
