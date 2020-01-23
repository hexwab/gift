/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
 * $Id: ui_main.c,v 1.323 2003/05/14 09:09:36 chnix Exp $
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
#include "get.h"
#include "format.h"

#include "protocol.h"

#define HITS (((query*)list_selected(&queries))->hits)

#define INPUT_MAX		200

enum {
	FIELD_SEARCH_TERM,
	FIELD_SEARCH_REALM,
	FIELD_SEARCH_BUTTON,
	FIELD_STOP_BUTTON,
	FIELD_RESULT_LIST,
	FIELD_MAX,
	FIELD_STATUSBAR,
};

static struct {
	int active_field;
	char search_term[INPUT_MAX];
	unsigned int search_pos;
	int curs_x, curs_y;
	list search_realm;
	char *slash_search_term;
	char *last_search_term;
	int slash_search_pos;
	unsigned int maxsize, minsize;
	int maxspeed, minspeed;
	format_t hit_fmt;
	format_t subhit_fmt;
} scr = {
FIELD_SEARCH_TERM, "", 0, 0, 0, LIST_INITIALIZER, NULL, 0, 0, 0, 0, -1,};

static void main_screen_draw(void);
static void main_screen_update_stats(void);
static void main_screen_update_results(void);
static void main_screen_update_info(void);
static void main_screen_update_searchbox(void);
static void download_add_handle(ntree * the_tree, void *);
static void incoming_search_item(void);
static void update_stats_if_focus(void);

static guint query_timer_tag = 0;

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

/* Update search results every so and so seconds */
static const int HITS_INTERVAL = 200;	/* 0.2 seonds */

void update_stats_if_focus(void)
{
	if (active_screen == &main_screen_methods) {
		main_screen_update_stats();
		refresh();
	}
}

static gboolean query_timer(void *data)
{
	query *q = list_selected(&queries);

	if (q->dirty)
		incoming_search_item();
	return TRUE;
}

void main_screen_init(void)
{
	unsigned int i;
	static const char *realms[] = {
		N_("everything"), N_("audio"), N_("video"),
		N_("images"), N_("text documents"), N_("software"),
		N_("user"), N_("hash"), N_("list my shares"),
	};

	list_initialize(&scr.search_realm);

	gift_search_init();
	misc_init(update_stats_if_focus);

	query_timer_tag = g_timeout_add(HITS_INTERVAL, query_timer, NULL);

	for (i = 0; i < G_N_ELEMENTS(realms); i++)
		list_append(&scr.search_realm, g_strdup(_(realms[i])));
	scr.search_realm.sel = 0;
	scr.slash_search_term = NULL;
	scr.active_field = 0;

	/* Load the default formats */
	format_load("$expanded", "{if expanded}-{else}+{endif}");
	format_load("$availability", "{if 2<availability}{%hit-good:B}"
				"{elif availability<1}{%hit-bad:B}{else}{%header:B}{endif}");
	format_load("$hit_pfx", "{if downloading}!{else}{$expanded}{endif}"
				"{$availability}{availability:2}{%standard}/"
				"{if downloading<1}{%header:B}{endif}{filesize:bi}{%standard}");
	scr.hit_fmt = format_get("hit", "{$hit_pfx} {filename}{space}");
	scr.subhit_fmt =
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

	g_assert(scr.hit_fmt);
	g_assert(scr.subhit_fmt);

	gift_register("ADDDOWNLOAD", (EventCallback) download_add_handle, NULL);
}

void main_screen_destroy(void)
{
	g_source_remove(query_timer_tag);
	g_free(scr.slash_search_term);
	list_free_entries(&scr.search_realm);
	gift_search_cleanup();
	format_unref(scr.hit_fmt);
	format_unref(scr.subhit_fmt);
	scr.hit_fmt = NULL;
	scr.subhit_fmt = NULL;

	misc_destroy();
}

static void incoming_search_item(void)
{
	if (active_screen == &main_screen_methods) {
		if (scr.active_field < FIELD_RESULT_LIST)
			main_screen_update_searchbox();
		main_screen_update_results();
		refresh();
	}
}

