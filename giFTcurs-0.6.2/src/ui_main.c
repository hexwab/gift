/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 G�ran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian H�ggstr�m <chm@c00.info>
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
 * $Id: ui_main.c,v 1.352 2003/11/22 12:21:15 saturn Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parse.h"
#include "protocol.h"
#include "gift.h"
#include "misc.h"
#include "screen.h"
#include "ui_mouse.h"
#include "ui.h"
#include "ui_draw.h"
#include "ui_main.h"
#include "ui_input.h"
#include "get.h"
#include "format.h"
#include "settings.h"
#include "protocol.h"

#define HITS ((query*)list_selected(&queries))

enum {
	FIELD_SEARCH_TERM,
	FIELD_SEARCH_REALM,
	FIELD_SEARCH_BUTTON,
	FIELD_STOP_BUTTON,
	FIELD_RESULT_LIST,
	FIELD_MAX,
	FIELD_STATUSBAR,
};

static int active_field = FIELD_SEARCH_TERM;
static list search_realm = LIST_INITIALIZER;
static char *last_search_term = NULL;
static format_t hit_fmt;
static format_t subhit_fmt;
static ui_input search_input = { 0 };
static ui_input slash_input = { 0 };
static ui_input realm_input = { 0 };

static const ui_methods main_screen_methods;

static void main_screen_draw(void);
static void main_screen_update_stats(void);
static void main_screen_update_results(void);
static void main_screen_update_info(void);
static void main_screen_update_searchbox(void);
static int detach_handle(ntree *data, void *);
static void download_add_handle(ntree *the_tree, void *);
static void incoming_search_item(void);
static char *hit_present(hit *foo);
static enum attr_type hit_getattr(const hit *h, const char *key, attr_value * v);
static char *subhit_present(subhit *foo);
static enum attr_type subhit_getattr(const subhit *h, const char *key, attr_value * v);
static enum attr_type stats_getattr(const gift_stat * s, const char *key, attr_value * v);

static guint my_screen_nr;

#ifdef MOUSE
/* Mouse event callbacks */
static void mouse_infoclick(int rx, int ry, void *data);
static void mouse_resultclick(int rx, int ry, void *data);
static void mouse_resultdoubleclick(int rx, int ry, void *data);
static void mouse_buttonclick(int rx, int ry, void *data);
static void mouse_inputclick(int rx, int ry, void *data);
static void mouse_wheel(int rx, int ry, void *data);
#endif

/* The stat box size is fixed. Other boxes may vary in size. */
#define STAT_BOX_W		26
#define STAT_BOX_H		6
#define STAT_HEADLEN	12
#define STAT_COL1		(max_x - STAT_BOX_W)
#define STAT_COL2		(STAT_COL1 + STAT_HEADLEN + 2)
#define SEARCH_BOX_W	(STAT_COL1 - 2)
#define RESULT_H		(max_y - 3 - STAT_BOX_H - show_buttonbar)

static void main_screen_update(int entities)
{
	if (entities & 0100)
		main_screen_update_stats();
	if (entities & 0200)
		incoming_search_item();
	refresh();
}

void main_screen_init(void)
{
	unsigned int i;
	static const char *realms[] = {
		N_("everything"), N_("audio"), N_("video"),
		N_("images"), N_("text documents"), N_("software"),
		N_("user"), N_("hash"), N_("list my shares"),
	};

	hit_class.pretty_line = (void *) hit_present;
	hit_class.getattr = (getattrF) hit_getattr;
	subhit_class.pretty_line = (void *) subhit_present;
	subhit_class.getattr = (getattrF) subhit_getattr;

	list_initialize(&search_realm);

	my_screen_nr = register_screen(&main_screen_methods);

	gift_search_init();
	misc_init();

	for (i = 0; i < G_N_ELEMENTS(realms); i++)
		list_append(&search_realm, _(realms[i]));
	search_realm.sel = 0;
	active_field = 0;

	/* initialize the ui_input's */
	ui_input_init(&search_input);
	ui_input_init(&slash_input);
	ui_input_init(&realm_input);
	ui_input_assign(&realm_input, list_selected(&search_realm));

	/* Load the default formats */
	format_load("$expanded", "{if expanded}-{else}+{endif}");
	format_load("$availability", "{if 2<availability}{%hit-good:B}"
				"{elif availability<1}{%hit-bad:B}{else}{%header:B}{endif}");
	format_load("$hit_pfx", "{if downloading}!{else}{$expanded}{endif}"
				"{$availability}{availability:2}{%standard}/"
				"{if downloading<1}{%header:B}{endif}{filesize:bi}{%standard}");
	hit_fmt = format_get("hit", "{$hit_pfx} {filename}{space}");
	subhit_fmt =
		format_get("subhit", " - {if 128<availability}"
				   "Inf{else}{availability:3}{endif}   {space}{user}{%header:B}@"
				   "{%standard}{fixed:20%}{net}{space}{endfixed}");

	format_load("browse", "{if downloading<1}{%header:B}{endif}{filesize:bi}"
				"{%standard} {path:}/{filename}{space}"
				"{if bitrate} {bitrate:b}bps{endif}{if duration} {duration:t}{endif}");

	format_load("image", "{$hit_pfx} {filename} {space}{if width}{width} x {height}{endif}");
	format_load("audio", "{$hit_pfx} {filename}{space}"
				"{if bitrate} {bitrate:b}bps{endif}{if duration} {duration:t}{endif}");
	format_load("video", "{$hit_pfx} {filename}{space}"
				"{if resolution} {resolution}{endif}{if duration} {duration:t}{endif}");

	g_assert(hit_fmt);
	g_assert(subhit_fmt);

	gift_register("ADDDOWNLOAD", (EventCallback) download_add_handle, NULL);
	gift_register("DETACH", (EventCallback) detach_handle, NULL);
}

