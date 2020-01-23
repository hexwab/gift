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
 * $Id: ui_transfer.c,v 1.211 2002/12/01 23:29:24 chnix Exp $
 */
#include "giftcurs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "screen.h"
#include "gift.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "ui_main.h"
#include "ui_transfer.h"
#include "transfer.h"
#include "get.h"
#include "poll.h"
#include "settings.h"
#include "format.h"

enum transfer_screen_fields {
	FIELD_DOWNLOADS,
	FIELD_UPLOADS,
	FIELD_MAX
};

static struct {
	int active_field;
	format_t transfer_fmt;
	format_t source_fmt;
	format_t down_total_fmt;
	format_t up_total_fmt;
} scr = {
FIELD_DOWNLOADS, NULL, NULL};

static void update_downloads(void);
static void update_uploads(void);
static void transfer_screen_draw(void);

#ifdef MOUSE
/* Mouse callbacks */
static void mouse_listclick(int rx, int ry, void *data);
static void mouse_wheel(int rx, int ry, void *data);
#endif

static int UPLOAD_H = 10;

#define DOWNLOAD_H	(max_y - UPLOAD_H - 2 - show_buttonbar)
#define HELP_PAD	2

static void update_downloads_if_focus(void)
{
	if (active_screen == &transfer_screen_methods) {
		tree_flatten(&downloads);
		update_downloads();
		refresh();
	}
}

static void update_uploads_if_focus(void)
{
	if (active_screen == &transfer_screen_methods) {
		tree_flatten(&uploads);
		update_uploads();
		refresh();
	}
}

void transfer_screen_init(void)
{
	UPLOAD_H = atoi(get_config("set", "upload-height", "10"));

	transfers_init();
	uploads.update_ui = update_uploads_if_focus;
	downloads.update_ui = update_downloads_if_focus;

	scr.transfer_fmt =
		format_get("transfer", "{$expanded}"
				   " {filename}{space} | [{fixed:30}{progress filesize transferred}{space}"
				   "{if active}{transferred:bi}/{filesize:bi} {ratio:3}%"
				   " @ {bandwidth:bi}B/s{else}{status}{endif}" "{space}{endprogress}{endfixed}]");
	scr.source_fmt =
		format_get("source", "  - {space}{user}@{net}"
				   " | [{fixed:30}{progress filesize start transferred}{space}"
				   "{status}{space}{endprogress}{endfixed}]");

	/* Remember that total line uses kB for all sizes */
	scr.down_total_fmt =
		format_get("downloads_total",
				   "{progress filesize transferred}"
				   "{if disk_free<2000000000}Disk free: {disk_free:ki}B{endif}"
				   "{space}"
				   "Total: {transferred:ki}/{filesize:ki} at {bandwidth:ki}B/s"
				   "{if eta} {eta:t}{endif}{endprogress}");
	scr.up_total_fmt =
		format_get("uploads_total",
				   "{progress filesize transferred}{space}"
				   "Total: {transferred:ki}/{filesize:ki} at {bandwidth:ki}B/s"
				   "{if eta} {eta:t}{endif}{endprogress}");
	assert(scr.transfer_fmt);
	assert(scr.source_fmt);
	assert(scr.down_total_fmt);
	assert(scr.up_total_fmt);
}

void transfer_screen_destroy(void)
{
	tree_destroy_all(&downloads);
	tree_destroy_all(&uploads);
	format_unref(scr.transfer_fmt);
	format_unref(scr.source_fmt);
	format_unref(scr.down_total_fmt);
	format_unref(scr.up_total_fmt);
}

static int at_bottom(tree * snafu)
{
	return snafu->flat.sel == snafu->flat.num - 1;
}

