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
 * $Id: transfer.c,v 1.132 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "parse.h"
#include "get.h"
#include "transfer.h"
#include "gift.h"
#include "settings.h"

/* This should never be used. It's only here so it can be translated. */
static const char *_statuses[] G_GNUC_UNUSED;
static const char *_statuses[] = {
	N_("Waiting"),
	N_("Paused"),
	N_("Queued (Remotely)"),
	N_("Queued"),
	N_("Active"),
	N_("Complete"),
	N_("Cancelled (Remotely)"),
	N_("Timed out"),
	N_("Awaiting connection"),
	N_("Connecting"),
	N_("Receiving"),
	N_("Verifying"),
	/*N_("Queued (position %s)"), */
	N_("Sending"),
	N_("Received HTTP PUSH"),
	N_("Received HTTP headers"),
	N_("Sent HTTP request"),
	N_("Malformed HTTP header"),
	N_("Remote host had an aneurism"),
	N_("Connection closed"),
	N_("Connection refused"),
	N_("File not found"),
	N_("Local read error"),
	N_("Incorrect chunk size"),
	N_("No route to host"),
	/* XXX: this list will hopefully be obsolete when giFT gets i18n */
};

static const int BANDWIDTH_TIMER = 2000;	/* 2 seconds */

int autodestroy_all = 0;
transfer_tree downloads, uploads;
tree_class_t transfer_class, source_class;
static tree_class_t transfer_tree_class;

static int nr_alive = 0;
static char **source_sort_methods;
static const char default_transfer_sort_methods[] = "filesize;transferred;bandwidth;filename";
static const char default_source_sort_methods[] = "bandwidth;user";
static int source_sort_method;
static guint bandwidth_timer_tag = 0;

static void transfer_change_handle(ntree * packet, transfer *);
static void transfer_add_handle(ntree * packet, transfer_tree *);
static void addsource_parse(ntree * packet, transfer *);
static gboolean bandwidth_timer(void *udata);
static void estimate_speed(struct bandwidth_meas *bw, int sizediff, gulong dt, gulong idle_time);
static void transfer_detach(transfer *);
static void transfer_detach_all(void);
static const char *transfer_sort_order(const transfer_tree *);
static const char *source_sort_order(void);
static void transfer_destroy(transfer *);
static void source_destroy(source *);

void transfers_init(void)
{
	const char *methods;

	tree_initialize(&downloads);
	tree_initialize(&uploads);
	CLASSOF(&downloads) = &transfer_tree_class;
	CLASSOF(&uploads) = &transfer_tree_class;

	transfer_tree_class.get_sort_order = (void *) transfer_sort_order;
	transfer_class.get_sort_order = (void *) source_sort_order;
	transfer_class.destroy = (void *) transfer_destroy;
	source_class.destroy = (void *) source_destroy;

	autodestroy_all = atoi(get_config("set", "autoclear", "0"));

	methods = get_config("sort", "downloads", NULL);
	if (!methods)
		methods = get_config("sort", "transfers", default_transfer_sort_methods);
	downloads.sort_methods = g_strsplit(methods, ";", -1);

	methods = get_config("sort", "uploads", NULL);
	if (!methods)
		methods = get_config("sort", "transfers", default_transfer_sort_methods);
	uploads.sort_methods = g_strsplit(methods, ";", -1);

	methods = get_config("sort", "sources", default_source_sort_methods);
	source_sort_methods = g_strsplit(methods, ";", -1);

	uploads.sort_method = downloads.sort_method = source_sort_method = 0;

	gift_register("ADDDOWNLOAD", (EventCallback) transfer_add_handle, &downloads);
	gift_register("ADDUPLOAD", (EventCallback) transfer_add_handle, &uploads);
	gift_register("DETACH", (EventCallback) transfer_detach_all, NULL);

	tree_resort(&uploads);
	tree_resort(&downloads);
}

static void source_destroy(source * s)
{
	g_free(s->user);
	g_free(s->node);
	g_free(s->href);
	g_free(s->status);
	tree_node_destroy(s);
}

static void transfer_destroy(transfer * t)
{
	transfer_detach(t);
	g_free(t->protocol);
	g_free(t->filename);
	g_free(t->hash);
	g_free(t->status);
	g_timer_destroy(t->last_time);
	tree_node_destroy(t);
}

