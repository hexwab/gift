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
 * $Id: transfer.c,v 1.98 2002/11/28 19:52:24 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "parse.h"
#include "get.h"
#include "transfer.h"
#include "gift.h"
#include "screen.h"
#include "protocol.h"
#include "settings.h"

/* This should never be used. It's only here so it can be translated. */
static __attribute__ ((unused))
const char *_statuses[] = {
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
	/*N_("Queued (position %s)"), */
	N_("Sending"),
	N_("Received HTTP PUSH"),
	N_("Connection closed"),
	N_("Connection refused"),
	/* FIXME: Must get rid of this list */
};

#define BANDWIDTH_TIMER SECS(2)

int autodestroy_all = 0;
tree downloads = TREE_INITIALIZER;
tree uploads = TREE_INITIALIZER;

static int nr_alive = 0;

static void transfer_change_handle(ntree * packet, void *udata);
static void transfer_add_handle(ntree * packet, void *udata);
static void addsource_parse(ntree * packet, transfer *);
static int transfer_compare_size(transfer * a, transfer * b);
static tick_t bandwidth_timer(void *udata);
static void transfer_update_bw(transfer * t, unsigned int current, tick_t dt);
static void transfer_detach_all(ntree *, void *udata);

void transfers_init(void)
{
	autodestroy_all = atoi(get_config("set", "autoclear", "0"));

	gift_register("ADDDOWNLOAD", (EventCallback) transfer_add_handle, (void *) TRANSFER_DOWNLOAD);
	gift_register("ADDUPLOAD", (EventCallback) transfer_add_handle, (void *) TRANSFER_UPLOAD);
	gift_register("DETACH", (EventCallback) transfer_detach_all, NULL);
	tree_sort(&uploads, (CmpFunc) transfer_compare_size);
	tree_sort(&downloads, (CmpFunc) transfer_compare_size);
}

void source_destroy(source * s)
{
	free(s->user);
	free(s->node);
	free(s->href);
	free(s->pretty);
	free(s->status);
	free(s);
}

void source_touch(source * s)
{
	TOUCH(s);
}

void transfer_destroy(transfer * t)
{
	transfer_detach(t);
	free(t->protocol);
	free(t->filename);
	free(t->pretty);
	free(t->hash);
	free(t->status);
	free(t);
}

void transfer_touch(transfer * t)
{
	TOUCH(t);
	if (t->type == TRANSFER_DOWNLOAD)
		list_foreach(&t->sourcen, (LFunc) source_touch);
}

void transfer_detach(transfer * t)
{
	transfer_touch(t);

	t->paused = 0;

	if (t->id) {
		gift_unregister_id(t->id);
		t->id = 0;

		if (--nr_alive == 0)
			poll_del_timer(bandwidth_timer);
	}

	list_destroy_all(&t->sourcen);

	if (t->search_id) {
		gift_search_stop(t->search_id, "LOCATE");
		t->search_id = 0;
	}
}

static void transfer_detach_panic(transfer * t)
{
	/* This is called when giFT disconnects without a word. */
	if (t->id) {
		free(t->status);
		t->status = strdup(N_("Unknown status"));
	}
	transfer_detach(t);
}

static void transfer_detach_all(ntree * unused, void *udata)
{
	list_foreach(&downloads.top, (LFunc) transfer_detach_panic);
	list_foreach(&uploads.top, (LFunc) transfer_detach_panic);
	if (downloads.update_ui)
		downloads.update_ui();
	if (uploads.update_ui)
		uploads.update_ui();
}

void transfer_forget(transfer * t)
{
	tree *transfer_tree = t->type == TRANSFER_DOWNLOAD ? &downloads : &uploads;

	tree_destroy_item(transfer_tree, t);
}