static int transfer_screen_handler(int key)
{
	transfer *t;
	source *s = NULL;
	tree *active_tree;
	int height;
	void (*update_func) (void);
	int i;

	switch (key) {
	case '\t':
	case KEY_BTAB:
		/* when we have more than two fields, take care of shift-tab here */
		scr.active_field++;
		if (scr.active_field >= FIELD_MAX)
			scr.active_field -= FIELD_MAX;
		transfer_screen_draw();
		refresh();
		return 0;
	case 'C':
	case 'c':
		tree_filter(&downloads, (int (*)(void *)) transfer_alive);
		tree_filter(&uploads, (int (*)(void *)) transfer_alive);
		update_downloads();
		update_uploads();
		refresh();
		return 0;
	case 'K':
	case 'k':
		if (DOWNLOAD_H > 3) {
			UPLOAD_H++;
			set_config_int("set", "upload-height", UPLOAD_H, 1);
			transfer_screen_draw();
			refresh();
		}
		return 0;
	case 'J':
	case 'j':
		if (UPLOAD_H > 3) {
			UPLOAD_H--;
			set_config_int("set", "upload-height", UPLOAD_H, 1);
			transfer_screen_draw();
			refresh();
		}
		return 0;
	case 'S':
		/* Search for new sources on all downloads at once. */
		for (i = 0; i < downloads.top.num; i++) {
			t = list_index(&downloads.top, i);
			if (t->transferred != t->filesize)
				download_search(t);
		}
		update_downloads();
		refresh();
		break;
	}

	/* This is getting clearer, but still ugly... */
	if (scr.active_field == FIELD_DOWNLOADS) {
		active_tree = &downloads;
		height = DOWNLOAD_H;
		update_func = update_downloads;
	} else {
		active_tree = &uploads;
		height = UPLOAD_H;
		update_func = update_uploads;
	}
	if (tree_isempty(active_tree))
		return 0;
	list_check_values(&active_tree->flat, height - 3);

	t = list_selected(&active_tree->flat);
	if (IS_A(t, source)) {
		s = (source *) t;
		t = s->parent;
	}

	switch (key) {
		int expand_mode;

	case 'P':
	case 'p':
		if (transfer_alive(t))
			transfer_suspend(t);
		update_func();
		refresh();
		break;
	case 'E':
		expand_mode = 0;
		/* see if all transfers are expanded, if so do unexpand */
		for (i = 0; i < active_tree->top.num; i++) {
			transfer *tt = list_index(&active_tree->top, i);

			if (!tt->expanded) {
				expand_mode = 1;
				break;
			}
		}

		/* if we unexpand and a source is selected, select its parent */
		if (s && expand_mode == 0)
			active_tree->flat.sel = list_find(&active_tree->flat, t);

		for (i = 0; i < active_tree->top.num; i++) {
			t = list_index(&active_tree->top, i);
			if (t->expanded != expand_mode) {
				t->expanded = expand_mode;
				TOUCH(t);
			}
		}
		tree_flatten(active_tree);
		update_func();
		refresh();
		break;
	case 'e':
	case '-':
	case '+':
	case KEY_ENTER:
		if (s)
			active_tree->flat.sel = list_find(&active_tree->flat, t);
		if (key == '+' || key == '-')
			t->expanded = key == '+';
		else
			t->expanded = !t->expanded;
		TOUCH(t);
		tree_flatten(active_tree);
		update_func();
		refresh();
		break;
	case 's':
		/* Issue a new search if one is not already running. Also
		   don't search if the download is finished. */
		if (t->type != TRANSFER_DOWNLOAD)
			break;
		if (t->transferred != t->filesize) {
			download_search(t);
			update_func();
			refresh();
		}
		break;
	case 'T':
	case 't':
		if (s) {
			if (t->type == TRANSFER_DOWNLOAD) {
				if (at_bottom(active_tree) || IS_A(below_selected(&active_tree->flat), transfer))
					active_tree->flat.sel--;
				else
					active_tree->flat.sel++;
				source_cancel(t, s);
			}
			break;
		}
		if (transfer_alive(t))
			transfer_cancel(t);
		else {
			if (at_bottom(active_tree))
				active_tree->flat.sel--;
			else
				active_tree->flat.sel++;
			transfer_forget(t);
		}
		update_func();
		refresh();
		break;
	case 'U':
	case 'u':
		if (s) {
			start_browse(s->user, s->node);
			update_func();
			refresh();
		}
		break;
	default:
		if (ui_list_handler(&active_tree->flat, key, height - 2)) {
			update_func();
			refresh();
		}
	}
	return 0;
}

