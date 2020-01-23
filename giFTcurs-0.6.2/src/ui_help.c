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
 * $Id: ui_help.c,v 1.85 2003/09/15 21:51:40 saturn Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "format.h"

static list help = LIST_INITIALIZER;
static int build_width = -1;
static int my_screen_number;
static const ui_methods help_screen_methods;
static char *generated_text;

/* Rules for the help text, so that line split works:
	- Don't use tab
	- Always highlight one word at a time only, or use non-breaking spaces */
const char helptext[] = N_("\
Welcome to giFTcurs!\n\
\n\
Key list:\n\
{%header:B}up{%prev}/{%header:B}down{%prev}        select item or scroll in a list\n\
{%header:B}PgUp{%prev}/{%header:B}PgDown{%prev}    scroll lists one screen at a time\n\
{%header:B}tab{%prev}            move to the next input field\n\
{%header:B}F1{%prev}             this help screen\n\
{%header:B}F2{%prev}             the main screen\n\
    {%header:B}enter{%prev}      download the highlighted file\n\
    {%header:B}left{%prev}/{%header:B}right{%prev} change sorting method\n\
    {%header:B}e{%prev}          expand/collapse search results\n\
    {%header:B}j{%prev}/{%header:B}k{%prev}        scroll information box\n\
    {%header:B}p{%prev}          change network in statistics box\n\
    {%header:B}u{%prev}          browse selected user\n\
    {%header:B}i{%prev}          ignore selected user, this session\n\
    {%header:B}shift{%prev}-{%header:B}i{%prev}    ignore selected user, all sessions\n\
{%header:B}F3{%prev}             the transfer screen\n\
    {%header:B}enter{%prev}/{%header:B}e{%prev}    expand/collapse transfer\n\
    {%header:B}left{%prev}/{%header:B}right{%prev} change sorting method\n\
    {%header:B}j{%prev}/{%header:B}k{%prev}        move window splitter\n\
{%header:B}F4{%prev}             the console screen\n\
    {%header:B}v{%prev}          toggle verbose mode\n\
    {%header:B}w{%prev}          toggle line wrap\n\
{%header:B}F5{%prev}             the settings screen\n\
{%header:B}F10{%prev}/{%header:B}q{%prev}          exit the program\n\
\n\
See giFTcurs(1) and giFTcurs.conf(5) for more information.");

/* TRANSLATORS: The authors' names are G"oran Weinholt and
   Christian H"aggstr"om, so if your charset has support for 'a' and 'o'
   with two dots over then please use those characters in their names. */
const char copyright[] = N_("\
Copyright (C) 2001, 2002, 2003 Goran Weinholt <weinholt@dtek.chalmers.se>.\n\
Copyright (C) 2003 Christian Haggstrom <chm@c00.info>.\n\
Our homepage is at http://www.nongnu.org/giftcurs/.");

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
	char *fnord;

	format_t tmp = format_compile(_(helptext));

	g_assert(tmp);
	fnord = format_expand(tmp, NULL, 99999, NULL);
	generated_text = g_strjoin("\n\n", fnord, _(copyright), license, NULL);
	g_free(fnord);

	help_screen_resize();

	my_screen_number = register_screen(&help_screen_methods);
}

static void help_screen_resize(void)
{
	list_free_entries(&help);
	wrap_lines(&help, generated_text, max_x - 4);
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
