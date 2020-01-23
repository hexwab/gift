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
 * $Id: search.c,v 1.113 2003/05/14 09:09:35 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "search.h"
#include "screen.h"
#include "gift.h"
#include "protocol.h"
#include "xcommon.h"
#include "get.h"
#include "ban.h"
#include "settings.h"
#include "format.h"

#define HITS(q) ((q)->hits.tnode.children)

list queries = LIST_INITIALIZER;

static int compare_hash(const hit * h1, const hit * h2);
static int compare_sources(const subhit * a, const subhit * b);
static void add_subhit(hit * bob, const char *user, const char *href, int av, const char *node);

static int max_hits;
static int filter_hits;
static char **sorting_methods;
static list ignored_users = LIST_INITIALIZER;

void gift_search_stop(gift_id id, const char *type)
{
	ntree *packet = NULL;

	g_assert(id);

	/* FIXME: Don't do this if we're disconnected */
	interface_append_int(&packet, type, id);
	interface_append(&packet, "action", "cancel");
	gift_write(&packet);

	gift_unregister_id(id);
}

static void queries_detach_all(void)
{
	list_foreach(&queries, (LFunc) gift_query_stop);
}

void query_destroy(query * q)
{
	gift_query_clear(q);
	format_unref(q->formatting);
	g_free(q->search_term);
	g_free(q);
}

void gift_search_cleanup(void)
{
	list_foreach(&queries, (LFunc) query_destroy);
	list_remove_all(&queries);
	banwords_cleanup();
}

void gift_search_init(void)
{
	query *q;
	const char *methods;

	max_hits = atoi(get_config("set", "max-hits", "0"));
	filter_hits = atoi(get_config("set", "filter-hits", "0"));

	methods =
		get_config("sort", "hits",
				   "availability,filesize;filename;path,filename;filesize;sources,filesize");
	sorting_methods = g_strsplit(methods, ";", -1);

	list_initialize(&queries);
	list_sort(&ignored_users, (CmpFunc) strcmp);

	gift_register("DETACH", (EventCallback) queries_detach_all, NULL);

	/* Add a dummy search, which represents empty search fields. */
	q = g_new0(query, 1);
	q->search_term = g_strdup("");
	q->realm = 0;
	q->id = 0;
	tree_initialize(&q->hits);
	list_append(&queries, q);
	queries.sel = 0;

	banwords_init();
}

query *new_query(char *search_term, int realm)
{
	query *q = g_new0(query, 1);

	q->search_term = g_strdup(search_term);
	q->realm = realm;
	q->id = 0;
	format_unref(q->formatting);
	q->formatting = NULL;
	tree_initialize(&q->hits);
	list_initialize(&q->hits_index);
	list_sort(&q->hits_index, (CmpFunc) compare_hash);

	/* sort agains the first item in sorting_methods */
	q->sorting_method = 0;
	tree_sort(&q->hits, sorting_methods[0]);
	return q;
}

void gift_query_stop(query * q)
{
	if (q->id) {
		gift_search_stop(q->id, "SEARCH");
		q->id = 0;
	}
}

void gift_query_clear(query * q)
{
	gift_query_stop(q);
	tree_destroy_all(&q->hits);
	list_remove_all(&q->hits_index);
	q->formatting = NULL;
	HITS(q).sel = -1;
	HITS(q).start = 0;
}

void subhit_destroy(subhit * sh)
{
	g_free(sh->user);
	g_free(sh->node);
	g_free(sh->href);
	tree_node_destroy(sh);
}

static void meta_destroy(metadata * sossen)
{
	g_free(sossen->key);
	g_free(sossen->data);
	g_free(sossen);
}

void hit_destroy(hit * bob)
{
	g_free(bob->filename);
	g_free(bob->directory);
	g_free(bob->hash);
	g_free(bob->mime);
	list_foreach(&bob->metadata, (LFunc) meta_destroy);
	list_remove_all(&bob->metadata);
	list_free_entries(&bob->infobox);
	list_destroy_all(&bob->tnode.children);
	format_unref(bob->formatting);
	tree_node_destroy(bob);
}

char *change_sort_method(int direction)
{
	/* direction == 0 just sorts with current method. */
	/* direction == -1 or +1 changes method and sorts */
	int i;
	char *s;
	query *q = list_selected(&queries);

	/* Change sorting method */
	i = q->sorting_method;
	g_assert(i >= 0);

	i += direction;
	if (i < 0)
		while (sorting_methods[i + 1])
			i++;
	else if (!sorting_methods[i])
		i = 0;

	q->sorting_method = i;

	s = sorting_methods[i];
	tree_sort(&q->hits, s);

	return s;
}

int hit_download_mark(const char *hash, unsigned int size)
{
	int i, j, change = 0;
	hit template;

	template.hash = (char *) hash;
	template.filesize = size;

	for (j = 0; j < queries.num; j++) {
		query *q = list_index(&queries, j);

		i = list_find(&q->hits_index, &template);
		if (i >= 0) {
			hit *h = list_index(&q->hits_index, i);

			if (!h->downloading) {
				int key = tree_hold(h);

				h->downloading = 1;
				/* invalidate previous pretty-string */
				tree_release(h, key);
				q->dirty = 1;
				change++;
			}
		}
	}
	return change;
}

static void add_subhit(hit * bob, const char *user, const char *href, int av, const char *node)
{
	subhit *sh = NEW_NODE(subhit);

	sh->user = g_strdup(user);
	sh->node = g_strdup(node);
	sh->href = g_strdup(href);
	sh->availability = av;
	tree_insort(bob, sh);
}