static int detach_handle(ntree *data, void *always_null)
{
	/* Make sure the screen reflects the state after an unexpected detach. */
	ui_update(my_screen_nr | 0200);
	return 0;
}

static void main_screen_destroy(void)
{
	gift_unregister(NULL, (EventCallback) detach_handle, NULL);
	ui_input_deinit(&search_input);
	ui_input_deinit(&slash_input);
	ui_input_deinit(&realm_input);
	list_remove_all(&search_realm);
	gift_search_cleanup();
	format_unref(hit_fmt);
	format_unref(subhit_fmt);
	hit_fmt = NULL;
	subhit_fmt = NULL;

	misc_destroy();
}

static void incoming_search_item(void)
{
	if (active_field < FIELD_RESULT_LIST)
		main_screen_update_searchbox();
	main_screen_update_results();
}

static query *prepare_search(int browse)
{
	int i;
	query *q;

	/* add query to history if it's not already there */
	/* skip the last dummy entry */
	for (i = 0; i < queries.num - 1; i++) {
		q = list_index(&queries, i);

		if (!strcmp(search_input.str->str, q->search_term) && search_realm.sel == q->realm) {
			queries.sel = i;
			gift_query_stop(q);
			/* This line is commented out because some protocols return more
			   search results when an identical search is issued again. */
			/* gift_query_clear(q); */
			return q;
		}
	}

	/* create a new query structure and append to list */
	q = new_query(search_input.str->str, search_realm.sel);
	q->formatting = browse ? format_get("browse", NULL) : NULL;
	q->callback = my_screen_nr | 0200;

	list_insert(&queries, q, i);
	queries.sel = i;

	return q;
}

/* This function may be called from other screens too...
 * be careful with updates of screen */
void start_browse(const char *user, const char *node)
{
	query *q;
	ntree *packet = NULL;

	ui_input_assign(&search_input, user);
	search_realm.sel = 6;		/* BROWSE */
	q = prepare_search(1);

	interface_append(&packet, "query", user);
	if (node)
		interface_append(&packet, "node", node);
	q->id = gift_write_register(&packet, "BROWSE", (EventCallback) search_result_item_handler, q);
	if (!q->id)
		g_message(_("Couldn't start search!"));
	else
		g_message(_("Retrieving file list..."));
	ui_update(my_screen_nr | 0200);
}

static void start_search_pressed(void)
{
	/* *INDENT-OFF* */
	static const struct item_t {
		const char *realm;
		const char *command;
		int format;
	} names[] = {
		{NULL, "SEARCH", 0},
		{"audio", "SEARCH", 0},
		{"video", "SEARCH", 0},
		{"image", "SEARCH", 0},
		{"text", "SEARCH", 0},
		{"application", "SEARCH", 0},
		{NULL, "BROWSE", 1},
		{NULL, "LOCATE", 0},
		{NULL, "SHARES", 1},
	};
	/* *INDENT-ON* */
	const struct item_t *t;
	query *q;
	ntree *packet = NULL;
	char *includes = NULL, *excludes = NULL, *protocols = NULL;

	t = names + search_realm.sel;
	q = prepare_search(t->format);

	parse_typed_query(q->search_term, &includes, &excludes, &protocols);
	interface_append(&packet, "query", includes);
	if (excludes && excludes[0])
		interface_append(&packet, "exclude", excludes);
	if (t->realm)
		interface_append(&packet, "realm", t->realm);
	if (protocols && protocols[0])
		interface_append(&packet, "protocol", protocols);

	q->id = gift_write_register(&packet, t->command, (EventCallback) search_result_item_handler, q);
	if (!q->id) {
		g_message(_("Couldn't start search!"));
	} else {
		g_message(_("Searching..."));
	}
	g_free(includes);
	g_free(excludes);
	g_free(protocols);
	incoming_search_item();
	refresh();
}

