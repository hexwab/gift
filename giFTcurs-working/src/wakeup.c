/* giFTcurs - curses interface to giFT
 * Copyright (C) 2002 Christian Häggström <chm@c00.info>
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
 * $Id: wakeup.c,v 1.14 2002/11/28 19:52:24 chnix Exp $
 */

/* This small app is meant to run in background and tries to
 * revive stalled downloads by regularily search for more sources.
 * Just a dirty hack until giFT handles this itself.
 * Also, we don't want to do this stuff in giFTcurs.
 */

#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "poll.h"
#include "parse.h"
#include "protocol.h"
#include "xcommon.h"
#include "list.h"

#define DEBUG(fmt, args...) printf(fmt "\n" , ##args)

typedef struct {
	int id;
	int search_id;
	char *hash;
	char *filename;
	unsigned int transferred;
	unsigned blessed:1;
} transfer;

static int cmp_transfer(transfer * a, transfer * b)
{
	return a->id - b->id;
}

static int next_id = 500;

static char *server_host = "localhost";
static char *server_port = "1213";
static int gift_fd;
static list downloads = LIST_INITIALIZER;

static void transfers_handle(ntree * packet);
static void download_search_more(transfer *);
static int gift_handle_line(char *buf);
static void gift_handle_read(int fd);
static void read_gift_config(void);
int gift_write(ntree * packet);

void download_incoming_source_handler(ntree * data)
{
	char *href, *user, *hash;
	unsigned int filesize;
	transfer *t;
	int i, id;
	ntree *packet = NULL;

	href = interface_lookup(data, "url");
	if (!href)
		return;
	user = interface_lookup(data, "user");
	hash = interface_lookup(data, "hash");
	filesize = my_atoi(interface_lookup(data, "size"));
	id = my_atoi(interface_value(data));

	if (!filesize || !hash || !user || !href || !id) {
		DEBUG("Source not usable.");
		return;
	}

	for (i = 0; i < downloads.num; i++) {
		t = list_index(&downloads, i);
		if (t->search_id == id)
			goto found;
	}
	DEBUG("Got source for download I dont have, cancelling search.");
	interface_append_int(&packet, "LOCATE", id);
	interface_append(&packet, "action", "cancel");
	gift_write(packet);
	interface_free(packet);
	return;

  found:
	/* just in case jasta's bug appear again :) */
	if (strcmp(t->hash, hash)) {
		DEBUG("Hash mismatch on hash search result!");
		return;
	}
	interface_append(&packet, "ADDSOURCE", NULL);
	interface_append(&packet, "user", user);
	interface_append(&packet, "hash", hash);
	interface_append_int(&packet, "size", filesize);
	interface_append(&packet, "url", href);
	interface_append(&packet, "save", t->filename);
	gift_write(packet);

	interface_free(packet);
}

static void download_search_more(transfer * t)
{
	ntree *packet = NULL;

	t->search_id = next_id++;
	interface_append_int(&packet, "LOCATE", t->search_id);
	interface_append(&packet, "query", t->hash);
	gift_write(packet);
	interface_free(packet);
}

static tick_t periodic_check(void *udata);

int init(int argc, char *argv[])
{
	ntree *packet = NULL;
	int intrvl = 300;

	if (argc > 1) {
		if (argc == 2) {
			char *endp;

			intrvl = strtol(argv[1], &endp, 10);

			if (!*endp && intrvl >= 30)
				goto ok;
		}
		printf("Usage: %s [int]  where int is number of seconds between polling giFT\n", argv[0]);
		puts("\tinterval must be at least 30 seconds, defaults to 300");
		return EXIT_FAILURE;
	}
  ok:

	/* Start timer subsystem */
	timer_init();

	/* Parse configuration file */
	read_gift_config();

	/* Initalize giFT interface */
	gift_fd = xconnect(server_host, server_port);
	if (gift_fd < 0) {
		DEBUG("Can't connect to the daemon at %s:%s", server_host, server_port);
		return EXIT_FAILURE;
	}
	DEBUG("Connected to %s:%s.", server_host, server_port);

	interface_append(&packet, "ATTACH", NULL);
	gift_write(packet);
	interface_free(packet);
	packet = NULL;

	poll_add_fd(gift_fd, gift_handle_read);

	list_sort(&downloads, (CmpFunc) cmp_transfer);

	poll_add_timer(SECS(10), periodic_check, (void *) SECS(intrvl));

	DEBUG("Checking for stalled downloads every %d seconds", intrvl);
	return 0;
}

int main(int argc, char *argv[])
{
	if (init(argc, argv))
		return EXIT_FAILURE;
	poll_forever();
	return 0;
}

static buffer the_gift_buffer = { NULL, 0, 0 };	/* The buffer we use for talking to giFT. */

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
	if (xgetlines(fd, &the_gift_buffer, gift_handle_line, find_semicolon) <= 0) {
		DEBUG("Daemon closed connection");
		exit(EXIT_FAILURE);
	}
}

int gift_write(ntree * packet)
{
	char *data;

	assert(packet);
	data = interface_construct(packet);
	xputs(data, gift_fd);
	free(data);
	return 0;
}