static query *prepare_search(int browse)
{
	int i;
	query *q;

	/* trim it and make cursor inside */
	trim(scr.search_term);
	scr.search_pos = strlen(scr.search_term);
	/* add query to history if it's not already there */
	/* skip the first dummy entry */
	for (i = 1; i < queries.num; i++) {
		q = list_index(&queries, i);

		if (!strcmp(scr.search_term, q->search_term) && scr.search_realm.sel == q->realm) {
			queries.sel = i;
			gift_query_stop(q);
			gift_query_clear(q);
			return q;
		}
	}

	/* create a new query structure and append to list */
	q = new_query(scr.search_term, scr.search_realm.sel);
	q->formatting = browse ? format_get("browse", NULL) : NULL;

	i--;
	list_insert(&queries, q, i);
	queries.sel = i;

	return q;
}

void start_browse(const char *user, const char *node)
{
	query *q;
	ntree *packet = NULL;

	strncpy(scr.search_term, user, INPUT_MAX);
	scr.search_realm.sel = 6;	/* BROWSE */
	q = prepare_search(1);

	q->id = gift_new_id();

	interface_append_int(&packet, "BROWSE", q->id);
	interface_append(&packet, "query", scr.search_term);
	if (node)
		interface_append(&packet, "node", node);
	if (gift_write(&packet) < 0) {
		g_message(_("Couldn't start search!"));
		q->id = 0;
	} else {
		gift_register_id(q->id, (EventCallback) search_result_item_handler, q);
		g_message(_("Retrieving file list..."));
	}
	incoming_search_item();
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
	char *includes = NULL, *excludes = NULL;

	t = names + scr.search_realm.sel;
	q = prepare_search(t->format);

	q->id = gift_new_id();

	interface_append_int(&packet, t->command, q->id);
	parse_typed_query(q->search_term, &includes, &excludes);
	interface_append(&packet, "query", includes);
	if (excludes && excludes[0])
		interface_append(&packet, "exclude", excludes);
	if (t->realm)
		interface_append(&packet, "realm", t->realm);
	if (gift_write(&packet) < 0) {
		g_message(_("Couldn't start search!"));
		q->id = 0;
	} else {
		gift_register_id(q->id, (EventCallback) search_result_item_handler, q);
		g_message(_("Searching..."));
	}
	g_free(includes);
	g_free(excludes);
	incoming_search_item();
}

static void slash_find_next(int dir)
{
	tree *t = &HITS;
	int prev, i;
	char *pattern = scr.slash_search_term;
	hit *h;
	list *l;

	if (!pattern || !pattern[0] || !pattern[1]) {
		g_message(_("No previous search pattern."));
		return;
	}
	if (tree_isempty(t)) {
		g_message(_("No hits to search on."));
		return;
	}

	if (pattern[0] == '?')
		dir = -dir;
	pattern++;

	h = list_selected(&t->flat);
	if (IS_A(h, subhit))
		h = tree_parent(h);

	l = tree_children(t);

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
			tree_select_top(t, i);
			show_status(scr.slash_search_term);
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
	return !tree_isempty(&q->hits);
}