static void transfer_screen_draw(void)
{
    /* *INDENT-OFF* */
    static const struct {
        const int key; 
        const char *desc;
    } help[] = {
        {'S', N_("find more sources")},
        {'T', N_("kill transfer")},
        {'C', N_("clear finished")},
        {'P', N_("(un)pause")},
    };
    /* *INDENT-ON* */
	static int len = -1;
	static int formatted_xdim = -1;
	int x1, w1, i;

	curs_set(0);
	leaveok(stdscr, TRUE);

	/* Check that splitter has a sane value */
	if (DOWNLOAD_H < 3)
		UPLOAD_H = max_y - 6;
	if (UPLOAD_H < 0)
		UPLOAD_H = 3;

	draw_box(0, 0, max_x, DOWNLOAD_H, _("Downloads"), COLOR(COLOR_DOWNLOAD_BOX, FIELD_DOWNLOADS));
	draw_box(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, _("Uploads"),
			 COLOR(COLOR_UPLOAD_BOX, FIELD_UPLOADS));

	mouse_clear(0);
	mouse_register(0, 0, max_x, DOWNLOAD_H, BUTTON1_PRESSED, mouse_listclick, &downloads.flat, 0);
	mouse_register(0, 0, max_x, DOWNLOAD_H, BUTTON_UP, mouse_wheel, (void *) (0 | 2), 0);
	mouse_register(0, 0, max_x, DOWNLOAD_H, BUTTON_DOWN, mouse_wheel, (void *) (1 | 2), 0);
	mouse_register(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, BUTTON1_PRESSED, mouse_listclick,
				   &uploads.flat, 0);
	mouse_register(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, BUTTON_UP, mouse_wheel, (void *) 0, 0);
	mouse_register(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, BUTTON_DOWN, mouse_wheel, (void *) 1, 0);

	/* Predict the length of the help stuff on the bottom in order to center it. */
	if (len == -1)
		for (i = 0; i < buflen(help); i++)
			len += 1 + 3 + strlen(_(help[i].desc)) + HELP_PAD;

	x1 = len < max_x ? (max_x - len) / 2 : 0;

	move(DOWNLOAD_H, 0);
	hline(' ', max_x);

	for (i = 0; i < buflen(help) && x1 < max_x; i++) {
		char buf[100];

		w1 = snprintf(buf, sizeof buf, "\vB%c\va - %s", help[i].key, _(help[i].desc));
		w1 -= 4;				/* do not count invisible characters */
		draw_fmt_str(x1, DOWNLOAD_H, max_x - x1, 0, buf);
		mouse_register(x1, DOWNLOAD_H, w1, 1, BUTTON1_PRESSED, ui_mouse_simulate_keypress,
					   (void *) help[i].key, 0);
		x1 += w1 + HELP_PAD;
	}

	if (formatted_xdim != max_x) {
		/* invalidate cached preformatted text */
		tree_touch_all(&downloads);
		tree_touch_all(&uploads);
		formatted_xdim = max_x;
	}

	tree_flatten(&downloads);
	tree_flatten(&uploads);

	update_uploads();
	update_downloads();
}