static void slash_find_next(int dir)
{
	query *q = HITS;
	int prev, i;
	char *pattern = slash_input.str->str;
	hit *h;
	list *l;

	if (!pattern || !pattern[0] || !pattern[1]) {
		g_message(_("No previous search pattern."));
		return;
	}
	if (tree_isempty(q)) {
		g_message(_("No hits to search on."));
		return;
	}

	if (pattern[0] == '?')
		dir = -dir;
	pattern++;

	h = list_selected(tree_flat(q));
	if (INSTANCEOF(h, subhit))
		 h = tree_parent(h);

	l = tree_children(q);

	prev = list_find(l, h);

	for (i = prev;;) {
		i += dir;
		if (i >= l->num)
			i = 0;
		if (i < 0)
			i = l->num - 1;
		if (i == prev) {
			g_message(_("Pattern not found: '%s'."), pattern);
			return;
		}
		h = list_index(l, i);
		if (stristr(h->filename, pattern)) {
			tree_select_top(q, i);
			show_status(slash_input.str->str);
			return;
		}
	}
}

/* returns 0 - can't, 1 - can clear, 2 - can stop */
static int can_stop_search(void)
{
	query *q;

	q = list_selected(&queries);
	if (q->id)
		return 2;
	return !tree_isempty(q);
}

