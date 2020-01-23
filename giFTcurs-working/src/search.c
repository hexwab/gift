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
 * $Id: search.c,v 1.94 2002/11/28 19:52:24 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "search.h"
#include "screen.h"
#include "gift.h"
#include "protocol.h"
#include "poll.h"
#include "xcommon.h"
#include "get.h"
#include "ban.h"
#include "settings.h"
#include "format.h"

#define HITS(q) ((q)->hits.top)

list queries = LIST_INITIALIZER;

static int compare_sources_filesize(const hit * h1, const hit * h2);
static int compare_availability_filesize(const hit * h1, const hit * h2);
static int compare_filesize(const hit * h1, const hit * h2);
static int compare_alfabet(const hit * h1, const hit * h2);
static int compare_alfabetpath(const hit * h1, const hit * h2);
static int compare_hash(const hit * h1, const hit * h2);
static int compare_source(const subhit * a, const subhit * b);
static void add_subhit(hit * bob, char *user, char *href, int av, char *node);

static CmpFunc hit_compare = (CmpFunc) compare_availability_filesize;
static int max_hits = 1000;
static int filter_hits = 0;

void gift_search_stop(gift_id id, char *type)
{
	ntree *packet = NULL;

	assert(id);

	/* FIXME: Don't do this if we're disconnected */
	interface_append_int(&packet, type, id);
	interface_append(&packet, "action", "cancel");
	gift_write(&packet);

	gift_unregister_id(id);
}

static void queries_detach_all()
{
	list_foreach(&queries, (LFunc) gift_query_stop);
}

void query_destroy(query * q)
{
	gift_query_clear(q);
	free(q->search_term);
	free(q);
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

	max_hits = atoi(get_config("set", "max-hits", "1000"));
	filter_hits = atoi(get_config("set", "filter-hits", "0"));

	list_initialize(&queries);

	gift_register("DETACH", (EventCallback) queries_detach_all, NULL);

	/* Add a dummy search, which represents empty search fields. */
	q = calloc(1, sizeof(query));
	q->search_term = strdup("");
	q->realm = 0;
	q->id = 0;
	tree_initialize(&q->hits);
	list_append(&queries, q);
	queries.sel = 0;

	banwords_init();
}

query *new_query(char *search_term, int realm)
{
	query *q = calloc(1, sizeof(query));

	q->search_term = strdup(search_term);
	q->realm = realm;
	q->id = 0;
	tree_initialize(&q->hits);
	list_initialize(&q->hits_index);
	list_sort(&q->hits_index, (CmpFunc) compare_hash);
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
	HITS(q).sel = -1;
	HITS(q).start = 0;
}

void subhit_destroy(subhit * sh)
{
	free(sh->user);
	free(sh->node);
	free(sh->href);
	free(sh->pretty);
	free(sh);
}

static void meta_destroy(metadata * sossen)
{
	free(sossen->key);
	free(sossen->data);
	free(sossen);
}

void hit_destroy(hit * bob)
{
	free(bob->pretty);
	free(bob->filename);
	free(bob->directory);
	free(bob->hash);
	free(bob->mime);
	list_foreach(&bob->metadata, (LFunc) meta_destroy);
	list_remove_all(&bob->metadata);
	list_free_entries(&bob->infobox);
	list_destroy_all(&bob->sources);
	format_unref(bob->formatting);
	free(bob);
}

char *change_sort_method(int direction)
{
	/* direction == 0 just sorts with current method. */
	/* direction == -1 or +1 changes method and sorts */
	static const void *methods[] = { compare_sources_filesize, compare_availability_filesize,
		compare_filesize, compare_alfabet, compare_alfabetpath, NULL
	};
	int i;
	query *q = list_selected(&queries);

	/* Change sorting method */
	for (i = 0; methods[i]; i++)
		if (methods[i] == hit_compare)
			break;
	if ((i += direction) < 0)
		i = buflen(methods) - 2;
	if (!(hit_compare = methods[i]))
		hit_compare = methods[0];

	tree_sort(&q->hits, (CmpFunc) hit_compare);

	if (hit_compare == (CmpFunc) compare_alfabet)
		return _("alphabetical");
	if (hit_compare == (CmpFunc) compare_alfabetpath)
		return _("alphabetical by path");
	if (hit_compare == (CmpFunc) compare_filesize)
		return _("size-based");
	if (hit_compare == (CmpFunc) compare_availability_filesize)
		return _("availability- and size-based");

	return _("number of sources- and size-based");
}

int hit_download_mark(char *hash, unsigned int size)
{
	int i, j, change = 0;
	hit template;

	template.hash = hash;
	template.filesize = size;

	for (j = 0; j < queries.num; j++) {
		query *q = list_index(&queries, j);

		i = list_find(&q->hits_index, &template);
		if (i >= 0) {
			hit *h = list_index(&q->hits_index, i);

			if (!h->downloading) {
				h->downloading = 1;
				/* invalidate previous pretty-string */
				TOUCH(h);
				q->dirty = 1;
				change++;
			}
		}
	}
	return change;
}

static void add_subhit(hit * bob, char *user, char *href, int av, char *node)
{
	subhit *sh = NEW(subhit);

	sh->user = strdup(user);
	sh->node = my_strdup(node);
	sh->href = strdup(href);
	sh->availability = av;
	sh->parent = bob;
	list_insort(&bob->sources, sh);
}

char *meta_data_lookup(hit * the_hit, char *key)
{
	int i;

	for (i = 0; i < the_hit->metadata.num; i++) {
		metadata *nisse = list_index(&the_hit->metadata, i);

		if (!strcmp(nisse->key, key))
			return nisse->data;
	}
	return NULL;
}

