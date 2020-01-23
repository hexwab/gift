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
 * $Id: misc.c,v 1.31 2003/05/11 01:58:28 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "misc.h"
#include "gift.h"
#include "protocol.h"
#include "screen.h"
#include "ui.h"
#include "list.h"
#include "parse.h"

static const int STATS_INTERVAL = 5000; /* 5 seconds */

gift_stat stats = { 0, 0, 0, 0, 0, 0 };
list messages = LIST_INITIALIZER;

static void stat_handle(ntree * packet, void (*callback) (void));
static void share_handle(ntree * packet, void *user_data);
static void attach_handle(ntree * packet, void *user_data);
static void detach_handle(ntree * packet, void (*callback) (void));
static void message_handle(ntree * packet, void *user_data);

static gboolean request_stats(void *data);
static guint stats_tag = 0;
int sharing = -2;				/* 0 hide, 1 show, -1 progress, -2 initial */
static int will_change_network = 0;

void misc_init(void (*stat_callback) (void))
{
	gift_register("STATS", (EventCallback) stat_handle, stat_callback);
	gift_register("SHARE", (EventCallback) share_handle, NULL);
	gift_register("ATTACH", (EventCallback) attach_handle, NULL);
	gift_register("MESSAGE", (EventCallback) message_handle, NULL);
	gift_register("DETACH", (EventCallback) detach_handle, stat_callback);
}

void misc_destroy(void)
{
	/* This may look as a no-op, but it prevents further updates of the ui */
	gift_unregister("STATS", (EventCallback) stat_handle, NULL);
	gift_unregister("DETACH", (EventCallback) detach_handle, NULL);
	gift_register("STATS", (EventCallback) stat_handle, NULL);
	gift_register("DETACH", (EventCallback) detach_handle, NULL);
}

static void detach_handle(ntree * unused, void (*stat_callback) (void))
{
	g_free(stats.network);
	memset(&stats, 0, sizeof stats);
	if (stats_tag) {
		g_source_remove(stats_tag);
		stats_tag = 0;
	}
	if (stat_callback)
		stat_callback();
}

static void stat_add_protocol(ntree * tree, void *udata)
{
	guint64 size;
	unsigned int files, users;
	const char *key = interface_name(tree);

	files = my_atoi(interface_lookup(tree, "files"));
	size = my_giga_atof(interface_lookup(tree, "size"));
	users = my_atoi(interface_lookup(tree, "users"));

	if (!strcmp(key, "giFT")) {
		stats.own_files = files;
		stats.own_bytes = size;
		return;
	}
	if (will_change_network) {
		if (!stats.network) {
			stats.network = g_strdup(key);
			stats.files = stats.users = 0;
			stats.bytes = 0;
			will_change_network = 0;
		} else if (!strcmp(key, stats.network)) {
			/* switch to the next network */
			g_free(stats.network);
			stats.network = NULL;
		}
	} else if (stats.network && strcmp(key, stats.network)) {
		return;
	}
	stats.files += files;
	stats.bytes += size;
	stats.users += users;
}

static void stat_handle(ntree * packet, void (*callback) (void))
{
	/* STATS giFT { users(0) files(266) size(25.37) } OpenFT { users(0) files(0) size(0.00) }; */
	stats.files = stats.users = 0;
	stats.bytes = 0;

	interface_foreach(packet, (PForEachFunc) stat_add_protocol, NULL);

	will_change_network = 0;

	if (callback)
		callback();

	stats_tag = g_timeout_add(STATS_INTERVAL, request_stats, NULL);
}

static gboolean request_stats(void *data)
{
	/* Ask for stats, and starts the stats polling loop */
	ntree *packet = NULL;
	static int recursion_depth = 0;

	/* This is needed if the gift_write below causes a connect and therefore
	 * get ATTACH reply, calls attach_handle, which calls ourself again */
	if (recursion_depth)
		return 0;

	recursion_depth++;
	interface_append(&packet, "STATS", NULL);
	gift_write(&packet);
	recursion_depth--;
	return 0;
}

void stats_cycle(void)
{
	if (stats_tag) {
		g_source_remove(stats_tag);
		stats_tag = 0;
	}
	will_change_network = 1;
	request_stats(NULL);
}

static void share_handle(ntree * packet, void *user_data)
{
	const char *action;

	if (!(action = interface_lookup(packet, "action"))) {
		DEBUG("no action");
		return;
	}

	if (!strcmp(action, "sync")) {
		const char *status = interface_lookup(packet, "status");

		if (!status) {
			DEBUG("missing argument");
			return;
		}
		g_message(_("The daemon reported the following sync status: %s"), status);
	} else if (!strcmp(action, "show")) {
		if (sharing != -2 && sharing != 1) {
			sharing = 1;
			ui_draw();			/* just to update stats or button */
			g_message(_("Daemon reported that sharing now is enabled."));
		}
		sharing = 1;
	} else if (!strcmp(action, "hide")) {
		if (sharing != -2 && sharing != 0) {
			sharing = 0;
			ui_draw();
			g_message(_("Daemon reported that sharing now is disabled."));
		}
		sharing = 0;
	} else {
		DEBUG("missing argument");
	}
}

static void attach_handle(ntree * packet, void *user_data)
{
	static int first_time = 1;
	const char *server, *version;

	server = interface_lookup(packet, "server");
	version = interface_lookup(packet, "version");

	if (!server || !version) {
		DEBUG("missing argument");
		return;
	}

	/* Skip the first, so that the notice about free software can be seen. */
	if (!first_time)
		g_message(_("Attached to %s v%s."), server, version);
	first_time = 0;

	/* Ask for the current share status. (hide or show) */
	packet = NULL;
	interface_append(&packet, "SHARE", NULL);
	interface_append(&packet, "action", NULL);
	gift_write(&packet);

	request_stats(NULL);
}

static void message_handle(ntree * packet, void *user_data)
{
	/* MESSAGE(x nodes have reported a more recent OpenFT revision than you are currently using.  You are STRONGLY advised to update your node as soon as possible.  See http://www.giftproject.org/ for more details.); */
	const char *msg;
	char *msg2;

	msg = interface_value(packet);

	if (!msg)
		msg = _("never mind");

	msg2 = g_strdup_printf(_("A message from the daemon:\n\n%s"), msg);
	list_append(&messages, msg2);
	g_message(_("There is an important message from the daemon! It will be shown at exit."));
}

void sharing_toggle(void)
{
	ntree *packet = NULL;

	interface_append(&packet, "SHARE", NULL);
	interface_append(&packet, "action", sharing ? "hide" : "show");
	gift_write(&packet);

	sharing = -1;				/* unsure until we get acknowledge from daemon */
}

void sharing_sync(void)
{
	ntree *packet = NULL;

	interface_append(&packet, "SHARE", NULL);
	interface_append(&packet, "action", "sync");
	gift_write(&packet);
}