/* returns 0 if key was not catched */
static int main_screen_handler(int key)
{
	g_assert(active_screen == my_screen_nr);

	if (key == '\t' || key == KEY_BTAB) {
		int direction = key == KEY_BTAB ? -1 : 1;

		for (;;) {
			active_field += direction;
			if (active_field >= FIELD_MAX)
				active_field -= FIELD_MAX;
			if (active_field < 0)
				active_field += FIELD_MAX;
			if (active_field == FIELD_STOP_BUTTON && !can_stop_search())
				continue;
			if (active_field == FIELD_RESULT_LIST && tree_isempty(HITS))
				continue;
			break;
		}

		if (active_field == FIELD_RESULT_LIST && tree_flat(HITS)->sel == -1)
			tree_flat(HITS)->sel++;

		main_screen_draw();
		refresh();
		return 1;
	}

	switch (active_field) {
		hit *h;
		subhit *sh;
		query *q;

	case FIELD_SEARCH_TERM:
		if (!ui_input_handler(&search_input, key))
			if (ui_list_handler(&queries, key, 5)) {
				q = list_selected(&queries);

				ui_input_assign(&search_input, q->search_term);
				search_realm.sel = q->realm;
				ui_input_assign(&realm_input, list_selected(&search_realm));
				incoming_search_item();
			}
		/* Check values */
		ui_input_validate(&search_input);
		if (key == KEY_ENTER && search_input.str->len) {
			active_field = FIELD_SEARCH_BUTTON;
			start_search_pressed();
			main_screen_update_results();
		}

		main_screen_update_searchbox();
		refresh();
		return 1;
	case FIELD_SEARCH_REALM:
		if (ui_list_handler(&search_realm, key, 1)) {
			list_check_values(&search_realm, 1);
			ui_input_assign(&realm_input, list_selected(&search_realm));
		} else if (key == KEY_ENTER && search_input.str->len) {
			active_field = FIELD_SEARCH_BUTTON;
			start_search_pressed();
			main_screen_update_results();
		} else {
			break;
		}
		main_screen_update_searchbox();
		refresh();
		return 1;
	case FIELD_SEARCH_BUTTON:
		if (PRESSED(key)) {
			start_search_pressed();
			main_screen_update_searchbox();
			main_screen_update_results();
			refresh();
			return 1;
		}
		break;
	case FIELD_STOP_BUTTON:
		if (PRESSED(key)) {
			q = list_selected(&queries);

			if (q->id) {
				gift_query_stop(q);
				g_message(_("Search stopped."));
			} else if (!tree_isempty(q)) {
				gift_query_clear(q);
				g_message(_("Search cleared."));
				main_screen_update_results();
			} else {
				g_message(_("A magical search appears, but it is cancelled too soon."));
			}
			main_screen_update_searchbox();
			refresh();
			return 1;
		}
		break;
	case FIELD_RESULT_LIST:
		q = HITS;

		if (tree_flat(q)->num == 0 || tree_flat(q)->sel < 0)
			break;

		/* set up pointers to selected items */
		h = list_selected(tree_flat(q));
		sh = NULL;
		if (INSTANCEOF(h, subhit)) {
			sh = (subhit *) h;
			h = tree_parent(sh);
		}

		if (PRESSED(key) || key == 'd' || key == 'D') {
			download_hit(h, sh);
			if (tree_flat(q)->sel < tree_flat(q)->num - 1)
				tree_flat(q)->sel++;
			main_screen_update_results();
		} else if (key == KEY_LEFT || key == KEY_RIGHT) {
			const char *method = change_sort_method(key == KEY_RIGHT ? 1 : -1);

			incoming_search_item();
			g_message(_("Sorting order changed to %s."), method);
		} else if (ui_list_handler(tree_flat(q), key, RESULT_H)) {
			main_screen_update_results();
		} else if (key == 'j' || key == 'J' || key == 'k' || key == 'K') {
			if (key == 'j' || key == 'J')
				h->infobox.start++;
			else
				h->infobox.start--;
			main_screen_update_info();
		} else if (key == '/' || key == '?') {
			active_field = FIELD_STATUSBAR;
			g_free(last_search_term);
			last_search_term = g_strdup(slash_input.str->str);
			ui_input_init(&slash_input);
			ui_input_handler(&slash_input, key);
			ui_input_validate(&slash_input);
			main_screen_update_results();
		} else if (key == 'n' || key == 'N') {
			slash_find_next(key == 'n' ? 1 : -1);
			main_screen_update_results();
		} else if (key == 'E' || key == 'e' || key == '+' || key == '-') {
			int pos;

			pos = tree_hold(h);
			/* (un) expand this item. */
			if (key == '+' || key == '-')
				h->tnode.expanded = key == '+';
			else
				h->tnode.expanded = !h->tnode.expanded;
			tree_release(h, pos);

			tree_select_item(q, h);
			q->hits.tnode.dirty = 1;
			main_screen_update_results();
		} else if (sh && (key == 'U' || key == 'u')) {
			start_browse(sh->user, sh->node);
		} else if (sh && (key == 'i' || key == 'I')) {
			/* select the parent hit */
			tree_flat(q)->sel = list_find(tree_flat(q), h);
			if (key == 'I') {
				set_config_multi("ignore", "user", sh->user, TRUE);
				g_message(_("Always ignoring results from '%s'."), sh->user);
			} else {
				g_message(_("Ignoring results from '%s' in this session only."), sh->user);
			}
			user_ignore(sh->user);
			/* sh may be freed after this point */
			main_screen_update_results();
		} else {
			break;
		}
		refresh();
		return 1;
	case FIELD_STATUSBAR:
		if (!ui_input_handler(&slash_input, key)) {
			active_field = FIELD_RESULT_LIST;
			if (!slash_input.str->str[0] || !slash_input.str->str[1])
				ui_input_assign(&slash_input, last_search_term);
			if (PRESSED(key))
				slash_find_next(1);
		}
		ui_input_validate(&slash_input);
		main_screen_update_results();
		refresh();
		return 1;
	}

	if (key == 'P' || key == 'p') {
		stats_cycle(my_screen_nr | 0100);
		return 1;
	}

	return 0;
}

