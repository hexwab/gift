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
 * $Id: ui_settings.c,v 1.81 2003/06/30 08:10:57 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gift.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "get.h"
#include "transfer.h"
#include "misc.h"

#include "settings.h"

static void settings_screen_draw(void);
static void settings_screen_update(void);

#ifdef MOUSE
/* Mouse callbacks */
static void mouse_colorclick(int rx, int ry, void *data);
static void mouse_buttonclick(int rx, int ry, void *data);
#endif

/* The color theme box has a variable height. */
#define TOOL_BOX_H		3
#define TOOL_BOX_Y		(max_y - TOOL_BOX_H - 1 - show_buttonbar)
#define COLOR_BOX_H		TOOL_BOX_Y
#define OPT_BOX_Y		(COLOR_BOX_H)
#define LIST_H			(COLOR_BOX_H - 6)
#define ITEM_W			(max_x / 3) - 6
#define BOX1_X			2
#define BOX2_X			(ITEM_W + 7)
#define BOX3_X			(2 * (ITEM_W + 6))

enum settings_screen_fields {
	/* Color stuff */
	FIELD_COLOR_ITEM,
	FIELD_COLOR_FG,
	FIELD_COLOR_BG,
	FIELD_SAVE_BUTTON,
	FIELD_RESET_BUTTON,

	/* The other stuff */
	FIELD_SYNC,
	FIELD_TOGGLE,
	FIELD_MAX
};

static const ui_methods settings_screen_methods;
static int active_field = FIELD_COLOR_ITEM;
static int my_screen_nr;

static list fgcolors = LIST_INITIALIZER;
static list bgcolors;			/* This shares data with fgcolors */
static list items = LIST_INITIALIZER;

void settings_screen_init(void)
{
	unsigned int i;

	for (i = 0; i < G_N_ELEMENTS(colornames); i++)
		list_append(&fgcolors, _(colornames[i]));
	bgcolors = fgcolors;

	for (i = 0; i < ITEMS_NUM; i++)
		list_append(&items, item_name(i));

	my_screen_nr = register_screen(&settings_screen_methods);

	/* update show/hide shares automatically */
	share_register(my_screen_nr);
}

static void settings_screen_destroy(void)
{
	list_remove_all(&items);
	list_remove_all(&fgcolors);
}