/* Replace a allocated string with another.
 * Avoids malloc/free if the strings are equal. */
static void change_string(char **dst, const char *src)
{
	if (!*dst) {
		*dst = g_strdup(src);
	} else if (strcmp(*dst, src)) {
		g_free(*dst);
		*dst = g_strdup(src);
	}
}

static void source_detach(source * s)
{
	/* What is an appropriate status here? */
	change_string(&s->status, N_("Inactive"));
}

static void transfer_detach(transfer * t)
{
	t->paused = 0;

	if (t->id) {
		gift_unregister_id(t->id);
		t->id = 0;

		if (--nr_alive == 0)
			g_source_remove(bandwidth_timer_tag);
	}

	/*list_destroy_all(&t->sourcen); */
	list_foreach(tree_children(t), (LFunc) source_detach);

	if (t->search_id) {
		gift_search_stop(t->search_id, "LOCATE");
		t->search_id = 0;
	}
}

static void transfer_detach_panic(transfer * t)
{
	/* This is called when giFT disconnects without a word. */
	int pos = tree_hold(t);

	if (t->id)
		change_string(&t->status, N_("Unknown status"));
	transfer_detach(t);

	tree_release(t, pos);
}

static void transfer_detach_all(void)
{
	list_foreach(tree_children(&downloads), (LFunc) transfer_detach_panic);
	list_foreach(tree_children(&uploads), (LFunc) transfer_detach_panic);
	ui_update(downloads.update_ui);
	ui_update(uploads.update_ui);
}

void transfer_forget(transfer * t)
{
	tree_unlink(t);
	transfer_destroy(t);
}

static transfer *transfer_new(gift_id id, const char *name, unsigned int size)
{
	transfer *t = NEW_NODE(transfer);

	t->id = id;
	t->filename = g_strdup(name);
	t->filesize = size;
	t->protocol = NULL;
	t->last_time = g_timer_new();
	list_initialize(tree_children(t));
	transfer_reset(t);
	return t;
}

void transfer_cancel(transfer * t)
{
	ntree *packet = NULL;

	if (!t->id)
		return;

	interface_append_int(&packet, "TRANSFER", t->id);
	interface_append(&packet, "action", "cancel");
	gift_write(&packet);
}

void source_cancel(transfer * t, source * s)
{
	ntree *packet = NULL;

	if (!t->id)
		return;

	interface_append_int(&packet, "DELSOURCE", t->id);
	interface_append(&packet, "url", s->href);
	gift_write(&packet);
}

void transfer_reset(transfer * t)
{
	/* reset progress statistics */
	t->transferred = 0;
	memset(&t->bw, 0, sizeof t->bw);
}

void transfer_suspend(transfer * t)
{
	ntree *packet = NULL;

	if (!t->id)
		return;

	interface_append_int(&packet, "TRANSFER", t->id);
	interface_append(&packet, "action", t->paused ? "unpause" : "pause");
	gift_write(&packet);
}

/* Find a (dead) download suitable for reuse */
transfer *transfer_find(transfer_tree * tr, const char *hash, unsigned int size, gboolean dead)
{
	int i;

	if (!hash)
		return NULL;

	for (i = 0; i < tree_children(tr)->num; i++) {
		transfer *t = list_index(tree_children(tr), i);

		if (dead && t->id)
			continue;
		if (t->filesize == size && t->hash && !strcmp(t->hash, hash))
			return t;
	}
	return NULL;
}

static transfer *transfer_find_id(transfer_tree * tr, gift_id id)
{
	int i;

	for (i = 0; i < tree_children(tr)->num; i++) {
		transfer *t = list_index(tree_children(tr), i);

		if (t->id == id)
			return t;
	}
	return NULL;
}

int transfer_alive(transfer * t)
{
	return !!t->id;
}

source *lookup_source(transfer * t, const char *user, const char *href)
{
	int i;

	for (i = 0; i < tree_children(t)->num; i++) {
		source *s = list_index(tree_children(t), i);

		if (!strcmp(user, s->user) && !strcmp(href, s->href))
			return s;
	}

	return NULL;
}

