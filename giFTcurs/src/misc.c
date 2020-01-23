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
 * $Id: misc.c,v 1.37 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "misc.h"
#include "gift.h"
#include "list.h"
#include "parse.h"

static const int STATS_INTERVAL = 5000;	/* 5 seconds */

gift_stat stats = { 0, 0, 0, 0, 0, 0 };

static void stat_handle(ntree * packet, int callback);
static void share_handle(ntree * packet, int callback);
static void attach_handle(ntree * packet);
static void stats_reset(ntree * packet, int callback);
static void message_handle(ntree * packet);

static gboolean request_stats_timeout(int callback);
static guint stats_tag = 0;
static guint stats_timeout_tag = 0;
static gboolean stats_in_progress = FALSE;

int sharing = -2;				/* 0 hide, 1 show, -1 progress, -2 initial */
static int will_change_network = FALSE;

void misc_init(void)
{
	gift_register("ATTACH", (EventCallback) attach_handle, NULL);
	gift_register("MESSAGE", (EventCallback) message_handle, NULL);
	gift_register("SHARE", (EventCallback) share_handle, GINT_TO_POINTER(-1));
}

void share_register(int callback)
{
	gift_unregister("SHARE", (EventCallback) share_handle, NULL);
	gift_register("SHARE", (EventCallback) share_handle, GINT_TO_POINTER(callback));
}

void misc_destroy(void)
{
	gift_unregister("STATS", (EventCallback) stat_handle, NULL);
	gift_unregister("DETACH", (EventCallback) stats_reset, NULL);
	gift_unregister("ATTACH", (EventCallback) stats_reset, NULL);
	gift_unregister("SHARE", (EventCallback) share_handle, NULL);
}

/* Cancel and reset stats requests. Called on ATTACH and DETACH */
static void stats_reset(ntree * unused, int callback)
{
	g_free(stats.network);
	memset(&stats, 0, sizeof stats);
	stats_in_progress = FALSE;
	if (stats_timeout_tag) {
		g_source_remove(stats_timeout_tag);
		stats_timeout_tag = 0;
	}
	ui_update(callback);
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
			will_change_network = FALSE;
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

static void stat_handle(ntree * packet, int callback)
{
	/* STATS giFT { users(0) files(266) size(25.37) } OpenFT { users(0) files(0) size(0.00) }; */
	stats.files = stats.users = 0;
	stats.bytes = 0;

	interface_foreach(packet, (PForEachFunc) stat_add_protocol, NULL);

	will_change_network = stats_in_progress = FALSE;

	if (!stats_timeout_tag)
		stats_timeout_tag =
			g_timeout_add(STATS_INTERVAL, (GSourceFunc) request_stats_timeout,
						  GINT_TO_POINTER(callback));

	ui_update(callback);
}

/* Notify the UI when STATS_INTERVAL have elapsed.
 * If in focus, it should call request_stats again to get fresh stats */
static gboolean request_stats_timeout(int callback)
{
	g_assert(stats_timeout_tag);
	stats_timeout_tag = 0;
	ui_update(callback);
	return FALSE;
}

/* Called whenever the UI wants fresh stats. Some checks is made so that
 * we don't send two packets in a row, or querying the daemon
 * more often than once every STATS_INTERVAL. */
void request_stats(int callback)
{
	ntree *packet = NULL;

	if (stats_tag == 0) {
		gift_register("STATS", (EventCallback) stat_handle, GINT_TO_POINTER(callback));
		gift_register("DETACH", (EventCallback) stats_reset, GINT_TO_POINTER(callback));
		gift_register("ATTACH", (EventCallback) stats_reset, GINT_TO_POINTER(callback));
		stats_tag = 1;
	}

	/* Check for already sent STATS or if timeout handler is installed */
	if (stats_in_progress || stats_timeout_tag)
		return;

	/* This is needed if the gift_write below causes a connect and therefore
	 * get ATTACH reply, calls attach_handle, which calls ourself again */
	stats_in_progress = TRUE;
	interface_append(&packet, "STATS", NULL);
	gift_write(&packet);
	return;
}

void stats_cycle(int callback)
{
	if (stats_timeout_tag) {
		g_source_remove(stats_timeout_tag);
		stats_timeout_tag = 0;
	}
	will_change_network = TRUE;
	request_stats(callback);
}

static void share_handle(ntree * packet, int callback)
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
			ui_update(callback);
			g_message(_("Daemon reported that sharing now is enabled."));
		}
		sharing = 1;
	} else if (!strcmp(action, "hide")) {
		if (sharing != -2 && sharing != 0) {
			sharing = 0;
			ui_update(callback);
			g_message(_("Daemon reported that sharing now is disabled."));
		}
		sharing = 0;
	} else {
		DEBUG("missing argument");
	}
}

static void attach_handle(ntree * packet)
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
}

static void message_handle(ntree * packet)
{
	/* MESSAGE(x nodes have reported a more recent OpenFT revision than you are currently using.  You are STRONGLY advised to update your node as soon as possible.  See http://www.giftproject.org/ for more details.); */
	const char *msg;

	msg = interface_value(packet);

	if (!msg)
		msg = _("never mind");

	g_message(_("There is an important message from the daemon! Check the console."));
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "And now a few words from our sponsor:\n%s", msg);
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