static void draw_transfers(tree * tr, int y, int height, int hilight, format_t format)
{
	int i;
	int fwidth = max_x - 40;
	transfer total;
	char *buf;

	memset(&total, 0, sizeof total);

	if (fwidth < 8)
		fwidth = 8;

	/* This shouldn't be done here. I have moved this to less frequent places */
	//tree_flatten(tr);
	list_check_values(&tr->flat, height - 3);
	draw_list_pretty(2, y, max_x - 4, height - 3, hilight, &tr->flat);

	for (i = 0; i < tr->top.num; i++) {
		transfer *t = list_index(&tr->top, i);

		if (transfer_alive(t) && t->transferred) {
			total.filesize += t->filesize / 1024;
			total.bandwidth += t->bandwidth;
			total.transferred += t->transferred / 1024;
		}
	}

	total.bandwidth /= 1024;

	buf = format_transfer(format, &total, max_x - 4);
	draw_fmt_str(2, y + height - 3, max_x - 4, 0, buf);
	free(buf);
}

static void update_downloads(void)
{
	draw_transfers(&downloads, 1, DOWNLOAD_H, scr.active_field == FIELD_DOWNLOADS,
				   scr.down_total_fmt);
}

static void update_uploads(void)
{
	draw_transfers(&uploads, DOWNLOAD_H + 2, UPLOAD_H, scr.active_field == FIELD_UPLOADS,
				   scr.up_total_fmt);
}

#ifdef MOUSE
static void mouse_listclick(int rx, int ry, void *data)
{
	list *foo = data;
	int h = 0, prevfield = scr.active_field;

	assert(active_screen == &transfer_screen_methods);

	if (foo == &downloads.flat) {
		h = DOWNLOAD_H;
		scr.active_field = FIELD_DOWNLOADS;
	} else if (foo == &uploads.flat) {
		h = UPLOAD_H;
		scr.active_field = FIELD_UPLOADS;
	} else
		assert(0);

	if (ry == 0)
		foo->sel -= h;
	else if (ry == h + 1)
		foo->sel += h;
	else
		foo->sel = foo->start + ry - 1;

	/* Expand/collapse a transfer result. */
	if (rx == 2)
		transfer_screen_handler('e');

	list_check_values(foo, h);

	if (scr.active_field != prevfield)
		transfer_screen_draw();
	else if (data == &downloads.flat)
		update_downloads();
	else
		update_uploads();
	refresh();
}

static void mouse_wheel(int rx, int ry, void *data)
{
	int dir = (int) data;
	tree *foo;
	int h;

	assert(active_screen == &transfer_screen_methods);

	if (dir & 2) {
		foo = &downloads;
		h = DOWNLOAD_H;
		scr.active_field = FIELD_DOWNLOADS;
	} else {
		foo = &uploads;
		h = UPLOAD_H;
		scr.active_field = FIELD_UPLOADS;
	}

	if (dir & ~2)
		foo->flat.sel += (h / 2) - 1;
	else
		foo->flat.sel -= (h / 2) - 1;

	list_check_values(&foo->flat, h - 1);
	transfer_screen_draw();
	refresh();
}
#endif

/* This function is used to create an entry in the lists in the transfer
 * screen. transfer_touch() is called to free t->pretty and set it to
 * NULL, thus we don't have to do anything if t->pretty is set.
 */
static char *pretty_transfer(transfer * t)
{
	if (!t->pretty)
		t->pretty = format_transfer(scr.transfer_fmt, t, max_x - 4);
	return t->pretty;
}

static char *pretty_source(source * s)
{
	if (!s->pretty)
		s->pretty = format_source(scr.source_fmt, s, max_x - 4);
	return s->pretty;
}

static list *transfer_expansion(transfer * t)
{
	return t->expanded ? &t->sourcen : NULL;
}

rendering transfer_methods = {
	(char *(*)(void *)) pretty_transfer,
	(list * (*)(void *)) transfer_expansion,
	(void (*)(void *)) transfer_destroy,
	(void (*)(void *)) transfer_touch,
};
rendering source_methods = {
	(char *(*)(void *)) pretty_source,
	NULL,
	(void (*)(void *)) source_destroy,
	(void (*)(void *)) source_touch,
};
ui_methods transfer_screen_methods = {
	transfer_screen_draw, transfer_screen_handler
};