static void main_screen_draw(void)
{
	int attr;

	g_assert(active_screen == my_screen_nr);

	if (active_field > FIELD_MAX)
		active_field = FIELD_RESULT_LIST;

	curs_set(0);
	leaveok(stdscr, TRUE);
	mouse_clear(0);

	if (active_field < FIELD_RESULT_LIST) {
		/* Searchbox */
		attr = COLOR_PAIR(COLOR_SEARCH_BOX) | A_BOLD;
		draw_box(0, 0, SEARCH_BOX_W, STAT_BOX_H, _("Search"), attr);
		main_screen_update_searchbox();
	} else {
		attr = COLOR_PAIR(COLOR_INFO_BOX) | A_BOLD;
		draw_box(0, 0, SEARCH_BOX_W, STAT_BOX_H, _("Extra Information"), attr);
		mouse_register(0, 0, SEARCH_BOX_W, STAT_BOX_H, BUTTON1_PRESSED, mouse_infoclick, NULL, 0);
	}

	/* Resultbox */
	draw_box(0, STAT_BOX_H, max_x, RESULT_H + 2, _("Search Results"),
			 COLOR(COLOR_RESULT_BOX, FIELD_RESULT_LIST));
	if (active_field == FIELD_RESULT_LIST)
		ui_cursor_move(0, 0);
	main_screen_update_results();
	mouse_register(0, STAT_BOX_H, max_x, RESULT_H + 2, BUTTON1_PRESSED, mouse_resultclick, NULL, 0);
	mouse_register(0, STAT_BOX_H, max_x, RESULT_H + 2, BUTTON1_DOUBLE_CLICKED,
				   mouse_resultdoubleclick, NULL, 0);
	mouse_register(0, 0, max_x, max_y, BUTTON_UP, mouse_wheel, GINT_TO_POINTER(0), 0);
	mouse_register(0, 0, max_x, max_y, BUTTON_DOWN, mouse_wheel, GINT_TO_POINTER(1), 0);

	/* Statbox */
	attr = COLOR_PAIR(COLOR_STAT_BOX) | A_BOLD;
	draw_box(max_x - STAT_BOX_W - 2, 0, STAT_BOX_W + 2, STAT_BOX_H, _("Statistics"), attr);
	main_screen_update_stats();

	move(curs_y, curs_x);
}

static void main_screen_update_searchbox(void)
{
	int len;

	curs_set(0);
	leaveok(stdscr, TRUE);

	ui_input_draw(2, 1, SEARCH_BOX_W - 4, COLOR(COLOR_SEARCH_BOX, FIELD_SEARCH_TERM), _("Query"),
				  &search_input, active_field == FIELD_SEARCH_TERM);

	ui_input_draw(2, 2, SEARCH_BOX_W - 4, COLOR(COLOR_SEARCH_BOX, FIELD_SEARCH_REALM), _("Realm"),
				  &realm_input, active_field == FIELD_SEARCH_REALM);

	len = vstrlen(_("Query"));
	mouse_register(len + 4, 1, max_x - STAT_BOX_W - len - 8, 1, BUTTON1_PRESSED, mouse_inputclick,
				   GINT_TO_POINTER(FIELD_SEARCH_TERM), 0);
	len = vstrlen(_("Realm"));
	mouse_register(len + 4, 2, max_x - STAT_BOX_W - len - 8, 1, BUTTON1_PRESSED, mouse_inputclick,
				   GINT_TO_POINTER(FIELD_SEARCH_REALM), 0);

	/* We have resizable buttons, so we need to clear the area first */
	mvhline(STAT_BOX_H - 2, 1, ' ', SEARCH_BOX_W - 2);

	draw_button(SEARCH_BOX_W / 4 + 1, STAT_BOX_H - 2, _("Start search"),
				COLOR_BUTTON(COLOR_SEARCH_BOX, FIELD_SEARCH_BUTTON, 1), mouse_buttonclick,
				GINT_TO_POINTER(FIELD_SEARCH_BUTTON));

	draw_button(3 * SEARCH_BOX_W / 4, STAT_BOX_H - 2,
				can_stop_search() > 1 ? _("Stop search") : _("Clear search"),
				COLOR_BUTTON(COLOR_SEARCH_BOX, FIELD_STOP_BUTTON, can_stop_search()),
				mouse_buttonclick, GINT_TO_POINTER(FIELD_STOP_BUTTON));

	move(curs_y, curs_x);
}

#define HEADER_FMT(extra_fmt, header) \
	"%c%c%s:%c%c " extra_fmt, COLOR_SELECT_CHAR, COLOR_HEADER | COLOR_BOLD, \
	header, COLOR_SELECT_CHAR, COLOR_STANDARD