static void source_change_parse(ntree * param, transfer * t)
{
	const char *user, *href, *status;
	source *s;
	unsigned int transmit, total, start;
	int diff;
	int pos;

	if (strcmp(interface_name(param), "SOURCE"))
		return;

	user = interface_lookup(param, "user");
	href = interface_lookup(param, "url");

	if (!user || !href) {
		DEBUG("Bad SOURCE.");
		return;
	}

	if (!(s = lookup_source(t, user, href))) {
		DEBUG("A source I didn't know about was changed");
		return;
	}

	pos = tree_hold(s);

	status = interface_lookup(param, "statusgrl");
	if (status) {
		/* giFT 030608 mentiones these strings as candidates for statusgrl:
		 * "Waiting" "Paused" "Queued (Remotely)" "Queued" "Active"
		 * "Complete" "Cancelled (Remotely)" "Timed out"
		 */
		if (status[0] == 'W')
			s->state = SOURCE_WAITING;
		else if (status[0] == 'P')
			s->state = SOURCE_PAUSED;
		else if (status[0] == 'A')
			s->state = SOURCE_ACTIVE;
		else if (status[0] == 'T')
			s->state = SOURCE_TIMEOUT;
		else if (status[0] == 'T')
			s->state = SOURCE_TIMEOUT;
		else if (status[0] == 'Q' || strchr(status, 'R'))
			s->state = SOURCE_QUEUED_REMOTE;
		else if (status[0] == 'Q')
			s->state = SOURCE_QUEUED_LOCAL;
		else if (status[0] == 'C' || strchr(status, 'R'))
			s->state = SOURCE_CANCELLED;
		else if (status[0] == 'C')
			s->state = SOURCE_COMPLETE;
	}

	/* FIXME: use status if there is no action. I hope this is only for
	   backwards compatibility. */
	if (!(status = interface_lookup(param, "action")))
		status = interface_lookup(param, "status");
	if (status)
		change_string(&s->status, status);

	total = my_atoi(interface_lookup(param, "total"));
	transmit = my_atoi(interface_lookup(param, "transmit"));
	start = my_atoi(interface_lookup(param, "start"));

	/* total(), transmit() and start() is skipped when status(Paused) */
	/* Also, don't measure bandwidth on the first transmit() tag */
	if (total && s->total) {
		/* Check if this source has started on a new chunk */
		if (start != s->start)
			diff = transmit + (s->total - s->transmit);
		else
			diff = transmit - s->transmit;

		if (diff < 0) {
			DEBUG("Hmm.. negative bandwidth");
			diff = 0;
		}

		estimate_speed(&s->bw, diff, t->elapsed, 0);
	} else {
		memset(&s->bw, 0, sizeof s->bw);
	}

	s->transmit = transmit;
	s->start = start;
	s->total = total;

	tree_release(s, pos);
}

static void addsource_parse(ntree * param, transfer * t)
{
	const char *user, *href, *status;
	source *s;

	user = interface_lookup(param, "user");
	href = interface_lookup(param, "url");
	status = interface_lookup(param, "status");

	if (!user || !href || !status) {
		DEBUG("Bad SOURCE.");
		return;
	}

	if (lookup_source(t, user, href)) {
		DEBUG("Already had that source");
		return;
	}
	s = NEW_NODE(source);
	s->user = g_strdup(user);
	s->node = g_strdup(interface_lookup(param, "node"));
	s->href = g_strdup(href);

	change_string(&s->status, status);
	s->start = my_atoi(interface_lookup(param, "start"));
	s->transmit = my_atoi(interface_lookup(param, "transmit"));
	s->total = my_atoi(interface_lookup(param, "total"));

	if (!t->protocol && strchr(href, ':'))
		t->protocol = g_strndup(href, strchr(href, ':') - href);
	tree_insort(t, s);
}

static void transfer_update_fields(transfer * t, ntree * param)
{
	const char *state = interface_lookup(param, "state");

	t->transferred = my_atoi(interface_lookup(param, "transmit"));

	if (!state)
		return;

	if (!t->status || strcmp(state, t->status)) {
		g_free(t->status);
		t->status = g_strdup(state);
		t->paused = !g_ascii_strcasecmp(state, "paused");
		if (t->paused)
			memset(&t->bw, 0, sizeof t->bw);
	}
}