const char *meta_data_lookup(const hit * the_hit, const char *key)
{
	int i;
	char *lowered = g_ascii_strdown(key, -1);

	for (i = 0; i < the_hit->metadata.num; i++) {
		metadata *nisse = list_index(&the_hit->metadata, i);

		if (!strcmp(nisse->key, lowered)) {
			g_free(lowered);
			return nisse->data;
		}
	}
	g_free(lowered);
	return NULL;
}

static void parse_meta_data_real(ntree * tree, hit * dobbs)
{
	metadata *meta;
	const char *key, *data;

	if (!(key = interface_name(tree)))
		return;

	/* Check if we already have this key */
	if (meta_data_lookup(dobbs, key))
		return;

	data = interface_value(tree);

	if (!data)
		return;

	meta = g_new(metadata, 1);
	meta->key = g_ascii_strdown(key, -1);
	meta->data = g_strdup(data);
	list_append(&dobbs->metadata, meta);
}

static void parse_meta_data(ntree * tree, hit * udata)
{
	if (!strcmp(interface_name(tree), "META"))
		interface_foreach_key(tree, (PForEachFunc) parse_meta_data_real, udata);
}

/* This is the function that parses search hits */
void search_result_item_handler(ntree * data, query * q)
{
	const char *href, *user, *filepath;
	int availability;
	hit *bob, template;
	int i;

	if (interface_isempty(data)) {
		/* end of search */
		gift_unregister_id(q->id);
		q->id = 0;
		q->dirty = 1;
		g_message(ngettext("Search complete. We got %i unique hit for %s.",
						   "Search complete. We got %i unique hits for %s.",
						   HITS(q).num), HITS(q).num, q->search_term);
		return;
	}

	href = interface_lookup(data, "url");
	user = interface_lookup(data, "user");
	template.hash = (char *) interface_lookup(data, "hash");
	template.filesize = my_atoi(interface_lookup(data, "size"));
	availability = my_atoi(interface_lookup(data, "availability"));

	if (filter_hits && !availability)
		return;

	if (user_isignored(user))
		return;

	/* Check if we already had this one, and insert it otherwise. */
	i = list_find(&q->hits_index, &template);
	if (i >= 0) {
		/* Found it. */
		if (!user || !href)
			return;

		bob = list_index(&q->hits_index, i);

		/* Check if we already have this user */
		for (i = 0; i < tree_children(bob)->num; i++) {
			subhit *sh = list_index(tree_children(bob), i);

			if (!strcmp(user, sh->user))
				return;
		}

		/* find the index into the real table before we change it */
		i = tree_hold(bob);
	} else {
		filepath = interface_lookup(data, "file");

		/* This is needed when listing your own shares */
		if (!filepath)
			filepath = interface_lookup(data, "path");

		if (!filepath || !banwords_check(filepath))
			return;

		if (max_hits) {
			if (HITS(q).num == max_hits - 1)
				g_message(_("The limit of %d unique hits was reached."), max_hits);
			else if (HITS(q).num >= max_hits)
				return;
		}

		/* It's a brand new hit */
		bob = NEW_NODE(hit);
		bob->hash = g_strdup(template.hash);
		bob->filesize = template.filesize;
		list_insort(&q->hits_index, bob);

		/* fill in the rest and add it to the list and the index */
		bob->filename = g_path_get_basename(filepath);
		bob->directory = g_path_get_dirname(filepath);
		bob->mime = g_strdup(interface_lookup(data, "mime"));
		list_initialize(&bob->tnode.children);
		list_initialize(&bob->metadata);
		list_sort(&bob->tnode.children, (CmpFunc) compare_sources);

		bob->downloading = !!transfer_find(&downloads, bob->hash, bob->filesize, 0);
		bob->formatting = format_ref(q->formatting);
	}

	if (user && href)
		add_subhit(bob, user, href, availability, interface_lookup(data, "node"));
	/* Parse and merge meta data. */
	interface_foreach(data, (PForEachFunc) parse_meta_data, bob);

	/* invalidate pretty string and resort */
	if (i == -1)
		tree_insort(&q->hits, bob);
	else
		tree_release(bob, i);
	q->dirty = 1;

	return;
}

static gboolean filter_user(subhit * h, char *user)
{
	return !!strcmp(h->user, user);
}

void user_ignore(char *user)
{
	int i, j;

	/* copy the string, we may destroy the original soon */
	user = g_strdup(user);

	/* Put on ignore list */
	list_insort(&ignored_users, user);

	/* Filter all searches */
	for (i = 0; i < queries.num; i++) {
		query *q = list_index(&queries, i);

		for (j = 0; j < q->hits.tnode.children.num; j++) {
			hit *h = list_index(&q->hits.tnode.children, j);

			list_filter(tree_children(h), (LFilter) filter_user, user,
						(void (*)(void *)) subhit_destroy);
		}
		/* resort the list */
		tree_sort(&q->hits, q->hits.sort_order);
	}
}

int user_isignored(const char *user)
{
	return list_find(&ignored_users, user) != -1;
}

static int compare_sources(const subhit * a, const subhit * b)
{
	return a->availability - b->availability;
}

/* sort against hash - better keep this fast and not use format strings */
static int compare_hash(const hit * h1, const hit * h2)
{
	if (h2->filesize < h1->filesize)
		return -1;

	if (h2->filesize > h1->filesize)
		return 1;

	if (h1->hash && h2->hash)
		return strcmp(h1->hash, h2->hash);

	if (!h1->hash && !h2->hash)
		return compare_pointers(h1, h2);

	return h1->hash ? -1 : 1;
}