static void make_the_extra_info(hit *allan)
{
	subhit *sh;
	GString *users;
	int i, width = SEARCH_BOX_W - 4;
	char *s;

	/* First clear the list. */
	list_free_entries(&allan->infobox);

	/* Size: */
	s = g_strdup_printf(HEADER_FMT("%u %s", _("Size")), allan->filesize,
						ngettext("byte", "bytes", allan->filesize));
	wrap_lines(&allan->infobox, s, width);
	g_free(s);

	/* User: */
	users = g_string_new("");
	g_string_printf(users, HEADER_FMT("", _("User")));
	for (i = 0; i < tree_children(allan)->num; i++) {
		sh = list_index(tree_children(allan), i);
		g_string_append_printf(users, i < tree_children(allan)->num - 1 ? "%s, " : "%s", sh->user);
	}
	wrap_lines(&allan->infobox, users->str, width);
	g_string_free(users, TRUE);

	/* Path: */
	s = g_strdup_printf(HEADER_FMT("%s", _("Path")), allan->directory);
	wrap_lines(&allan->infobox, s, width);
	g_free(s);

	/* Hash: */
	if (allan->hash) {
		s = g_strdup_printf(HEADER_FMT("%s", _("Hash")), allan->hash);
		wrap_lines(&allan->infobox, s, width);
		g_free(s);
	}

	/* Mime: */
	if (allan->mime) {
		s = g_strdup_printf(HEADER_FMT("%s", _("Mime")), allan->mime);
		wrap_lines(&allan->infobox, s, width);
		g_free(s);
	}

	/* Other meta data */
	for (i = 0; i < allan->metadata.num; i++) {
		metadata *metarn = list_index(&allan->metadata, i);

		s = g_strdup_printf(HEADER_FMT("%s", _(metarn->key)), metarn->data);
		wrap_lines(&allan->infobox, s, width);
		g_free(s);
	}

	allan->meta_dirty = FALSE;
}

static void main_screen_update_info(void)
{
	hit *info;
	query *q = HITS;

	clear_area(1, 1, SEARCH_BOX_W - 2, STAT_BOX_H - 2);

	if (tree_flat(q)->sel < 0) {
		mvaddstr(1, 2, _("Nothing selected."));
		return;
	}
	info = list_selected(tree_flat(q));
	if (INSTANCEOF(info, subhit))
		 info = tree_parent(info);

	if (info->meta_dirty)
		make_the_extra_info(info);

	list_check_values_simple(&info->infobox, STAT_BOX_H - 2);
	info->infobox.sel = -1;
	draw_list(2, 1, SEARCH_BOX_W - 4, STAT_BOX_H - 2, FALSE, &info->infobox);
}

static void main_screen_update_stats(void)
{
	int i, y = 1;
	int width = max_x - STAT_COL1 - 2;
	char tmp[15];
	static struct {
		const char *format;
		format_t compiled;
	} statlines[] = {
		/* *INDENT-OFF* */
		{ N_("{%header:B}Users online: {if users}{%stat-data}{else}{%stat-bad}{endif}{users}"), NULL},
		{ N_("{%header:B}Total shared: {%stat-data}{bytes:bi}B{%stat-box:B}/{%stat-data}{files:b}"), NULL},
		{ N_("{%header:B}Local shares: {%stat-data}{if sharing}{own_bytes:bi}B{%stat-box:B}/{%stat-data}{own_files:b}{else}Hidden{endif}"), NULL},
		{ N_("{%header:B} Shared/user: {%stat-data}{if users}{shared_per_user:bi}B{else} - {endif}"), NULL},
		/* *INDENT-ON* */
	};

	clear_area(STAT_COL1, y, width, STAT_BOX_H - 2);

	for (i = 0; i < G_N_ELEMENTS(statlines); i++) {
		char *s;

		if (!statlines[i].compiled) {
			statlines[i].compiled = format_compile(_(statlines[i].format));
			g_assert(statlines[i].compiled);
		}
		s = format_expand(statlines[i].compiled, (getattrF) stats_getattr, width, &stats);
		move(y++, STAT_COL1);
		draw_printfmt(width, "%s", s);
		g_free(s);
	}

	/* Show the protocol on the bottom frame */
	attrset(COLOR_PAIR(COLOR_STAT_BOX) | A_BOLD);
	g_snprintf(tmp, sizeof tmp, stats.network ? " %s " : "", stats.network);
	mvaddstr(y, STAT_COL1, tmp);
	for (i = strlen(tmp); i < sizeof tmp; i++)
		addch(ACS_HLINE);
	move(curs_y, curs_x);

	/* ask for more stats. misc.c will install a 5 sec timeout */
	request_stats(my_screen_nr | 0100);
}

static void hit_touch(hit *h)
{
	/* terminal have been resized, must render the metadata again */
	h->meta_dirty = TRUE;
}