/* This is where we handle stuff we get from daemon. */
static int gift_handle_line(char *buf)
{
	char *command;
	ntree *parsed;
	int id;

	//DEBUG("=> %s", buf);
	if (!(parsed = interface_parse(buf))) {
		DEBUG("Does not parse: '%s'", buf);
		return -1;
	}

	if (!(command = interface_name(parsed))) {
		DEBUG("Invalid response: '%s'", buf);
		interface_free(parsed);
		return -1;
	}

	id = my_atoi(interface_value(parsed));
	if (next_id <= id)
		next_id = id + 1;
	if (strlen(command) == 11 && !strcmp(command + 3, "DOWNLOAD"))
		/* {ADD|CHG|DEL}DOWNLOAD */
		transfers_handle(parsed);
	else if (!strcmp(command, "ITEM"))
		download_incoming_source_handler(parsed);
	else
		DEBUG("Ignored response: %s", command);

	interface_free(parsed);
	return 0;
}

#include <errno.h>

void do_log(int important, const char *fmt, ...)
{
	char *p;
	va_list ap;

	if (!important)
		return;

	va_start(ap, fmt);
	if (vasprintf(&p, fmt, ap) < 0);
	return;

	if (important & 2) {
		/* more info is available in errno */
		char *tmp;

		asprintf(&tmp, "%s: %s (%d)", p, strerror(errno), errno);
		free(p);
		p = tmp;
	}

	puts(p);
	free(p);
	if (important & 4)
		/* die. */
		exit(1);
}

static int parse_configuration_line(char *line, char **section, char **name, char **value);

void read_gift_config(void)
{
	char line[256];
	char *file = malloc(strlen(getenv("HOME")) + strlen("/.giFT/ui/ui.conf") + 1);
	FILE *f;

	strcpy(file, getenv("HOME"));
	strcat(file, "/.giFT/ui/ui.conf");
	f = fopen(file, "r");
	free(file);

	if (!f)
		return;

	while (fgets(line, sizeof line, f)) {
		char *foo, *equals, *value;

		if (!parse_configuration_line(line, &foo, &equals, &value))
			continue;

		if (!strcmp(foo, "port"))
			server_port = strdup(value);
		else if (!strcmp(foo, "host"))
			server_host = strdup(value);
	}
	fclose(f);
}

static int parse_configuration_line(char *line, char **section, char **name, char **value)
{
	char *p;

	if ((p = strchr(line, '#')))
		*p = '\0';

	/* Make sure there aren't any tabs or any other junk in here. */
	trim(line);

	if (!(*section = strtok(line, " ")))
		return 0;
	if (!(*name = strtok(NULL, " ")))
		return 0;
	if (!(*value = strtok(NULL, "")))
		return 0;
	return 1;
}

static void transfers_handle(ntree * packet)
{
	int id, i, paused;
	transfer *t, tmp;
	char *hash, *filename, *command;
	unsigned int size, tx;

	command = interface_name(packet);
	id = my_atoi(interface_value(packet));

	tmp.id = id;
	i = list_find(&downloads, &tmp);

	if (!strcmp(command, "DELDOWNLOAD")) {
		if (i < 0) {
			DEBUG("unknown DELDOWNLOAD ignored.");
			return;
		}
		t = list_index(&downloads, i);
		DEBUG("Transfer %20.20s was completed/cancelled", t->filename);
		free(t->hash);
		free(t->filename);
		free(t);
		list_remove_entry(&downloads, i);
		return;
	}

	hash = interface_lookup(packet, "hash");
	size = my_atoi(interface_lookup(packet, "size"));
	paused = my_atoi(interface_lookup(packet, "paused"));
	tx = my_atoi(interface_lookup(packet, "transmit"));
	filename = interface_lookup(packet, "file");

	if (!hash || !size || !id || paused || size == tx) {
		DEBUG("Ignoring completed/paused download %20.20s.", filename);
		return;
	}

	if (i >= 0) {
		/* found */
		t = list_index(&downloads, i);
		if (!t->blessed && t->transferred != tx) {
			DEBUG("OK, %20.20s is alive.", filename);
			t->blessed = 1;
			t->transferred = tx;
		}
	} else {
		/* new */
		t = malloc(sizeof *t);
		t->hash = strdup(hash);
		t->filename = strdup(filename);
		t->transferred = tx;
		t->blessed = 1;
		t->id = id;
		list_insort(&downloads, t);
		DEBUG("Got a new transfer %20.20s.", filename);
	}
}

static tick_t periodic_check(void *udata)
{
	int i;

	DEBUG("Checking for stalled downloads...");
	for (i = 0; i < downloads.num; i++) {
		transfer *t = list_index(&downloads, i);

		if (!t->blessed) {
			DEBUG("Searching for more sources on %20.20s", t->filename);
			download_search_more(t);
		}
		t->blessed = 0;
	}
	DEBUG("...done");
	return (int) udata;
}

/* dummy function to make list.o link */
char *get_config(char *item, char *def)
{
	return def;
}
