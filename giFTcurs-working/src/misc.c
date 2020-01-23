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
 * $Id: misc.c,v 1.18 2002/11/22 20:48:32 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "misc.h"
#include "gift.h"
#include "protocol.h"
#include "poll.h"
#include "screen.h"
#include "ui.h"
#include "list.h"
#include "parse.h"

#define STATS_INTERVAL SECS(5)

gift_stat stats = { 0, 0, 0, 0, 0 };
list messages = LIST_INITIALIZER;

static void stat_handle(ntree * packet, void (*callback) (void));
static void share_handle(ntree * packet, void *user_data);
static void attach_handle(ntree * packet, void *user_data);
static void detach_handle(ntree * packet, void (*callback) (void));
static void message_handle(ntree * packet, void *user_data);

static tick_t request_stats(void);
int sharing = -2;				/* 0 hide, 1 show, -1 progress, -2 initial */

void misc_init(void (*stat_callback) (void))
{
	gift_register("STATS", (EventCallback) stat_handle, stat_callback);
	gift_register("SHARE", (EventCallback) share_handle, NULL);
	gift_register("ATTACH", (EventCallback) attach_handle, stat_callback);
	gift_register("MESSAGE", (EventCallback) message_handle, NULL);
	gift_register("DETACH", (EventCallback) detach_handle, stat_callback);
}

static void detach_handle(ntree * unused, void (*stat_callback) (void))
{
	memset(&stats, 0, sizeof stats);
	poll_del_timer((TFunc) request_stats);
	if (stat_callback)
		stat_callback();
}

static void stat_add_protocol(ntree * tree, void *udata)
{
	unsigned int size = 0, files = 0;
	char *key = interface_name(tree);

	files = atoi(interface_lookup(tree, "files"));

#if 0
	if ((foo = interface_lookup(tree, "size"))) {
		/* convert floating point gigs to integer megs */
		float x = atof(foo) * 1000.0;

		/* will need to change this when reaching 4 petabytes */
		if (x > (float) UINT_MAX)
			DEBUG("%f megs of shares will not fit into unsigned int", x);
		size = (unsigned int) x;
	}
#endif
	size = my_kilo_atof(interface_lookup(tree, "size"));

	if (key && strcmp(key, "giFT")) {
		stats.files += files;
		stats.megs += size;
		stats.users += my_atoi(interface_lookup(tree, "users"));
	} else {
		stats.own_files = files;
		stats.own_megs = size;
	}
}

static void stat_handle(ntree * packet, void (*callback) (void))
{
	/* STATS giFT { users(0) files(266) size(25.37) } OpenFT { users(0) files(0) size(0.00) }; */
	memset(&stats, 0, sizeof stats);
	interface_foreach(packet, (PForEachFunc) stat_add_protocol, NULL);

	if (callback)
		callback();

	poll_add_timer(STATS_INTERVAL, (TFunc) request_stats, NULL);
}

static void share_handle(ntree * packet, void *user_data)
{
	char *action;

	if (!(action = interface_lookup(packet, "action"))) {
		DEBUG("no action");
		return;
	}

	if (!strcmp(action, "sync")) {
		char *status = interface_lookup(packet, "status");

		if (!status) {
			DEBUG("missing argument");
			return;
		}
		message(_("The daemon reported the following sync status: %s"), status);
	} else if (!strcmp(action, "show")) {
		if (sharing != -2 && sharing != 1) {
			sharing = 1;
			ui_draw();			/* just to update stats or button */
			message(_("Daemon reported that sharing now is enabled."));
		}
		sharing = 1;
	} else if (!strcmp(action, "hide")) {
		if (sharing != -2 && sharing != 0) {
			sharing = 0;
			ui_draw();
			message(_("Daemon reported that sharing now is disabled."));
		}
		sharing = 0;
	} else {
		DEBUG("missing argument");
	}
}

static tick_t request_stats(void)
{
	/* Ask for stats, and starts the stats polling loop */
	ntree *packet = NULL;

	interface_append(&packet, "STATS", NULL);
	gift_write(&packet);
	return 0;
}

static void attach_handle(ntree * packet, void *user_data)
{
	static int first_time = 1;
	char *server, *version;

	server = interface_lookup(packet, "server");
	version = interface_lookup(packet, "version");

	if (!server || !version) {
		DEBUG("missing argument");
		return;
	}

	/* Skip the first, so that the notice about free software can be seen. */
	if (!first_time)
		message(_("Attached to %s v%s."), server, version);
	first_time = 0;

	/* Ask for the current share status. (hide or show) */
	packet = NULL;
	interface_append(&packet, "SHARE", NULL);
	interface_append(&packet, "action", NULL);
	gift_write(&packet);

	request_stats();
}

static void message_handle(ntree * packet, void *user_data)
{
	/* MESSAGE(x nodes have reported a more recent OpenFT revision than you are currently using.  You are STRONGLY advised to update your node as soon as possible.  See http://www.giftproject.org/ for more details.); */
	char *msg;

	msg = interface_value(packet);

	if (!msg)
		msg = _("never mind");

	asprintf(&msg, _("A message from the daemon:\n\n%s"), msg);
	list_append(&messages, msg);
	message(_("There is an important message from the daemon! It will be shown at exit."));
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
