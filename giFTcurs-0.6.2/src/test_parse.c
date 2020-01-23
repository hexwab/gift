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
 * $Id: test_parse.c,v 1.6 2003/08/19 14:54:17 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "parse.h"

#define FAIL			1
#define PASS			0
#define MAX_TIME		10
#define rnd(min,max)	(min + (int) ((float) max * rand() / (RAND_MAX + 1.0)))

int test_bitmap(void);
int test_parse_typed_query(void);
int test_remove_word(void);

void timeout(int sig)
{
	printf("AIEE! Aborting test after %i seconds.\n", MAX_TIME);
	exit(FAIL);
}

int main(int argc, char *argv[])
{
	signal(SIGALRM, timeout);
	alarm(MAX_TIME);
	srand(time(NULL));
	return !!(test_bitmap() + test_parse_typed_query() + test_remove_word());
}

/* Test bitmap_set and bitmap_find_unset. */
#define test_size		4096
int test_bitmap(void)
{
	int ret = PASS;
	guchar id_bitmap[test_size / 8] = { 0 };
	int i;

	/* Set everything to TRUE */
	for (i = 0; i < test_size; i++)
		bitmap_set(id_bitmap, sizeof id_bitmap, i, TRUE);
	if ((i = bitmap_find_unset(id_bitmap, sizeof id_bitmap)) != -1) {
		printf("bitmap: bitmap_find_unset found unset bit at %i\n", i);
		ret = FAIL;
	}
	for (i = 0; i < sizeof id_bitmap; i++)
		if (id_bitmap[i] != 0xff) {
			printf("bitmap: id_bitmap[%i] == %i\n", i, id_bitmap[i]);
			ret = FAIL;
		}

	/* The bitmap is full of truth, unset some stuff and test it. */
	for (i = 0; i < 256; i++) {
		int foo = rnd(0, test_size), bar;

		bitmap_set(id_bitmap, sizeof id_bitmap, foo, FALSE);
		if ((bar = bitmap_find_unset(id_bitmap, sizeof id_bitmap)) != foo) {
			printf("bitmap_set unset bit %i instead of %i\n", bar, foo);
			ret = 1;
		}
		bitmap_set(id_bitmap, sizeof id_bitmap, foo, TRUE);
	}

	/* Set every other bit and see if bitmap_get works. */
	for (i = 0; i < test_size; i += 2)
		bitmap_set(id_bitmap, sizeof id_bitmap, i, FALSE);
	for (i = 0; i < test_size; i++) {
		int foo = bitmap_get(id_bitmap, sizeof id_bitmap, i);

		if ((foo && i % 2 == 0) || (!foo && i % 2 == 1)) {
			printf("bitmap_get returned bit %i wrong\n", i);
			ret = 1;
		}
	}

	return ret;
}

int test_parse_typed_query(void)
{
	int ret = PASS;
	/* *INDENT-OFF* */
	struct {
		const char *test;		/* test value */
		const char *includes;	/* expected values */
		const char *excludes;
		const char *protocols;
	} tests[] = {
		{"foo","foo","",""},
		{"-foo","","foo",""},
		{"foo -bar","foo","bar",""},
		{"foo -bar -fnord","foo","bar fnord",""},
		{"-foo -bar fnord -eris","fnord","foo bar eris",""},
		{"pro -anti protocol:one,two","pro","anti","one two"},
		{"pro -anti protocol:one,two protocol:three","pro","anti","one two three"},
		{"","","",""},
	};
	/* *INDENT-ON* */
	char *includes, *excludes, *protocols;
	int i;

	for (i = 0; i < G_N_ELEMENTS(tests); i++) {
		parse_typed_query(tests[i].test, &includes, &excludes, &protocols);
		if (strcmp(includes, tests[i].includes) || strcmp(excludes, tests[i].excludes) || strcmp(protocols, tests[i].protocols)) {
			printf("parse_typed_query for '%s' returned ('%s', '%s', '%s'); expected ('%s', '%s', '%s')\n",
				   tests[i].test, includes, excludes, protocols, tests[i].includes, tests[i].excludes, tests[i].protocols);
			ret = FAIL;
		}
		g_free(includes);
		g_free(excludes);
		g_free(protocols);
	}

	return ret;
}

int test_remove_word(void)
{
	int ret = PASS;
	/* *INDENT-OFF* */
	struct {
		const char *string;	/* test value */
		int pos;			/* test value */
		const char *e_str;
		int e_pos;
	} tests[] = {
		{"foo", 3, "", 0},
		{"foo", 2, "o", 0},
		{"foo bar", 3, " bar", 0},
		{"foo bar", 7, "foo ", 4},
		{"debian uber alles", 12, "debian alles", 7},
		{"foo   ", 6, "", 0},
		{"", 0, "", 0},
	};
	/* *INDENT-ON* */
	int pos;
	GString *gs;
	int i;

	for (i = 0; i < G_N_ELEMENTS(tests); i++) {
		gs = g_string_new(tests[i].string);
		pos = tests[i].pos;
		remove_word(gs, &pos);
		if (pos != tests[i].e_pos || strcmp(gs->str, tests[i].e_str)) {
			printf("test_remove_word for ('%s',%i) returned ('%s',%i); expected ('%s',%i)\n",
				   tests[i].string, tests[i].pos, gs->str, pos, tests[i].e_str, tests[i].e_pos);
			ret = FAIL;
		}
		g_string_free(gs, TRUE);
	}

	return ret;
}