static void transfer_add_handle(ntree * param, transfer_tree * transfer_tree)
{
	gift_id id;
	transfer *t;
	const char *filename, *hash;
	unsigned int size;
	int pos = -1;

	id = my_atoi(interface_value(param));

	if (!id) {
		DEBUG("Daemon sent a spooky transfer with a zero id.");
		return;
	}

	if (transfer_find_id(transfer_tree, id)) {
		DEBUG("Got transfer_ADD but already have it");
		return;
	}
	filename = interface_lookup(param, "file");
	hash = interface_lookup(param, "hash");
	size = my_atoi(interface_lookup(param, "size"));

	if (!size)
		return;

	if (!filename)
		filename = "";

	/* Check if we can reuse an existing (lost) transfer */
	/* Maybe we should search by name here */
	if ((t = transfer_find(transfer_tree, hash, size, TRUE))) {
		pos = tree_hold(t);
		g_free(t->filename);
		t->filename = g_strdup(filename);
		t->id = id;
	} else {
		t = transfer_new(id, filename, size);
		t->hash = g_strdup(hash);
	}

	transfer_update_fields(t, param);

	if (transfer_tree == &uploads) {
		if (!my_atoi(interface_lookup(param, "shared")))
			t->autodestroy = 1;
	}

	interface_foreach(param, (PForEachFunc) addsource_parse, t);

	if (pos == -1)
		tree_insort(transfer_tree, t);
	else
		tree_release(t, pos);

	ui_update(transfer_tree->update_ui);

	if (nr_alive++ == 0)
		bandwidth_timer_tag = g_timeout_add(BANDWIDTH_TIMER, bandwidth_timer, NULL);

	gift_register_id(id, (EventCallback) transfer_change_handle, t);
}

static void transfer_change_handle(ntree * param, transfer * t)
{
	transfer_tree *transfer_tree;
	const char *command = interface_name(param);
	int pos;

	transfer_tree = tree_parent(t);

	g_timer_start(t->last_time);

	if (!strcmp(command, "ADDSOURCE")) {
		pos = tree_hold(t);
		addsource_parse(param, t);
		tree_release(t, pos);
	} else if (!strcmp(command, "DELSOURCE")) {
		const char *user, *href, *status;
		source *s;

		user = interface_lookup(param, "user");
		href = interface_lookup(param, "url");
		status = interface_lookup(param, "status");

		if (!user || !href || !status) {
			DEBUG("Bad SOURCE.");
			return;
		}

		if (!(s = lookup_source(t, user, href))) {
			DEBUG("Didn't have that source");
			return;
		}

		pos = tree_hold(t);

		tree_unlink(s);
		source_destroy(s);

		tree_release(t, pos);
	} else if (strstr(command, "DEL")) {
		if (transfer_tree == &downloads) {
			if (t->transferred == t->filesize) {
				ui_update(transfer_tree->update_ui | UPDATE_ATTENTION);
				g_message(_("Download of %s completed."), t->filename);
			} else {
				g_message(_("Download of %s cancelled."), t->filename);
			}
		}
		if (autodestroy_all || t->autodestroy) {
			transfer_forget(t);
		} else {
			pos = tree_hold(t);
			transfer_detach(t);
			if (t->transferred < t->filesize)
				change_string(&t->status, N_("Cancelled"));
			tree_release(t, pos);
		}
	} else {					/* CHG* */
		unsigned int throughput, elapsed;

		pos = tree_hold(t);

		transfer_update_fields(t, param);

		throughput = my_atoi(interface_lookup(param, "throughput"));
		elapsed = my_atoi(interface_lookup(param, "elapsed"));
		t->elapsed = elapsed;

		estimate_speed(&t->bw, throughput, elapsed, 0);

		/* source_change_parse wants t->elapsed to be set */
		interface_foreach(param, (PForEachFunc) source_change_parse, t);

		tree_release(t, pos);
	}

	ui_update(transfer_tree->update_ui);
}

static const int ESTIMATE_TIME = 8000;	/* 8 seconds or so */

/* This function recalculates the bandwidth, taking account to history of
 * previous samples, a new sample, and the time this transfer has been idle.
 */