/* returns 0 if key was not catched */
static int main_screen_handler(int key)
{
	if (key == '\t' || key == KEY_BTAB) {
		int direction = key == KEY_BTAB ? -1 : 1;

		for (;;) {
			scr.active_field += direction;
			if (scr.active_field >= FIELD_MAX)
				scr.active_field -= FIELD_MAX;
			if (scr.active_field < 0)
				scr.active_field += FIELD_MAX;
			if (scr.active_field == FIELD_STOP_BUTTON && !can_stop_search())
				continue;
			if (scr.active_field == FIELD_RESULT_LIST && tree_isempty(&HITS))
				continue;
			break;
		}

		if (scr.active_field == FIELD_RESULT_LIST && HITS.flat.sel == -1)
			HITS.flat.sel++;

		main_screen_draw();
		refresh();
		return 1;
	}

	switch (scr.active_field) {
		hit *h;
		subhit *sh;

	case FIELD_SEARCH_TERM:
		if (!ui_input_handler(scr.search_term, &scr.search_pos, key, INPUT_MAX - 1))
			if (ui_list_handler(&queries, key, 5)) {
				query *q = list_selected(&queries);

				scr.search_pos = INPUT_MAX;	/* place cursor at the end */
				strcpy(scr.search_term, q->search_term);
				scr.search_realm.sel = q->realm;
				incoming_search_item();
			}
		/* Check values */
		ui_input_handler(scr.search_term, &scr.search_pos, 0, INPUT_MAX - 1);
		if (key == KEY_ENTER && *scr.search_term) {
			scr.active_field = FIELD_SEARCH_BUTTON;
			start_search_pressed();
			main_screen_update_results();
		}

		main_screen_update_searchbox();
		refresh();
		return 1;
	case FIELD_SEARCH_REALM:
		if (ui_list_handler(&scr.search_realm, key, 1)) {
			list_check_values(&scr.search_realm, 1);
		} else if (key == KEY_ENTER && *scr.search_term) {
			scr.active_field = FIELD_SEARCH_BUTTON;
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
			query *q;

			q = list_selected(&queries);

			if (q->id) {
				gift_query_stop(q);
				g_message(_("Search stopped."));
			} else if (!tree_isempty(&q->hits)) {
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
		if (HITS.flat.num == 0 || HITS.flat.sel < 0)
			break;

		/* set up pointers to selected items */
		h = list_selected(&HITS.flat);
		sh = NULL;
		if (IS_A(h, subhit)) {
			sh = (subhit *) h;
			h = tree_parent(sh);
		}

		if (PRESSED(key) || key == 'd' || key == 'D') {
			download_hit(h, sh);
			if (HITS.flat.sel < HITS.flat.num - 1)
				HITS.flat.sel++;
			main_screen_update_results();
		} else if (key == KEY_LEFT || key == KEY_RIGHT) {
			const char *method = change_sort_method(key == KEY_RIGHT ? 1 : -1);

			incoming_search_item();
			g_message(_("Sorting order changed to %s."), method);
		} else if (ui_list_handler(&HITS.flat, key, RESULT_H)) {
			main_screen_update_results();
		} else if (key == 'j' || key == 'J' || key == 'k' || key == 'K') {
			if (key == 'j' || key == 'J')
				h->infobox.start++;
			else
				h->infobox.start--;
			main_screen_update_info();
		} else if (key == '/' || key == '?') {
			scr.active_field = FIELD_STATUSBAR;
			if (!scr.slash_search_term)
				scr.slash_search_term = g_new0(char, INPUT_MAX);

			g_free(scr.last_search_term);
			scr.last_search_term = g_strdup(scr.slash_search_term);
			scr.slash_search_term[0] = key;
			scr.slash_search_term[1] = '\0';
			scr.slash_search_pos = strlen(scr.slash_search_term);
			main_screen_update_results();
		} else if (key == 'n' || key == 'N') {
			slash_find_next(key == 'n' ? 1 : -1);
			main_screen_update_results();
		} else if (key == 'E' || key == 'e' || key == '+' || key == '-') {
			int key;

			key = tree_hold(h);
			/* (un) expand this item. */
			if (key == '+' || key == '-')
				h->tnode.expanded = key == '+';
			else
				h->tnode.expanded = !h->tnode.expanded;
			tree_release(h, key);

			tree_select_item(&HITS, h);
			HITS.tnode.dirty = 1;
			main_screen_update_results();
		} else if (sh && (key == 'U' || key == 'u')) {
			start_browse(sh->user, sh->node);
		} else if (sh && (key == 'i' || key == 'I')) {
			/* select the parent hit */
			HITS.flat.sel = list_find(&HITS.flat, h);
			g_message(_("Ignoring results from '%s'"), sh->user);
			user_ignore(sh->user);
			/* sh may be freed after this point */
			main_screen_update_results();
		} else {
			break;
		}
		refresh();
		return 1;
	case FIELD_STATUSBAR:
		if (!ui_input_handler(scr.slash_search_term, &scr.slash_search_pos, key, INPUT_MAX - 1)) {
			scr.active_field = FIELD_RESULT_LIST;
			if (!scr.slash_search_term[0] || !scr.slash_search_term[1]) {
				g_free(scr.slash_search_term);
				scr.slash_search_term = g_strdup(scr.last_search_term);
			}
			if (PRESSED(key))
				slash_find_next(1);
		}
		main_screen_update_results();
		refresh();
		return 1;
	}

	if (key == 'P' || key == 'p') {
		stats_cycle();
		return 1;
	}

	return 0;
}

static void main_screen_draw(void)
{
	int y = 1, attr;

	if (active_screen != &main_screen_methods)
		return;

	if (scr.active_field > FIELD_MAX)
		scr.active_field = FIELD_RESULT_LIST;

	curs_set(0);
	leaveok(stdscr, TRUE);
	mouse_clear(0);

	if (scr.active_field < FIELD_RESULT_LIST) {
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
	if (scr.active_field == FIELD_RESULT_LIST)
		scr.curs_x = scr.curs_y = 0;
	main_screen_update_results();
	mouse_register(0, STAT_BOX_H, max_x, RESULT_H + 2, BUTTON1_PRESSED, mouse_resultclick, NULL, 0);
	mouse_register(0, STAT_BOX_H, max_x, RESULT_H + 2, BUTTON1_DOUBLE_CLICKED,
				   mouse_resultdoubleclick, NULL, 0);
	mouse_register(0, 0, max_x, max_y, BUTTON_UP, mouse_wheel, (void *) 0, 0);
	mouse_register(0, 0, max_x, max_y, BUTTON_DOWN, mouse_wheel, (void *) 1, 0);

	/* Statbox */
	attr = COLOR_PAIR(COLOR_STAT_BOX) | A_BOLD;
	draw_box(max_x - STAT_BOX_W - 2, 0, STAT_BOX_W + 2, STAT_BOX_H, _("Statistics"), attr);
	attrset(COLOR_PAIR(COLOR_HEADER) | A_BOLD);
	draw_header(STAT_COL1, y++, _("Users online"));
	draw_header(STAT_COL1, y++, _("Total shared"));
	draw_header(STAT_COL1, y++, _("Local shares"));
	draw_header(STAT_COL1, y++, _(" Shared/user"));
	main_screen_update_stats();

	move(scr.curs_y, scr.curs_x);
}

static void draw_input_special(int x, int y, int w, char *header, char *value, int field, int pos)
{
	int xx, offset = 0, headlen = strlen(header);

	if (scr.active_field == field && pos > w - headlen - 5)
		offset = pos - (w - headlen - 5);

	xx = draw_input(x, y, w, header, value + offset, COLOR(COLOR_SEARCH_BOX, field));

	if (scr.active_field == field) {
		scr.curs_x = xx;
		if (pos >= 0)
			scr.curs_x = x + pos - offset + headlen + 3;
		getyx(stdscr, scr.curs_y, xx);
		curs_set(1);
		leaveok(stdscr, FALSE);
	}
}

static void main_screen_update_searchbox(void)
{
	int len;

	curs_set(0);
	leaveok(stdscr, TRUE);

	draw_input_special(2, 1, SEARCH_BOX_W - 4, _("Query"),
					   scr.search_term, FIELD_SEARCH_TERM, scr.search_pos);

	draw_input_special(2, 2, SEARCH_BOX_W - 4, _("Realm"),
					   list_selected(&scr.search_realm), FIELD_SEARCH_REALM, -1);

	len = strlen(_("Query"));
	mouse_register(len + 4, 1, max_x - STAT_BOX_W - len - 8, 1, BUTTON1_PRESSED, mouse_inputclick,
				   (void *) FIELD_SEARCH_TERM, 0);
	len = strlen(_("Realm"));
	mouse_register(len + 4, 2, max_x - STAT_BOX_W - len - 8, 1, BUTTON1_PRESSED, mouse_inputclick,
				   (void *) FIELD_SEARCH_REALM, 0);

	/* since we have resizable buttons now, we need to clear the area first */
	mvhline(STAT_BOX_H - 2, 1, ' ', SEARCH_BOX_W - 2);

	draw_button(SEARCH_BOX_W / 4 + 1, STAT_BOX_H - 2, _("Start search"),
				COLOR_BUTTON(COLOR_SEARCH_BOX, FIELD_SEARCH_BUTTON, 1), mouse_buttonclick,
				(void *) FIELD_SEARCH_BUTTON);

	draw_button(3 * SEARCH_BOX_W / 4, STAT_BOX_H - 2,
				can_stop_search() > 1 ? _("Stop search") : _("Clear search"),
				COLOR_BUTTON(COLOR_SEARCH_BOX, FIELD_STOP_BUTTON, can_stop_search()),
				mouse_buttonclick, (void *) FIELD_STOP_BUTTON);

	move(scr.curs_y, scr.curs_x);
}

static void make_the_extra_info(hit * allan)
{
	subhit *sh;
	GString *users;
	int i;
	char *s;

	users = g_string_new("\vBUser:\va ");
	for (i = 0; i < tree_children(allan)->num; i++) {
		sh = list_index(tree_children(allan), i);
		g_string_append_printf(users, i < tree_children(allan)->num - 1 ? "%s, " : "%s", sh->user);
	}

	list_free_entries(&allan->infobox);
	s = g_strdup_printf("\vBSize:\va %u %s", allan->filesize,
						ngettext("byte", "bytes", allan->filesize));
	list_append(&allan->infobox, s);
	list_append(&allan->infobox, g_strdup(users->str));
	s = g_strdup_printf("\vBPath:\va %s", allan->directory);
	list_append(&allan->infobox, s);
	if (allan->mime) {
		s = g_strdup_printf("\vBMime:\va %s", allan->mime);
		list_append(&allan->infobox, s);
	}
	if (allan->hash) {
		s = g_strdup_printf("\vBHash:\va %s", allan->hash);
		list_append(&allan->infobox, s);
	}

	for (i = 0; i < allan->metadata.num; i++) {
		metadata *metarn = list_index(&allan->metadata, i);

		s = g_strdup_printf("\vB%s:\va %s", metarn->key, metarn->data);
		list_append(&allan->infobox, s);
	}
	g_string_free(users, TRUE);
}

static void main_screen_update_info(void)
{
	hit *info;

	clear_area(1, 1, SEARCH_BOX_W - 2, STAT_BOX_H - 2);

	if (HITS.flat.sel < 0) {
		mvaddstr(1, 2, _("Nothing selected."));
		return;
	}
	info = list_selected(&HITS.flat);
	if (IS_A(info, subhit))
		info = tree_parent(info);

	/* FIXME: dirty */
	if (!info->infobox.num /* || info->dirty */ )
		make_the_extra_info(info);

	list_check_values_simple(&info->infobox, STAT_BOX_H - 2);
	info->infobox.sel = -1;
	draw_list(2, 1, SEARCH_BOX_W - 4, STAT_BOX_H - 2, FALSE, &info->infobox);
}

static void draw_critic_stat(int x, int y, int w, unsigned int val)
{
	if (val) {
		attrset(COLOR_PAIR(COLOR_STAT_DATA));
		move(y, x);
		draw_printfmt(w, "\vf%u", val);
	} else {
		attrset(COLOR_PAIR(COLOR_STAT_DATA_BAD));
		mvaddch(y, x, '0');
	}
}

static void main_screen_update_stats(void)
{
	char tmp[15];
	int i, y = 1;
	int width = max_x - STAT_COL2 - 1;

	clear_area(STAT_COL2, y, width, STAT_BOX_H - 2);

	draw_critic_stat(STAT_COL2, y++, width, stats.users);

	move(y++, STAT_COL2);
	draw_printfmt(width, _("\vf%sB\vE/\vf%s"), humanify(stats.bytes), humanify_1000(stats.files));

	move(y++, STAT_COL2);
	if (sharing)
		draw_printfmt(width, _("\vf%sB\vE/\vf%s"), humanify(stats.own_bytes),
					  humanify_1000(stats.own_files));
	else
		draw_printfmt(width, _("Hidden"));

	move(y++, STAT_COL2);
	if (stats.users)
		draw_printfmt(width, _("\vf%sB"), humanify(stats.bytes / stats.users));
	else
		draw_printfmt(width, "\vf  -");

	attrset(COLOR_PAIR(COLOR_STAT_BOX) | A_BOLD);
	g_snprintf(tmp, sizeof tmp, stats.network ? " %s " : "", stats.network);
	mvaddstr(y, STAT_COL1, tmp);
	for (i = strlen(tmp); i < sizeof tmp; i++)
		addch(ACS_HLINE);
	move(scr.curs_y, scr.curs_x);
}
static void main_screen_update_results(void)
{
	char tmp[15];
	int i, h = RESULT_H;
	query *q;
	tree *t;

	q = list_selected(&queries);
	t = &q->hits;

	if (t->tnode.dirty)
		tree_flatten(t);

	/* invalidate all pretty-strings if screen width has changed. */
	if (q->pretty_width != max_x) {
		tree_touch_all(t);
		q->pretty_width = max_x;
	}

	/* Update resultbox, keep cursor invisible if not activated */
	if (t->flat.sel != -1)
		list_check_values(&t->flat, h);

	/* quick hack to show the cursor even if browsing not begun */
	i = t->flat.sel;
	if (i == -1)
		t->flat.sel++;
	draw_list_pretty(2, STAT_BOX_H + 1, max_x - 4, h,
					 scr.active_field >= FIELD_RESULT_LIST, &t->flat);
	t->flat.sel = i;

	move(STAT_BOX_H + RESULT_H + 1, 3);
	attrset(COLOR_PAIR(COLOR_RESULT_BOX) | (scr.active_field >= FIELD_RESULT_LIST ? A_BOLD : 0));
	/* find the selected item. FIXME: this is too expensive! */
	if (i != -1 && !tree_isempty(t)) {
		hit *h = list_selected(&t->flat);

		if (IS_A(h, subhit))
			h = tree_parent(h);
		i = list_find(tree_children(t), h);
	}
	g_snprintf(tmp, sizeof tmp, " %i/%i ", i + 1, tree_children(t)->num);
	addstr(tmp);
	for (i = strlen(tmp); i < sizeof tmp; i++)
		addch(ACS_HLINE);

	if (scr.active_field == FIELD_STATUSBAR) {
		show_status(scr.slash_search_term);
		scr.curs_y = max_y - 1 - show_buttonbar;
		scr.curs_x = scr.slash_search_pos;
		curs_set(1);
		leaveok(stdscr, FALSE);
	} else if (scr.active_field == FIELD_RESULT_LIST) {
		curs_set(0);
		leaveok(stdscr, TRUE);
		main_screen_update_info();
	}

	move(scr.curs_y, scr.curs_x);
	q->dirty = 0;
}

static void download_add_handle(ntree * the_tree, void *udata)
{
	const char *hash;
	unsigned int size;

	hash = interface_lookup(the_tree, "hash");
	size = my_atoi(interface_lookup(the_tree, "size"));

	if (!hash || !size)
		return;

	if (hit_download_mark(hash, size) && active_screen == &main_screen_methods) {
		main_screen_update_results();
		refresh();
	}
}

#ifdef MOUSE
static void mouse_infoclick(int rx, int ry, void *data)
{
	if (active_screen != &main_screen_methods)
		return;
	scr.active_field = FIELD_SEARCH_TERM;
	main_screen_draw();
	refresh();
}

static void mouse_resultclick(int rx, int ry, void *data)
{
	if (active_screen != &main_screen_methods || tree_isempty(&HITS))
		return;

	if (scr.active_field != FIELD_RESULT_LIST) {
		scr.active_field = FIELD_RESULT_LIST;
		main_screen_draw();
	}
	if (ry == 0)
		HITS.flat.sel -= RESULT_H;
	else if (ry == RESULT_H + 1)
		HITS.flat.sel += RESULT_H;
	else
		HITS.flat.sel = HITS.flat.start + ry - 1;

	/* Expand/collapse a search result. */
	if (rx == 2)
		ui_handler('e');

	list_check_values(&HITS.flat, RESULT_H);
	main_screen_update_results();
	main_screen_update_info();
	refresh();
}

/* Double clicking a hit starts a download */
static void mouse_resultdoubleclick(int rx, int ry, void *data)
{
	hit *h = list_selected(&HITS.flat);

	if (active_screen != &main_screen_methods || tree_isempty(&HITS))
		return;

	if (IS_A(h, subhit))
		download_hit(NULL, (subhit *) h);
	else
		download_hit((hit *) h, NULL);
}

static void mouse_buttonclick(int rx, int ry, void *data)
{
	int field = (int) data;

	g_assert(active_screen == &main_screen_methods);

	scr.active_field = field;
	main_screen_handler(KEY_ENTER);
	main_screen_draw();
	refresh();
}

static void mouse_inputclick(int rx, int ry, void *data)
{
	int field = (int) data;

	if (active_screen != &main_screen_methods)
		return;

	if (field == FIELD_SEARCH_TERM) {
		scr.search_pos = rx - 1;
		ui_input_handler(scr.search_term, &scr.search_pos, 0, INPUT_MAX - 1);
	}

	scr.active_field = field;
	main_screen_draw();
	refresh();
}

static void mouse_wheel(int rx, int ry, void *data)
{
	int dir = (int) data;

	if (active_screen != &main_screen_methods || tree_isempty(&HITS))
		return;

	/* TODO: support scrolling in more fields? */
	if (scr.active_field != FIELD_RESULT_LIST)
		return;

	if (dir)
		HITS.flat.sel += (RESULT_H / 2);
	else
		HITS.flat.sel -= (RESULT_H / 2);

	list_check_values(&HITS.flat, RESULT_H);
	main_screen_update_results();
	main_screen_update_info();
	refresh();
}
#endif

#define RETURN_INT(x) return v->intval = (x), ATTR_INT
#define RETURN_STR(x) return v->string = (x), ATTR_STRING

static enum attr_type hit_getattr(const hit * h, const char *key, attr_value * v)
{
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

	RETURN_STR(meta_data_lookup(h, key) ? : "");
}

/* This describes how to get meta data from a subhit */
static enum attr_type subhit_getattr(const subhit * h, const char *key, attr_value * v)
{
	if (!strcmp(key, "user"))
		RETURN_STR(h->user);
	if (!strcmp(key, "url"))
		RETURN_STR(h->href);
	if (!strcmp(key, "availability"))
		RETURN_INT(h->availability);
	if (!strcmp(key, "net")) {
		char *colon = strchr(h->href, ':');

		if (!colon)
			RETURN_STR("");
		v->strlen.len = colon - h->href;
		v->strlen.string = h->href;
		return ATTR_STRLEN;
	}
	return hit_getattr(tree_parent(h), key, v);
}

static char *hit_present(hit * foo)
{
	/* Compile the format corresponding to the first part of the mime */
	char *slash;

	if (!foo->formatting) {
		slash = foo->mime ? strchr(foo->mime, '/') : NULL;
		if (slash) {
			*slash = '\0';
			foo->formatting = format_get(foo->mime, NULL);
			*slash = '/';
		}
		if (!foo->formatting)
			foo->formatting = format_ref(scr.hit_fmt);
	}
	return format_expand(foo->formatting, (getattrF) hit_getattr, max_x - 4, foo);
}

static char *subhit_present(subhit * foo)
{
	return format_expand(scr.subhit_fmt, (getattrF) subhit_getattr, max_x - 4, foo);
}

const rendering hit_methods = {
	(void *) hit_present,
	(void *) hit_destroy,
	(getattrF) hit_getattr,
};
const rendering subhit_methods = {
	(void *) subhit_present,
	(void *) subhit_destroy,
	(getattrF) subhit_getattr,
};

const ui_methods main_screen_methods = {
	main_screen_draw, main_screen_handler
};