static void parse_meta_data_real(ntree * tree, hit * dobbs)
{
	metadata *meta;
	char *key, *data;

	if (!(key = interface_name(tree)))
		return;
	data = interface_value(tree);

	if (!(meta = malloc(sizeof *meta)))
		FATAL("malloc");
	meta->key = strlower(strdup(key));
	meta->data = strdup(data ? data : "");
	list_append(&dobbs->metadata, meta);
}

static void parse_meta_data(ntree * tree, hit * udata)
{
	if (!strcasecmp(interface_name(tree), "META"))
		interface_foreach_key(tree, (PForEachFunc) parse_meta_data_real, udata);
}

/* This is the function that parses search hits */
void search_result_item_handler(ntree * data, query * q)
{
	char *href, *user, *filepath;
	int availability;
	hit *bob, template;
	int i;

	if (!data || !(href = interface_lookup(data, "url"))) {
		/* end of search */
		gift_unregister_id(q->id);
		q->id = 0;
		q->dirty = 1;
		message(ngettext("Search complete. We got %i unique hit for %s.",
						 "Search complete. We got %i unique hits for %s.",
						 HITS(q).num), HITS(q).num, q->search_term);
		return;
	}

	user = interface_lookup(data, "user");
	template.hash = interface_lookup(data, "hash");
	template.filesize = my_atoi(interface_lookup(data, "size"));
	availability = my_atoi(interface_lookup(data, "availability"));

	if (!user)
		return;

	if (filter_hits && !availability)
		return;

	/* Check if we already had this one, and insert it otherwise. */
	i = list_find(&q->hits_index, &template);
	if (i >= 0) {
		/* Found it. */
		bob = list_index(&q->hits_index, i);

		/* Check if we already have this user */
		for (i = 0; i < bob->sources.num; i++) {
			subhit *sh = list_index(&bob->sources, i);

			if (!strcmp(user, sh->user))
				return;
		}

		/* find the index into the real table before we change it */
		i = list_find(&HITS(q), bob);
		assert(i >= 0);

		/* Never seen this href before */
		add_subhit(bob, user, href, availability, interface_lookup(data, "node"));

		/* Add meta data if we don't have it already. */
		if (!bob->metadata.num)
			interface_foreach(data, (PForEachFunc) parse_meta_data, bob);

		/* Invalidate previous pretty-string */
		TOUCH(bob);

		list_resort(&HITS(q), i);
		q->dirty = 1;
		return;
	}

	filepath = interface_lookup(data, "file");

	if (!filepath || !banwords_check(filepath))
		return;

	if (max_hits) {
		if (HITS(q).num == max_hits - 1)
			message(_("The limit of %d unique hits was reached."), max_hits);
		else if (HITS(q).num >= max_hits)
			return;
	}

	/* It's a brand new hit */
	bob = NEW(hit);
	bob->hash = my_strdup(template.hash);
	bob->filesize = template.filesize;

	/* fill in the rest and add it to the list and the index */
	bob->filename = strdup(basename(filepath));
	bob->directory = strdup(dirname(filepath));	/* modifies filepath */
	bob->mime = my_strdup(interface_lookup(data, "mime"));
	list_initialize(&bob->sources);
	list_initialize(&bob->metadata);
	list_sort(&bob->sources, (CmpFunc) compare_source);
	add_subhit(bob, user, href, availability, interface_lookup(data, "node"));
	bob->downloading = !!transfer_find(&downloads, bob->hash, bob->filesize, 0);

	/* Parse meta data. */
	interface_foreach(data, (PForEachFunc) parse_meta_data, bob);

	list_insort(&HITS(q), bob);
	list_insort(&q->hits_index, bob);
	q->dirty = 1;

	return;
}

CmpFunc default_cmp_func(int user_browse)
{
	return user_browse ? (CmpFunc) compare_alfabetpath : hit_compare;
}

static int compare_source(const subhit * a, const subhit * b)
{
	return a->availability - b->availability;
}

/* IMPORTANT NOTE:
 * All hit comparing routines must return 0 only if the items have the
 * same adress. Otherwise the optimized list finding doesn't work.
 * IMPORTANT NOTE 2 (dedicated to saturn):
 *   you can't compare ints by subtraction! (shorts are ok)
 */

/* sort against number of sources and filesize */
static int compare_sources_filesize(const hit * h1, const hit * h2)
{
	if (h1->sources.num != h2->sources.num)
		return h2->sources.num - h1->sources.num;

	return compare_filesize(h1, h2);
}

/* sort against availability and filesize */
static int compare_availability_filesize(const hit * h1, const hit * h2)
{
	int i, a1 = 0, a2 = 0;

	for (i = 0; i < h1->sources.num; i++)
		a1 += !!((subhit *) list_index(&h1->sources, i))->availability;
	for (i = 0; i < h2->sources.num; i++)
		a2 += !!((subhit *) list_index(&h2->sources, i))->availability;

	if (a2 < a1)
		return -1;
	if (a2 > a1)
		return 1;

	return compare_sources_filesize(h1, h2);
}

/* sort against filesize */
static int compare_filesize(const hit * h1, const hit * h2)
{
	if (h2->filesize < h1->filesize)
		return -1;

	if (h2->filesize > h1->filesize)
		return 1;

	return compare_pointers(h1, h2);
}

/* sort alphabetically */
static int compare_alfabet(const hit * h1, const hit * h2)
{
	int i = strcoll(h1->filename, h2->filename);

	if (i)
		return i;

	return compare_pointers(h1, h2);
}

/* sort alphabetically by path */
static int compare_alfabetpath(const hit * h1, const hit * h2)
{
	int i = strcoll(h1->directory, h2->directory);

	if (i)
		return i;

	return compare_alfabet(h1, h2);
}

/* sort against hash */
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
