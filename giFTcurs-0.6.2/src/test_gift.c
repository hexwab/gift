/* giFTcurs - curses interface to giFT
 * Copyright (C) 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
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
 * $Id: test_gift.c,v 1.2 2003/09/08 17:47:23 saturn Exp $
 */
#include "giftcurs.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "gift.h"
gift_id gift_new_id(void);		/* need to import this too */

#define PASS			0
#define FAIL			1
#define MAX_TIME		10

/* Set to FAIL by stuff other than test_*(). */
int other_tests = PASS;

/* We provide our own "communication" stuff. */
const char *server_host = "example.com";
const char *server_port = "1213";
const char *profile_name = "test";
int xconnect(const char *host, const char *port)
{
	/* One day we might want to do trickery and have this succeed. */
	printf("xconnect() should not be called in this test.\n");
	other_tests = FAIL;
	return -1;
}

void timeout(int sig)
{
	printf("AIEE! Aborting test after %i seconds.\n", MAX_TIME);
	exit(FAIL);
}

int test_stuff(void);

int main(int argc, char *argv[])
{
	signal(SIGALRM, timeout);
	alarm(MAX_TIME);

	return !!(test_stuff() + other_tests);
}

/* Test general cleanliness of event and ID system. */
int increase_counter(void *, void *);
int test_stuff(void)
{
	int ret = PASS;
	int counter = 0;
	int i, id = 0;

	gift_register("TEST", (EventCallback) increase_counter, &counter);

	gift_emit("TEST", GINT_TO_POINTER(42));
	if (counter != 42) {
		printf("emitting TEST did not increase counter.\n");
		ret = FAIL;
	}

	gift_cleanup();

	/* Make sure no ID's are taken. Brute force style. */
	for (i = 1; i < 32767; i++)
		if ((id = gift_new_id()) != i) {
			printf("gift_new_id() did not return %i\n", i);
			ret = FAIL;
		}

	/* TODO: Make sure everything else is cleaned up nicely. */

	return ret;
}

int increase_counter(gpointer how_much, void *counter)
{
	(*(int *) counter) += GPOINTER_TO_INT(how_much);
	return 0;
}