static int settings_screen_handler(int key)
{
	if (key == '\t' || key == KEY_BTAB || key == KEY_RIGHT || key == KEY_LEFT) {
		int dir;

		dir = key == KEY_LEFT || key == KEY_BTAB ? -1 : 1;

		active_field += dir;
		if (active_field == FIELD_MAX)
			active_field = 0;
		if (active_field < 0)
			active_field += FIELD_MAX;
		settings_screen_draw();
		refresh();
		return 1;
	}

	switch (active_field) {
	case FIELD_COLOR_ITEM:
		if (ui_list_handler(&items, key, LIST_H)) {
			settings_screen_update();
			refresh();
			return 1;
		}
		break;
	case FIELD_COLOR_FG:
		if (ui_list_handler(&fgcolors, key, LIST_H)) {
			init_pair(items.sel + 1, fgcolors.sel, bgcolors.sel);
			ui_draw();
			refresh();
			return 1;
		}
		break;
	case FIELD_COLOR_BG:
		if (ui_list_handler(&bgcolors, key, LIST_H)) {
			init_pair(items.sel + 1, fgcolors.sel, bgcolors.sel);
			ui_draw();
			refresh();
			return 1;
		}
		break;
	case FIELD_SAVE_BUTTON:
		if (PRESSED(key)) {
			if (save_color_config() == 0)
				g_message(_("Color theme saved."));
			return 1;
		}
		break;
	case FIELD_RESET_BUTTON:
		if (PRESSED(key)) {
			set_default_colors();
			ui_draw();
			refresh();
			g_message(_("Default color theme loaded."));
			return 1;
		}
		break;
	case FIELD_SYNC:
		if (PRESSED(key)) {
			sharing_sync();
			return 1;
		}
		break;
	case FIELD_TOGGLE:
		if (PRESSED(key)) {
			sharing_toggle();
			return 1;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void settings_screen_draw(void)
{
	g_assert(active_screen == my_screen_nr);

	curs_set(0);
	leaveok(stdscr, TRUE);
	mouse_clear(0);

	/* Color theme */
	draw_box(0, 0, max_x, COLOR_BOX_H, _("Color Theme"),
			 COLOR_PAIR(COLOR_SEARCH_BOX) | (active_field < FIELD_SYNC ? A_BOLD : 0));
	clear_area(1, 1, max_x - 2, COLOR_BOX_H - 2);

	/* Draw the boxen */
	draw_box(BOX1_X, 1, ITEM_W + 4, LIST_H + 2, _("Item"),
			 COLOR(COLOR_SEARCH_BOX, FIELD_COLOR_ITEM));
	draw_box(BOX2_X, 1, ITEM_W + 4, LIST_H + 2, _("Foreground"),
			 COLOR(COLOR_SEARCH_BOX, FIELD_COLOR_FG));
	draw_box(BOX3_X, 1, ITEM_W + 4, LIST_H + 2, _("Background"),
			 COLOR(COLOR_SEARCH_BOX, FIELD_COLOR_BG));

	mouse_register(BOX1_X, 1, ITEM_W + 4, LIST_H + 2, BUTTON1_PRESSED, mouse_colorclick, &items, 0);
	mouse_register(BOX2_X, 1, ITEM_W + 4, LIST_H + 2, BUTTON1_PRESSED, mouse_colorclick, &fgcolors,
				   0);
	mouse_register(BOX3_X, 1, ITEM_W + 4, LIST_H + 2, BUTTON1_PRESSED, mouse_colorclick, &bgcolors,
				   0);

	draw_button(max_x / 4, COLOR_BOX_H - 2, _("Save theme"),
				COLOR_BUTTON(COLOR_SEARCH_BOX, FIELD_SAVE_BUTTON, 1), mouse_buttonclick,
				(void *) FIELD_SAVE_BUTTON);
	draw_button(3 * max_x / 4, COLOR_BOX_H - 2, _("Load defaults"),
				COLOR_BUTTON(0, FIELD_RESET_BUTTON, 1), mouse_buttonclick,
				(void *) FIELD_RESET_BUTTON);

	/* The "Sharing" box */
	draw_box(0, TOOL_BOX_Y, max_x, TOOL_BOX_H, _("Sharing"),
			 COLOR_PAIR(COLOR_RESULT_BOX) | (active_field >= FIELD_SYNC ? A_BOLD : 0));
	clear_area(1, TOOL_BOX_Y + 1, max_x - 2, TOOL_BOX_H - 2);

	draw_button(max_x / 4, TOOL_BOX_Y + 1, _("Sync shares"),
				COLOR_BUTTON(COLOR_RESULT_BOX, FIELD_SYNC, 1), mouse_buttonclick,
				(void *) FIELD_SYNC);

	draw_button(3 * max_x / 4, TOOL_BOX_Y + 1,
				sharing <
				0 ? _("Unknown sharing status") : sharing ? _("Hide shares") : _("Show shares"),
				COLOR_BUTTON(COLOR_RESULT_BOX, FIELD_TOGGLE, sharing >= 0), mouse_buttonclick,
				(void *) FIELD_TOGGLE);

	settings_screen_update();
}

static void settings_screen_update(void)
{
	short fg, bg;

	/* Get the color values */
	pair_content(items.sel + 1, &fg, &bg);
	fgcolors.sel = fg, bgcolors.sel = bg;
	list_check_values(&items, LIST_H);
	list_check_values(&fgcolors, LIST_H);
	list_check_values(&bgcolors, LIST_H);

	/* Draw the lists */
	draw_list(BOX1_X + 2, 2, ITEM_W, LIST_H, TRUE, &items);
	draw_list(BOX2_X + 2, 2, ITEM_W, LIST_H, TRUE, &fgcolors);
	draw_list(BOX3_X + 2, 2, ITEM_W, LIST_H, TRUE, &bgcolors);

	refresh();
}

#ifdef MOUSE
static void mouse_colorclick(int rx, int ry, void *data)
{
	list *foo = data;

	g_assert(active_screen == my_screen_nr);

	if (active_field != FIELD_COLOR_ITEM && foo == &items)
		active_field = FIELD_COLOR_ITEM;
	else if (active_field != FIELD_COLOR_FG && foo == &fgcolors)
		active_field = FIELD_COLOR_FG;
	else if (active_field != FIELD_COLOR_BG && foo == &bgcolors)
		active_field = FIELD_COLOR_BG;

	if (ry == 0)
		foo->sel -= LIST_H;
	else if (ry == LIST_H + 1)
		foo->sel += LIST_H;
	else
		foo->sel = foo->start + ry - 1;

	if (foo != &items) {
		list_check_values(&fgcolors, LIST_H);
		list_check_values(&bgcolors, LIST_H);
		init_pair(items.sel + 1, fgcolors.sel, bgcolors.sel);
	} else {
		list_check_values(&items, LIST_H);
	}

	settings_screen_draw();
	refresh();
}

static void mouse_buttonclick(int rx, int ry, void *data)
{
	int field = GPOINTER_TO_INT(data);

	g_assert(active_screen == my_screen_nr);

	active_field = field;
	settings_screen_handler(KEY_ENTER);
	settings_screen_draw();
	refresh();
}
#endif

static const ui_methods settings_screen_methods = {
	settings_screen_draw,
	NULL,						/* hide */
	(void *) settings_screen_draw,	/* update */
	settings_screen_handler,
	settings_screen_destroy,
	N_("Settings"),
};
