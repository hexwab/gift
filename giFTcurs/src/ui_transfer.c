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
 * $Id: ui_transfer.c,v 1.248 2003/06/27 11:20:15 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "parse.h"
#include "screen.h"
#include "gift.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "ui_main.h"
#include "transfer.h"
#include "get.h"
#include "settings.h"
#include "format.h"

enum transfer_screen_fields {
	FIELD_DOWNLOADS,
	FIELD_UPLOADS,
	FIELD_MAX
};

static int active_field = FIELD_DOWNLOADS;
static format_t transfer_fmt, source_fmt, down_total_fmt, up_total_fmt;
static int my_screen_nr;
static const ui_methods transfer_screen_methods;

static void update_downloads(void);
static void update_uploads(void);
static void transfer_screen_draw(void);
static enum attr_type total_getattr(const transfer * t, char *key, attr_value * v);
static enum attr_type source_getattr(const source * h, char *key, attr_value * v);
static enum attr_type transfer_getattr(const transfer * t, const char *key, attr_value * v);
static char *pretty_transfer(const transfer * t);
static char *pretty_source(const source * t);

#ifdef MOUSE
/* Mouse callbacks */
static void mouse_listclick(int rx, int ry, void *data);
static void mouse_wheel(int rx, int ry, void *data);
#endif

static int UPLOAD_H = 10;

#define DOWNLOAD_H	(max_y - UPLOAD_H - 2 - show_buttonbar)
#define HELP_PAD	2

static void update_transfers(int which)
{
	if (which & 0100)
		update_downloads();
	if (which & 0200)
		update_uploads();
	refresh();
}

void transfer_screen_init(void)
{
	UPLOAD_H = atoi(get_config("set", "upload-height", "10"));

	my_screen_nr = register_screen(&transfer_screen_methods);
	transfers_init();

	downloads.update_ui = my_screen_nr | 0100;
	uploads.update_ui = my_screen_nr | 0200;

	transfer_class.getattr = (void *) transfer_getattr;
	source_class.getattr = (void *) source_getattr;
	transfer_class.pretty_line = (void *) pretty_transfer;
	source_class.pretty_line = (void *) pretty_source;

	transfer_fmt =
		format_get("transfer", _("{$expanded}"
								 " {filename}{space} | [{progress filesize transferred}{fixed:26}{space}"
								 "{if active}{transferred:bi}/{filesize:bi} {ratio:3}%"
								 " @ {bandwidth:bi}B/s{else}{status}{endif}"
								 "{space}{endfixed}{endprogress}]"));
	source_fmt =
		format_get("source",
				   _("  - {space}{user}@{net}"
					 " | [{progress filesize start transferred}{fixed:26}{space}"
					 "{if active}{bandwidth:bi}B/s{else}{status}{endif}"
					 "{space}{endfixed}{endprogress}]"));

	down_total_fmt =
		format_get("downloads_total",
				   _("{progress filesize transferred}"
					 "{if disk_free<2000000000}Disk free: {disk_free:bi}B{endif}"
					 "{space}"
					 "Total: {transferred:bi}/{filesize:bi} at {bandwidth:bi}B/s"
					 "{if eta} {eta:t}{endif}{endprogress}"));
	up_total_fmt =
		format_get("uploads_total",
				   _("{progress filesize transferred}{space}"
					 "Total: {transferred:bi}/{filesize:bi} at {bandwidth:bi}B/s"
					 "{if eta} {eta:t}{endif}{endprogress}"));
	g_assert(transfer_fmt);
	g_assert(source_fmt);
	g_assert(down_total_fmt);
	g_assert(up_total_fmt);
}

static void transfer_screen_destroy(void)
{
	g_strfreev(downloads.sort_methods);
	g_strfreev(uploads.sort_methods);

	tree_destroy_all(&downloads);
	tree_destroy_all(&uploads);
	format_unref(transfer_fmt);
	format_unref(source_fmt);
	format_unref(down_total_fmt);
	format_unref(up_total_fmt);

	uploads.update_ui = -1;
	downloads.update_ui = -1;
}

static gboolean at_bottom(transfer_tree * snafu)
{
	return tree_flat(snafu)->sel == tree_flat(snafu)->num - 1;
}