static void main_screen_update_results(void)
{
	char tmp[15];
	int i, h = RESULT_H;
	query *q = list_selected(&queries);

	if (q->hits.tnode.dirty)
		tree_flatten(q);

	/* invalidate all pretty-strings if screen width has changed. */
	if (q->pretty_width != max_x) {
		tree_touch_all(q);
		list_foreach(tree_children(q), (LFunc) hit_touch);
		q->pretty_width = max_x;
	}

	/* Update resultbox, keep cursor invisible if not activated */
	if (tree_flat(q)->sel != -1)
		list_check_values(tree_flat(q), h);

	/* quick hack to show the cursor even if browsing not begun */
	i = tree_flat(q)->sel;
	if (i == -1)
		tree_flat(q)->sel++;
	draw_list_pretty(2, STAT_BOX_H + 1, max_x - 4, h,
					 active_field >= FIELD_RESULT_LIST, tree_flat(q));
	tree_flat(q)->sel = i;

	move(STAT_BOX_H + RESULT_H + 1, 3);
	attrset(COLOR_PAIR(COLOR_RESULT_BOX) | (active_field >= FIELD_RESULT_LIST ? A_BOLD : 0));
	/* find the selected item. */
	if (i != -1 && !tree_isempty(q)) {
		hit *_hit = list_selected(tree_flat(q));

		if (INSTANCEOF(_hit, subhit))
			 _hit = tree_parent(_hit);

		i = list_find(tree_children(q), _hit);
	}
	g_snprintf(tmp, sizeof tmp, " %i/%i ", i + 1, tree_children(q)->num);
	addstr(tmp);
	for (i = strlen(tmp); i < sizeof tmp; i++)
		addch(ACS_HLINE);

	if (active_field == FIELD_STATUSBAR) {
		show_status(slash_input.str->str);
		ui_cursor_move(slash_input.vpos, max_y - 1 - show_buttonbar);
		curs_set(1);
		leaveok(stdscr, FALSE);
	} else if (active_field == FIELD_RESULT_LIST) {
		curs_set(0);
		leaveok(stdscr, TRUE);
		main_screen_update_info();
	}

	move(curs_y, curs_x);
}

static void download_add_handle(ntree *the_tree, void *udata)
{
	const char *hash;
	unsigned int size;

	hash = interface_lookup(the_tree, "hash");
	size = my_atoi(interface_lookup(the_tree, "size"));

	if (!hash || !size)
		return;

	if (hit_download_mark(hash, size))
		ui_update(my_screen_nr | 0200);
}

#ifdef MOUSE
static void mouse_infoclick(int rx, int ry, void *data)
{
	g_assert(active_screen == my_screen_nr);
	active_field = FIELD_SEARCH_TERM;
	main_screen_draw();
	refresh();
}

static void mouse_resultclick(int rx, int ry, void *data)
{
	query *q = list_selected(&queries);

	g_assert(active_screen == my_screen_nr);
	if (tree_isempty(q))
		return;

	if (active_field != FIELD_RESULT_LIST) {
		active_field = FIELD_RESULT_LIST;
		main_screen_draw();
	}
	if (ry == 0)
		tree_flat(q)->sel -= RESULT_H;
	else if (ry == RESULT_H + 1)
		tree_flat(q)->sel += RESULT_H;
	else
		tree_flat(q)->sel = tree_flat(q)->start + ry - 1;

	/* Expand/collapse a search result. */
	if (rx == 2)
		ui_handler('e');

	list_check_values(tree_flat(q), RESULT_H);
	main_screen_update_results();
	main_screen_update_info();
	refresh();
}

/* Double clicking a hit starts a download */
static void mouse_resultdoubleclick(int rx, int ry, void *data)
{
	query *q = list_selected(&queries);
	hit *h = list_selected(tree_flat(q));

	g_assert(active_screen == my_screen_nr);
	if (tree_isempty(q))
		return;

	if (INSTANCEOF(h, subhit))
		 download_hit(NULL, (subhit *) h);

	else
		download_hit((hit *) h, NULL);
}

static void mouse_buttonclick(int rx, int ry, void *data)
{
	int field = GPOINTER_TO_INT(data);

	g_assert(active_screen == my_screen_nr);

	active_field = field;
	main_screen_handler(KEY_ENTER);
	main_screen_draw();
	refresh();
}

static void mouse_inputclick(int rx, int ry, void *data)
{
	int field = GPOINTER_TO_INT(data);

	g_assert(active_screen == my_screen_nr);

	if (field == FIELD_SEARCH_TERM)
		ui_input_setvpos(&search_input, rx - 1);

	active_field = field;
	main_screen_draw();
	refresh();
}

static void mouse_wheel(int rx, int ry, void *data)
{
	query *q = list_selected(&queries);
	int dir = GPOINTER_TO_INT(data);

	g_assert(active_screen == my_screen_nr);
	if (tree_isempty(q))
		return;

	/* TODO: support scrolling in more fields? */
	if (active_field != FIELD_RESULT_LIST)
		return;

	if (dir)
		tree_flat(q)->sel += (RESULT_H / 2);
	else
		tree_flat(q)->sel -= (RESULT_H / 2);

	list_check_values(tree_flat(q), RESULT_H);
	main_screen_update_results();
	main_screen_update_info();
	refresh();
}
#endif