static transfer *transfer_new(gift_id id, int type, char *name, unsigned int size)
{
	transfer *t = NEW(transfer);

	t->id = id;
	t->filename = strdup(name);
	t->filesize = size;
	t->type = type;
	t->protocol = NULL;
	list_initialize(&t->sourcen);
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
	t->transferred = t->bandwidth = 0;
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
transfer *transfer_find(tree * tr, char *hash, unsigned int size, int dead)
{
	int i;

	if (!hash)
		return NULL;

	for (i = 0; i < tr->top.num; i++) {
		transfer *t = list_index(&tr->top, i);

		if (dead && t->id)
			continue;
		if (t->filesize == size && t->hash && !strcmp(t->hash, hash))
			return t;
	}
	return NULL;
}

transfer *transfer_find_id(tree * tr, gift_id id)
{
	int i;

	for (i = 0; i < tr->top.num; i++) {
		transfer *t = list_index(&tr->top, i);

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

	for (i = 0; i < t->sourcen.num; i++) {
		source *s = list_index(&t->sourcen, i);

		if (!strcmp(user, s->user) && !strcmp(href, s->href))
			return s;
	}

	return NULL;
}

static void source_change_parse(ntree * param, transfer * t)
{
	char *user, *href, *status;
	source *s;

	if (strcasecmp(interface_name(param), "SOURCE"))
		return;

	user = interface_lookup(param, "user");
	href = interface_lookup(param, "url");
	/* FIXME: use status if there is no action. I hope this is only for
	   backwards compatibility. */
	if (!(status = interface_lookup(param, "action")))
		status = interface_lookup(param, "status");

	if (!user || !href || !status) {
		DEBUG("Bad SOURCE.");
		return;
	}

	if (!(s = lookup_source(t, user, href))) {
		DEBUG("A source I didn't know about was changed");
		return;
	}

	free(s->status);
	s->status = strdup(status);
	s->start = my_atoi(interface_lookup(param, "start"));
	s->transmit = my_atoi(interface_lookup(param, "transmit"));
	s->total = my_atoi(interface_lookup(param, "total"));
	TOUCH(s);
}

static void addsource_parse(ntree * param, transfer * t)
{
	char *user, *href, *status;
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
	s = NEW(source);
	s->parent = t;
	s->user = strdup(user);
	s->node = my_strdup(interface_lookup(param, "node"));
	s->href = strdup(href);
	list_append(&t->sourcen, s);
	if (!t->protocol && strchr(href, ':'))
		t->protocol = strndup(href, strchr(href, ':') - href);

	free(s->status);
	s->status = strdup(status);
	s->start = my_atoi(interface_lookup(param, "start"));
	s->transmit = my_atoi(interface_lookup(param, "transmit"));
	s->total = my_atoi(interface_lookup(param, "total"));
	TOUCH(s);
}

static void transfer_update_fields(transfer * t, ntree * param)
{
	char *state = interface_lookup(param, "state");

	t->transferred = my_atoi(interface_lookup(param, "transmit"));

	if (!state)
		return;

	if (!t->status || strcmp(state, t->status)) {
		free(t->status);
		t->status = strdup(state);
		t->paused = !strcasecmp(state, "paused");
	}
}

static void transfer_add_handle(ntree * param, void *udata)
{
	gift_id id;
	tree *transfer_tree;
	int transfer_type = (int) udata;
	transfer *t;
	char *filename;
	char *hash;
	unsigned int size;

	id = my_atoi(interface_value(param));

	if (!id) {
		DEBUG("Daemon sent a spooky transfer with a zero id.");
		return;
	}

	transfer_tree = transfer_type == TRANSFER_DOWNLOAD ? &downloads : &uploads;

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
	if ((t = transfer_find(transfer_tree, hash, size, 1))) {
		free(t->filename);
		t->filename = strdup(filename);
		t->id = id;
	} else {
		t = transfer_new(id, transfer_type, filename, size);
		t->hash = my_strdup(hash);
		list_insort(&transfer_tree->top, t);
	}

	transfer_update_fields(t, param);

	if (transfer_type == TRANSFER_UPLOAD) {
		if (!my_atoi(interface_lookup(param, "shared")))
			t->autodestroy = 1;
	}

	interface_foreach(param, (PForEachFunc) addsource_parse, t);

	TOUCH(t);

	if (transfer_tree->update_ui)
		transfer_tree->update_ui();

	if (nr_alive++ == 0)
		poll_add_timer(BANDWIDTH_TIMER, bandwidth_timer, NULL);

	gift_register_id(id, (EventCallback) transfer_change_handle, t);
}

static void transfer_change_handle(ntree * param, void *udata)
{
	tree *transfer_tree;
	int transfer_type;
	transfer *t = udata;
	char *command = interface_name(param);

	transfer_type = t->type;
	transfer_tree = transfer_type == TRANSFER_DOWNLOAD ? &downloads : &uploads;

	if (!strcmp(command, "ADDSOURCE")) {
		addsource_parse(param, t);
	} else if (!strcmp(command, "DELSOURCE")) {
		char *user, *href, *status;
		source *s;
		int i;

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

		i = list_find(&t->sourcen, s);
		assert(i >= 0);
		source_destroy(s);
		list_remove_entry(&t->sourcen, i);
	} else if (strstr(command, "DEL")) {
		transfer_detach(t);

		if (transfer_type == TRANSFER_DOWNLOAD)
			message(t->transferred == t->filesize ? _("Download of %s completed.") :
					_("Download of %s cancelled."), t->filename);

		if (autodestroy_all || t->autodestroy)
			transfer_forget(t);
		else if (t->transferred < t->filesize) {
			free(t->status);
			t->status = strdup(N_("Cancelled"));
		}
	} else {					/* CHG* */
		unsigned int throughput, elapsed;

		transfer_update_fields(t, param);

		throughput = my_atoi(interface_lookup(param, "throughput"));
		elapsed = my_atoi(interface_lookup(param, "elapsed"));
		if (elapsed)
			transfer_update_bw(t, 1000 * throughput / elapsed, SECS(elapsed) / 1000);
		t->last_time = uptime();

		interface_foreach(param, (PForEachFunc) source_change_parse, t);
	}

	TOUCH(t);

	if (transfer_tree->update_ui)
		transfer_tree->update_ui();
}

/* How long does it take for a stalled download to reach half bandwidth */
#define EWMA SECS(5)

/* This function tries to estimate the bandwidth a transfer has. To
 * consider is that a new transfer can start in the middle of a file, so
 * we don't want it to say 250MB/s just because it started 250MB into the
 * file. Another thing to consider is that the time between calculations
 * might be zero.
 */
static void transfer_update_bw(transfer * t, unsigned int current, tick_t dt)
{
	int diff = current - t->bandwidth;

	if (!diff)
		return;
	/* should use real exp() here, but since dt we don't care that much,
	 * a linear approximation can do fine */
	if (!t->bandwidth)
		t->bandwidth = current;
	else if (dt > EWMA)
		t->bandwidth += diff / 2;
	else {
		int change = (int) dt * diff / (int) (2 * EWMA);

		if (change == 0 && t->bandwidth)
			t->bandwidth--;
		else
			t->bandwidth += change;
	}
}

static int transfer_calculate(transfer * t)
{
	if (t->paused) {
		t->bandwidth = 0;
		return 0;
	}

	if (uptime() - t->last_time < BANDWIDTH_TIMER || t->bandwidth == 0)
		return 0;

	/* Transfer is stalled. Start cutting down the bandwidth */
	transfer_update_bw(t, 0, BANDWIDTH_TIMER);

	t->last_time = uptime();

	TOUCH(t);
	return 1;
}

static tick_t bandwidth_timer(void *udata)
{
	int changed, i;

	for (changed = 0, i = 0; i < downloads.top.num; i++)
		changed |= transfer_calculate(list_index(&downloads.top, i));
	if (changed && downloads.update_ui)
		downloads.update_ui();

	for (changed = 0, i = 0; i < uploads.top.num; i++)
		changed |= transfer_calculate(list_index(&uploads.top, i));
	if (changed && uploads.update_ui)
		uploads.update_ui();
	return BANDWIDTH_TIMER;
}

static int transfer_compare_size(transfer * a, transfer * b)
{
	if (a->filesize < b->filesize)
		return -1;
	if (a->filesize > b->filesize)
		return 1;
	return compare_pointers(a, b);
}