static void estimate_speed(struct bandwidth_meas *bw, int transferred, gulong elapsed,
						   gulong total_time)
{
	int i;
	unsigned int total_size;

	//DEBUG("%p: transferred=%d elapsed=%d idle=%d", bw, transferred, elapsed, total_time);

	/* check if deltatime is enough */
	if (bw->samples[0].dt + elapsed < ESTIMATE_TIME / G_N_ELEMENTS(bw->samples)) {
		/* add this onto the previous one */
		bw->samples[0].dt += elapsed;
		bw->samples[0].dsize += transferred;
	} else {
		/* shift all previous samples down */
		memmove(bw->samples + 1, bw->samples,
				sizeof bw->samples[0] * (G_N_ELEMENTS(bw->samples) - 1));
		bw->samples[0].dt = elapsed;
		bw->samples[0].dsize = transferred;
	}

	total_size = 0;

	for (i = 0; i < G_N_ELEMENTS(bw->samples) && total_time < ESTIMATE_TIME; i++) {
		unsigned int dt = bw->samples[i].dt, dsize = bw->samples[i].dsize;

		//DEBUG("%p: dt=%d ds=%d", bw, dt, dsize);

		if (total_time + dt > ESTIMATE_TIME) {
			total_size += (ESTIMATE_TIME - total_time) * dsize / dt;
			total_time = ESTIMATE_TIME;
			break;
		}
		total_time += dt;
		total_size += dsize;
	}
	if (total_size == 0)
		bw->bandwidth = 0;
	else if (total_time == 0)
		bw->bandwidth = INT_MAX;	/* prevent division by zero */
	else
		bw->bandwidth = 1000 * total_size / total_time;

	//DEBUG("%p: size=%d time=%d bandwidth=%d", bw, total_size, total_time, bw->bandwidth);
}

static int transfer_calculate(transfer * t)
{
	int i;
	gulong idle_time;
	int key, key2;

	if (t->paused)
		return 0;

	idle_time = g_timer_elapsed(t->last_time, NULL) * 1000;

	/* Check if the transfer has been inactive for BANDWIDTH_TIMER ms. */
	if (idle_time < BANDWIDTH_TIMER)
		return 0;

	key = tree_hold(t);

	/* Transfer is stalled. Start cutting down the bandwidth */
	estimate_speed(&t->bw, 0, 0, idle_time);

	/* Ditto for the sources */
	for (i = 0; i < tree_children(t)->num; i++) {
		source *s = list_index(tree_children(t), i);

		key2 = tree_hold(s);
		estimate_speed(&s->bw, 0, 0, idle_time);
		tree_release(s, key2);
	}

	tree_release(t, key);
	return 1;
}

static gboolean bandwidth_timer(void *udata)
{
	int changed, i;

	for (changed = i = 0; i < tree_children(&downloads)->num; i++)
		changed += transfer_calculate(list_index(tree_children(&downloads), i));
	if (changed)
		ui_update(downloads.update_ui);

	for (changed = i = 0; i < tree_children(&uploads)->num; i++)
		changed += transfer_calculate(list_index(tree_children(&uploads), i));
	if (changed)
		ui_update(uploads.update_ui);
	return TRUE;
}

char *transfer_change_sort_method(transfer_tree * t, int dir)
{
	/* direction == 0 just sorts with current method. */
	/* direction == -1 or +1 changes method and sorts */
	int i;

	/* Change sorting method */
	i = t->sort_method;
	g_assert(i >= 0);

	i += dir;
	if (i < 0)
		while (t->sort_methods[i + 1])
			i++;
	else if (!t->sort_methods[i])
		i = 0;
	t->sort_method = i;

	tree_resort(t);

	return t->sort_methods[i];
}

char *source_change_sort_method(int dir)
{
	/* direction == 0 just sorts with current method. */
	/* direction == -1 or +1 changes method and sorts */
	int i;

	/* Change sorting method */
	i = source_sort_method;
	g_assert(i >= 0);

	i += dir;
	if (i < 0)
		while (source_sort_methods[i + 1])
			i++;
	else if (!source_sort_methods[i])
		i = 0;

	source_sort_method = i;

	list_foreach(tree_children(&downloads), tree_resort);
	list_foreach(tree_children(&uploads), tree_resort);

	return source_sort_methods[i];
}

static const char *transfer_sort_order(const transfer_tree * t)
{
	return t->sort_methods[t->sort_method];
}

static const char *source_sort_order(void)
{
	return source_sort_methods[source_sort_method];
}