#define RETURN_INT(x) return v->intval = (x), ATTR_INT
#define RETURN_LONG(x) return v->longval = (x), ATTR_LONG
#define RETURN_STR(x) return v->string = (x), ATTR_STRING

static enum attr_type hit_getattr(const hit *h, const char *key, attr_value * v)
{
	/* COMMENT: All meta data can also be accessed. */
	if (!strcmp(key, "hash"))
		RETURN_STR(h->hash);
	if (!strcmp(key, "filename"))
		RETURN_STR(h->filename);
	if (!strcmp(key, "path"))
		RETURN_STR(h->directory);
	if (!strcmp(key, "filesize"))
		RETURN_INT(h->filesize);
	if (!strcmp(key, "sources"))
		RETURN_INT(tree_children(h)->num);
	if (!strcmp(key, "availability")) {
		int i, avail = 0;

		for (i = 0; i < tree_children(h)->num; i++) {
			subhit *sh = list_index(tree_children(h), i);

			if (sh->availability)
				avail++;
		}
		RETURN_INT(avail);
	}
	if (!strcmp(key, "downloading"))
		RETURN_INT(h->downloading);
	if (!strcmp(key, "expanded"))
		RETURN_INT(h->tnode.expanded);
	if (!strcmp(key, "suffix")) {
		char *t = strrchr(h->filename + 1, '.');

		RETURN_STR(t && t[1] ? t + 1 : "");
	}
	if (!strcmp(key, "net")) {
		char *colon;
		subhit *sh;

		if (!tree_children(h)->num)
			RETURN_STR("");

		sh = list_index(tree_children(h), 0);
		if (!(colon = strchr(sh->href, ':')))
			RETURN_STR("");
		v->strlen.len = colon - sh->href;
		v->strlen.string = sh->href;
		return ATTR_STRLEN;
	}

	RETURN_STR(meta_data_lookup(h, key) ? : "");
}

/* This describes how to get meta data from a subhit */
static enum attr_type subhit_getattr(const subhit *h, const char *key, attr_value * v)
{
	/* COMMENT: Keys from the parent hit can also be accessed. */
	if (!strcmp(key, "user"))
		RETURN_STR(h->user);
	if (!strcmp(key, "url"))
		RETURN_STR(h->href);
	if (!strcmp(key, "availability"))
		RETURN_INT(h->availability);
	if (!strcmp(key, "net")) {
		char *colon;
		if ((colon = strchr(h->href, ':'))) {
			v->strlen.len = colon - h->href;
			v->strlen.string = h->href;
			return ATTR_STRLEN;
		}
	}

	return hit_getattr(tree_parent(h), key, v);
}

static char *hit_present(hit *foo)
{
	/* Compile the format corresponding to the first part of the mime */
	char *slash;

	if (!foo->formatting) {
		slash = foo->mime ? strchr(foo->mime, '/') : NULL;
		if (slash)
			*slash = '\0';
		if (foo->mime)
			foo->formatting = format_get(foo->mime, NULL);
		if (slash)
			*slash = '/';
		if (!foo->formatting)
			foo->formatting = format_ref(hit_fmt);
	}
	return format_expand(foo->formatting, (getattrF) hit_getattr, max_x - 4, foo);
}

static char *subhit_present(subhit *foo)
{
	return format_expand(subhit_fmt, (getattrF) subhit_getattr, max_x - 4, foo);
}

static enum attr_type stats_getattr(const gift_stat * s, const char *key, attr_value * v)
{
	if (!strcmp(key, "users"))
		RETURN_INT(s->users);
	if (!strcmp(key, "files"))
		RETURN_INT(s->files);
	if (!strcmp(key, "bytes"))
		RETURN_LONG(s->bytes);
	if (!strcmp(key, "own_files"))
		RETURN_INT(s->own_files);
	if (!strcmp(key, "own_bytes"))
		RETURN_LONG(s->own_bytes);
	if (!strcmp(key, "network"))
		RETURN_STR(s->network);
	if (!strcmp(key, "sharing"))
		RETURN_INT(sharing);
	if (!strcmp(key, "shared_per_user"))
		if (s->users)
			RETURN_LONG(s->bytes / s->users);
	RETURN_STR("");
}

static const ui_methods main_screen_methods = {
	main_screen_draw,
	NULL,						/* hide */
	main_screen_update,
	main_screen_handler,
	main_screen_destroy,
	N_("Searches"),
};