static int transfer_screen_handler(int key)
{
	transfer *t;
	source *s = NULL;
	transfer_tree *active_tree;
	int height;
	void (*update_func) (void);
	int i;

	switch (key) {
	case '\t':
	case KEY_BTAB:
		/* when we have more than two fields, take care of shift-tab here */
		active_field++;
		if (active_field >= FIELD_MAX)
			active_field -= FIELD_MAX;
		transfer_screen_draw();
		refresh();
		return 1;
	case 'C':
	case 'c':
		tree_filter(&downloads, (void *) transfer_alive);
		tree_filter(&uploads, (void *) transfer_alive);
		update_downloads();
		update_uploads();
		refresh();
		return 1;
	case 'K':
	case 'k':
		if (DOWNLOAD_H > 3) {
			UPLOAD_H++;
			set_config_int("set", "upload-height", UPLOAD_H, 1);
			transfer_screen_draw();
			refresh();
		}
		return 1;
	case 'J':
	case 'j':
		if (UPLOAD_H > 3) {
			UPLOAD_H--;
			set_config_int("set", "upload-height", UPLOAD_H, 1);
			transfer_screen_draw();
			refresh();
		}
		return 1;
	case 'S':
		/* Search for new sources on all downloads at once. */
		for (i = 0; i < tree_children(&downloads)->num; i++) {
			t = list_index(tree_children(&downloads), i);
			if (t->transferred != t->filesize)
				download_search(t);
		}
		update_downloads();
		refresh();
		return 1;
	}

	/* This is getting clearer, but still ugly... */
	if (active_field == FIELD_DOWNLOADS) {
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
	list_check_values(tree_flat(active_tree), height - 3);

	t = list_selected(tree_flat(active_tree));
	if (INSTANCEOF(t, source)) {
		s = (source *) t;
		t = tree_parent(s);
	}

	switch (key) {
		int expand_mode, pos;
		const char *method;

	case 'P':
	case 'p':
		if (transfer_alive(t))
			transfer_suspend(t);
		update_func();
		refresh();
		return 1;
	case 'E':
		expand_mode = 0;
		/* see if all transfers are expanded, if so do unexpand */
		for (i = 0; i < tree_children(active_tree)->num; i++) {
			transfer *tt = list_index(tree_children(active_tree), i);

			if (!tt->tnode.expanded) {
				expand_mode = 1;
				break;
			}
		}

		/* if we unexpand and a source is selected, select its parent */
		if (s && expand_mode == 0)
			tree_flat(active_tree)->sel = list_find(tree_flat(active_tree), t);

		for (i = 0; i < tree_children(active_tree)->num; i++) {
			t = list_index(tree_children(active_tree), i);
			if (t->tnode.expanded != expand_mode) {
				pos = tree_hold(t);
				t->tnode.expanded = expand_mode;
				tree_release(t, pos);
				active_tree->tnode.tnode.dirty = 1;
			}
		}
		update_func();
		refresh();
		return 1;
	case 'e':
	case '-':
	case '+':
	case KEY_ENTER:
		pos = tree_hold(t);
		if (s)
			tree_flat(active_tree)->sel = list_find(tree_flat(active_tree), t);
		if (key == '+' || key == '-')
			t->tnode.expanded = key == '+';
		else
			t->tnode.expanded = !t->tnode.expanded;
		tree_release(t, pos);
		active_tree->tnode.tnode.dirty = 1;
		update_func();
		refresh();
		return 1;
	case 's':
		/* Issue a new search if one is not already running. Also
		   don't search if the download is finished. */
		if (active_tree == &uploads)
			break;
		if (t->transferred != t->filesize) {
			download_search(t);
			update_func();
			refresh();
		}
		return 1;
	case 'T':
	case 't':
		if (s) {
			if (active_tree != &downloads)
				break;
			if (at_bottom(active_tree)
				|| INSTANCEOF(below_selected(tree_flat(active_tree)), transfer))
				tree_flat(active_tree)->sel--;
			else
				tree_flat(active_tree)->sel++;
			source_cancel(t, s);
		} else if (transfer_alive(t)) {
			if (active_tree == &downloads && key == 't') {
				/* This can prevent unwanted cancellations,
				 * at least until you get used to press shift-t */
				unsigned int oops_limit = atoi(get_config("set", "confirm-cancel", "0"));

				if (oops_limit && t->transferred >= oops_limit) {
					g_message(_
							  ("Please confirm deletion of %sB downloaded data by pressing shift-t"),
							  humanify(t->transferred));
					return 1;
				}
			}
			transfer_cancel(t);
		} else {
			if (at_bottom(active_tree))
				tree_flat(active_tree)->sel--;
			else
				tree_flat(active_tree)->sel++;
			transfer_forget(t);
		}
		update_func();
		refresh();
		return 1;
	case 'U':
	case 'u':
		if (s) {
			start_browse(s->user, s->node);
			update_func();
			refresh();
		}
		return 1;
	case KEY_LEFT:
	case KEY_RIGHT:
		if (s)
			method = source_change_sort_method(key == KEY_RIGHT ? 1 : -1);
		else
			method = transfer_change_sort_method(active_tree, key == KEY_RIGHT ? 1 : -1);
		update_func();
		if (s)
			g_message(_("Source sorting order changed to %s."), method);
		else
			g_message(_("Transfer sorting order changed to %s."), method);
		return 1;
	default:
		if (ui_list_handler(tree_flat(active_tree), key, height - 2)) {
			update_func();
			refresh();
			return 1;
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
	mouse_register(0, 0, max_x, DOWNLOAD_H, BUTTON1_PRESSED, mouse_listclick, tree_flat(&downloads),
				   0);
	mouse_register(0, 0, max_x, DOWNLOAD_H, BUTTON_UP, mouse_wheel, GINT_TO_POINTER(0 | 2), 0);
	mouse_register(0, 0, max_x, DOWNLOAD_H, BUTTON_DOWN, mouse_wheel, GINT_TO_POINTER(1 | 2), 0);
	mouse_register(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, BUTTON1_PRESSED, mouse_listclick,
				   tree_flat(&uploads), 0);
	mouse_register(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, BUTTON_UP, mouse_wheel, GINT_TO_POINTER(0),
				   0);
	mouse_register(0, DOWNLOAD_H + 1, max_x, UPLOAD_H, BUTTON_DOWN, mouse_wheel, GINT_TO_POINTER(1),
				   0);

	/* Predict the length of the help stuff on the bottom in order to center it. */
	if (len == -1)
		for (i = 0; i < G_N_ELEMENTS(help); i++)
			len += 1 + 3 + strlen(_(help[i].desc)) + HELP_PAD;

	x1 = len < max_x ? (max_x - len) / 2 : 0;

	move(DOWNLOAD_H, 0);
	hline(' ', max_x);

	for (i = 0; i < G_N_ELEMENTS(help) && x1 < max_x; i++) {
		char buf[100];

		w1 = g_snprintf(buf, sizeof buf, "\vB%c\va - %s", help[i].key, _(help[i].desc));
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

	update_uploads();
	update_downloads();
}

static void draw_transfers(transfer_tree * tr, int y, int height, int hilight, format_t format)
{
	int i;
	int fwidth = max_x - 40;
	transfer total;
	char *buf;

	memset(&total, 0, sizeof total);

	if (fwidth < 8)
		fwidth = 8;

	if (tr->tnode.tnode.dirty)
		tree_flatten(tr);
	list_check_values(tree_flat(tr), height - 3);
	draw_list_pretty(2, y, max_x - 4, height - 3, hilight, tree_flat(tr));

	for (i = 0; i < tree_children(tr)->num; i++) {
		transfer *t = list_index(tree_children(tr), i);

		if (transfer_alive(t) && t->transferred) {
			total.filesize += t->filesize;
			total.bw.bandwidth += t->bw.bandwidth;
			total.transferred += t->transferred;
		}
	}

	buf = format_expand(format, (getattrF) total_getattr, max_x - 4, &total);
	draw_fmt_str(2, y + height - 3, max_x - 4, 0, buf);
	g_free(buf);
}

static void update_downloads(void)
{
	draw_transfers(&downloads, 1, DOWNLOAD_H, active_field == FIELD_DOWNLOADS, down_total_fmt);
}

static void update_uploads(void)
{
	draw_transfers(&uploads, DOWNLOAD_H + 2, UPLOAD_H, active_field == FIELD_UPLOADS, up_total_fmt);
}

#ifdef MOUSE
static void mouse_listclick(int rx, int ry, void *data)
{
	list *foo = data;
	int h = 0, prevfield = active_field;

	g_assert(active_screen == my_screen_nr);

	if (foo == tree_flat(&downloads)) {
		h = DOWNLOAD_H;
		active_field = FIELD_DOWNLOADS;
	} else if (foo == tree_flat(&uploads)) {
		h = UPLOAD_H;
		active_field = FIELD_UPLOADS;
	} else
		g_assert_not_reached();

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

	if (active_field != prevfield)
		transfer_screen_draw();
	else if (data == tree_flat(&downloads))
		update_downloads();
	else
		update_uploads();
	refresh();
}

static void mouse_wheel(int rx, int ry, void *data)
{
	int dir = GPOINTER_TO_INT(data);
	transfer_tree *foo;
	int h;

	g_assert(active_screen == my_screen_nr);

	if (dir & 2) {
		foo = &downloads;
		h = DOWNLOAD_H;
		active_field = FIELD_DOWNLOADS;
	} else {
		foo = &uploads;
		h = UPLOAD_H;
		active_field = FIELD_UPLOADS;
	}

	if (dir & ~2)
		tree_flat(foo)->sel += (h / 2) - 1;
	else
		tree_flat(foo)->sel -= (h / 2) - 1;

	list_check_values(tree_flat(foo), h - 1);
	transfer_screen_draw();
	refresh();
}
#endif

#define RETURN_INT(x) return v->intval = (x), ATTR_INT
#define RETURN_LONG(x) return v->longval = (x), ATTR_LONG
#define RETURN_STR(x) return v->string = (x), ATTR_STRING

/* This describes how to get meta data from a transfer */
static enum attr_type transfer_getattr(const transfer * t, const char *key, attr_value * v)
{
	if (!strcmp(key, "filename"))
		RETURN_STR(t->filename);
	if (!strcmp(key, "filesize"))
		RETURN_LONG(t->filesize);
	if (!strcmp(key, "bandwidth"))
		RETURN_INT(t->bw.bandwidth);
	if (!strcmp(key, "ratio"))
		RETURN_INT(t->filesize ? 100 * t->transferred / t->filesize : 0);
	if (!strcmp(key, "transferred"))
		RETURN_LONG(t->transferred);
	if (!strcmp(key, "searching"))
		RETURN_INT(!!t->search_id);
	if (!strcmp(key, "expanded"))
		RETURN_INT(t->tnode.expanded);
	if (!strcmp(key, "sources"))
		RETURN_INT(tree_children(t)->num);
	if (!strcmp(key, "status"))
		RETURN_STR(_(t->status));
	if (!strcmp(key, "active"))
		RETURN_INT(t->id && !t->paused);
	if (!strcmp(key, "eta"))
		RETURN_INT(t->bw.bandwidth ? (t->filesize - t->transferred) / t->bw.bandwidth : 0);
	if (!strcmp(key, "upload"))
		RETURN_INT(tree_parent(t) == &uploads);
	if (!strcmp(key, "download"))
		RETURN_INT(tree_parent(t) == &downloads);
	return ATTR_NONE;
}

static enum attr_type total_getattr(const transfer * t, char *key, attr_value * v)
{
	if (!strcmp(key, "disk_free"))
		RETURN_LONG(disk_free());
	return transfer_getattr(t, key, v);
}

/* This describes how to get meta data from a source */
static enum attr_type source_getattr(const source * h, char *key, attr_value * v)
{
	if (!strcmp(key, "user"))
		RETURN_STR(h->user);
	if (!strcmp(key, "url"))
		RETURN_STR(h->href);
	if (!strcmp(key, "start"))
		RETURN_INT(h->start);
	if (!strcmp(key, "transferred"))
		RETURN_INT(h->transmit);
	if (!strcmp(key, "bandwidth"))
		RETURN_INT(h->bw.bandwidth);
	if (!strcmp(key, "total"))
		RETURN_INT(h->total);
	if (!strcmp(key, "net")) {
		char *colon = strchr(h->href, ':');

		if (!colon)
			RETURN_STR("");
		v->strlen.len = colon - h->href;
		v->strlen.string = h->href;
		return ATTR_STRLEN;
	}
	/* We no more try to keep strings like "Queued (\d+)" localized. */
	if (!strcmp(key, "status"))
		RETURN_STR(h->status ? _(h->status) : _("Unknown status"));
	if (!strcmp(key, "active"))
		RETURN_INT(h->state == SOURCE_ACTIVE);
	if (!strcmp(key, "paused"))
		RETURN_INT(h->state == SOURCE_PAUSED);
	if (!strcmp(key, "queued"))
		RETURN_INT(h->state == SOURCE_QUEUED_LOCAL || h->state == SOURCE_QUEUED_REMOTE);
	if (!strcmp(key, "eta"))
		RETURN_INT(h->bw.bandwidth ? (h->total - h->transmit) / h->bw.bandwidth : 0);
	return transfer_getattr(tree_parent(h), key, v);
}

static char *pretty_transfer(const transfer * t)
{
	return format_expand(transfer_fmt, (getattrF) transfer_getattr, max_x - 4, t);
}

static char *pretty_source(const source * s)
{
	return format_expand(source_fmt, (getattrF) source_getattr, max_x - 4, s);
}

static const ui_methods transfer_screen_methods = {
	transfer_screen_draw,
	NULL,						/* hide */
	update_transfers,
	transfer_screen_handler,
	transfer_screen_destroy,
	N_("Transfers"),
};
