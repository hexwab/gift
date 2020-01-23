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
 * $Id: gift.c,v 1.181 2002/11/22 20:48:31 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include "parse.h"
#include "gift.h"
#include "protocol.h"
#include "screen.h"
#include "poll.h"
#include "list.h"
#include "xcommon.h"

#define RECONNECT_INTERVAL SECS(30)

static int gift_attach(void);
static void gift_detach(void);
static int gift_handle_line(char *buf);
static tick_t reconnect(void *data);

static int the_gift_fd = -1;	/* The connection we maintain with giFT. */
static buffer the_gift_buffer = { NULL, 0, 0 };	/* The buffer we use for talking to giFT. */
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

static char *find_semicolon(char *buf)
{
	/* find the semicolon that ends the command */
	/* watch out for escaped semicolons */
	do {
		if (*buf == '\\')
			buf++;
		else if (*buf == ';')
			return buf;
	} while (*buf++);
	return NULL;
}

static void gift_handle_read(int fd)
{
	assert(fd == the_gift_fd);

	if (xgetlines(fd, &the_gift_buffer, gift_handle_line, find_semicolon) <= 0) {
		gift_detach();
		message(_("The connection with the daemon was unexpectedly closed!"));
		poll_add_timer(RECONNECT_INTERVAL, reconnect, NULL);
		return;
	}
}

static int gift_attach(void)
{
	ntree *packet = NULL;

	if (the_gift_fd != -1)
		return 1;

	if ((the_gift_fd = xconnect(server_host, server_port)) < 0) {
		error_message(_("Can't connect to the daemon at %s:%s"), server_host, server_port);
		return -1;
	}
	DEBUG("Connected to %s:%s.", server_host, server_port);

	/* Init the buffer */
	free(the_gift_buffer.buf);
	memset(&the_gift_buffer, 0, sizeof(the_gift_buffer));

	next_id = 900;				/* use high number to minimize id collisions */

	interface_append(&packet, "ATTACH", NULL);
	interface_append(&packet, "client", PACKAGE);
	interface_append(&packet, "version", VERSION);
	if (profile_name)
		interface_append(&packet, "profile", profile_name);
	gift_write(&packet);

	poll_del_timer(reconnect);
	poll_add_fd(the_gift_fd, gift_handle_read);

	return 0;
}

static void gift_detach(void)
{
	if (the_gift_fd == -1)
		return;

	gift_emit("DETACH", NULL);

	close(the_gift_fd);
	poll_del_fd(the_gift_fd);
	the_gift_fd = -1;
	free(the_gift_buffer.buf);
	memset(&the_gift_buffer, 0, sizeof(the_gift_buffer));
}

int gift_write(ntree ** packet)
{
	char *data;

	assert(*packet);
	if (gift_attach() < 0) {
		interface_free(*packet);
		*packet = NULL;
		return -1;
	}

	data = interface_construct(*packet);
	interface_free(*packet);
	*packet = NULL;
	DEBUG("<= %s", data);
	xputs(data, the_gift_fd);
	free(data);
	return 0;
}

void gift_cleanup(void)
{
	gift_detach();
	poll_del_timer(reconnect);
	list_free_entries(&callbacks);
}

void gift_init(void)
{
	if (gift_attach() < 0)
		poll_add_timer(RECONNECT_INTERVAL, reconnect, NULL);
}


static tick_t reconnect(void *data)
{
	return gift_attach() < 0 ? RECONNECT_INTERVAL : 0;
}

void gift_register(char *command, EventCallback cb, void *udata)
{
	event_callback *foo = malloc(sizeof(event_callback));

	assert(command);
	assert(cb);

	foo->command = strdup(command);
	foo->cb = cb;
	foo->udata = udata;
	list_append(&callbacks, foo);
}

void gift_register_id(gift_id id, EventCallback cb, void *udata)
{
	char num[15];

	sprintf(num, "%d", id);
	gift_register(num, cb, udata);
}

/* A NULL in one parameter acts as a wildcard */
void gift_unregister(char *command, EventCallback cb, void *udata)
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
		free(foo->command);
		list_remove_entry(&callbacks, i);
		i--;
	}
}

void gift_unregister_id(gift_id id)
{
	char num[15];

	gift_release_id(id);
	sprintf(num, "%d", id);
	gift_unregister(num, NULL, NULL);
}

int gift_emit(char *command, void *tree)
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
	char *command, *id;
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
