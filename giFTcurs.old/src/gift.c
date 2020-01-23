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
 * $Id: gift.c,v 1.196 2003/05/16 10:05:31 chnix Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include "parse.h"
#include "gift.h"
#include "protocol.h"
#include "screen.h"
#include "list.h"
#include "xcommon.h"

static const int RECONNECT_INTERVAL = 30000;	/* 30 seconds */

static int gift_attach(void);
static void gift_detach(void);
static int gift_handle_line(char *buf);
static gboolean reconnect(void *data);

static guint reconnect_tag = 0;
static GIOChannel *gift_stream = NULL;	/* The connection we maintain with giFT. */
static gift_id next_id = 900;	/* Next unused ID. */

static list callbacks = LIST_INITIALIZER;

typedef struct {
	char *command;
	EventCallback cb;
	void *udata;
} event_callback;

gift_id gift_new_id(void)
{
	return next_id++;
}

void gift_claimed_id(gift_id id)
{
	if (id >= next_id)
		next_id = id + 1;
	//DEBUG("Next ID would be: %u", next_id);
}

void gift_release_id(gift_id id)
{
	/* XXX: if we want a table of unavailable id's.. */
	/* We have 4 billion, isn't that enough? */
	//DEBUG("Released ID %u", id);
}

static gboolean gift_handle_read(GIOChannel * source, GIOCondition condition, gpointer data)
{
	char *line;

	if (g_io_channel_read_line(source, &line, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
		gift_detach();
		g_message(_("The connection with the daemon was unexpectedly closed!"));
		reconnect_tag = g_timeout_add(RECONNECT_INTERVAL, reconnect, NULL);
		return FALSE;
	}
	gift_handle_line(line);
	g_free(line);
	return TRUE;
}

static int gift_attach(void)
{
	ntree *packet = NULL;
	int fd;

	if (gift_stream)
		return 1;

	if ((fd = xconnect(server_host, server_port)) < 0) {
		g_message(_("Can't connect to the daemon at %s:%s"), server_host, server_port);
		return -1;
	}
	DEBUG("Connected to %s:%s.", server_host, server_port);

	next_id = 900;				/* use high number to minimize id collisions */

	gift_stream = g_io_channel_unix_new(fd);

	/* Note: This violates the protocol as described in the docs. But using
	 * plain ";" as line terminator would break in places where ; is escaped.
	 * Hopefully giFT will always send a newline after the semicolon. */
	g_io_channel_set_encoding(gift_stream, NULL, NULL);
	g_io_channel_set_line_term(gift_stream, ";\n", -1);

	interface_append(&packet, "ATTACH", NULL);
	interface_append(&packet, "client", PACKAGE);
	interface_append(&packet, "version", VERSION);
	if (profile_name && profile_name[0])
		interface_append(&packet, "profile", profile_name);
	gift_write(&packet);

	g_io_add_watch(gift_stream, G_IO_IN, gift_handle_read, NULL);

	return 0;
}

static void gift_detach(void)
{
	if (gift_stream == NULL)
		return;

	gift_emit("DETACH", NULL);

	g_io_channel_shutdown(gift_stream, FALSE, NULL);
	g_io_channel_unref(gift_stream);
	gift_stream = NULL;
}

int gift_write(ntree ** packet)
{
	char *data;

	g_assert(*packet);

	if (gift_attach() < 0) {
		interface_free(*packet);
		*packet = NULL;
		return -1;
	}

	data = interface_construct(*packet);
	interface_free(*packet);
	*packet = NULL;
	DEBUG("<= %s", data);
	g_io_channel_write_chars(gift_stream, data, -1, NULL, NULL);
	g_io_channel_flush(gift_stream, NULL);
	g_free(data);
	return 0;
}

void gift_cleanup(void)
{
	gift_detach();
	if (reconnect_tag) {
		g_source_remove(reconnect_tag);
		reconnect_tag = 0;
	}
	list_free_entries(&callbacks);
}

void gift_init(void)
{
	if (gift_attach() < 0)
		reconnect_tag = g_timeout_add(RECONNECT_INTERVAL, reconnect, NULL);
}

static gboolean reconnect(void *data)
{
	return gift_attach() < 0;
}

void gift_register(const char *command, EventCallback cb, void *udata)
{
	event_callback *foo = g_new(event_callback, 1);

	g_assert(command);
	g_assert(cb);

	foo->command = g_strdup(command);
	foo->cb = cb;
	foo->udata = udata;
	list_append(&callbacks, foo);
}

void gift_register_id(gift_id id, EventCallback cb, void *udata)
{
	gift_register(itoa(id), cb, udata);
}

/* A NULL in one parameter acts as a wildcard */
void gift_unregister(const char *command, EventCallback cb, void *udata)
{
	int i;

	for (i = 0; i < callbacks.num; i++) {
		event_callback *foo = list_index(&callbacks, i);

		if (command && strcmp(command, foo->command))
			continue;
		if (cb && cb != foo->cb)
			continue;
		if (udata && udata != foo->udata)
			continue;
		g_free(foo->command);
		list_remove_entry(&callbacks, i);
		i--;
	}
}

void gift_unregister_id(gift_id id)
{
	gift_release_id(id);
	gift_unregister(itoa(id), NULL, NULL);
}

int gift_emit(const char *command, void *tree)
{
	int i, n = 0;

	for (i = 0; i < callbacks.num; i++) {
		event_callback *foo = list_index(&callbacks, i);

		if (!strcmp(foo->command, command)) {
			foo->cb(tree, foo->udata);
			n++;
		}
	}
	return n;
}

/* This is where we handle stuff we get from daemon. */
static int gift_handle_line(char *buf)
{
	const char *command, *id;
	ntree *parsed;
	int handled = 0;

	DEBUG("=> %s", buf);

	if (!(parsed = interface_parse(buf))) {
		DEBUG("Does not parse: '%s'", buf);
		return -1;
	}

	if (!(command = interface_name(parsed))) {
		DEBUG("Invalid command: '%s'", buf);
		interface_free(parsed);
		return -1;
	}

	id = interface_value(parsed);

	if (id) {
		gift_claimed_id(atoi(id));
		handled = gift_emit(id, parsed);
	}

	handled += gift_emit(command, parsed);

	if (!handled)
		DEBUG("Unhandled command: %s", command);

	interface_free(parsed);
	return 0;
}
