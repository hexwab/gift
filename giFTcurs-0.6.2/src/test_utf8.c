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
 * $Id: test_utf8.c,v 1.4 2003/11/20 15:32:50 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <locale.h>

#include "screen.h"
#include "parse.h"

#define FAIL			1
#define PASS			0
#define MAX_TIME		10

static int test_vstrlen(void);
static int test_str_occupy(void);
static int test_addstr(void);

static void timeout_(int sig)
{
	printf("AIEE! Aborting test after %i seconds.\n", MAX_TIME);
	exit(FAIL);
}

int main(int argc, char *argv[])
{
	int ok = PASS;

	signal(SIGALRM, timeout_);
	alarm(MAX_TIME);
	srand(time(NULL));

	utf8 = TRUE;

	if (test_vstrlen() == FAIL)
		ok = FAIL;
	if (test_str_occupy() == FAIL)
		ok = FAIL;
#if 0							/* FIXME: Doesn't seem to work for some strange reason */
	if (test_addstr() == FAIL)
		ok = FAIL;
#endif
	return ok;
}

/* *INDENT-OFF* */
static struct {
	const char *test_name;
	const char *test;		/* test value */
	int expected_len;	/* expected values */
} tests[] = {
	/* swedish characters */
	{"Swedish umlauts", "\xc3\xa5\xc3\xa4\xc3\xb6", 3},
	/* combining characters */
	{"Combining umlauts", "Mo\xcc\x88t mig ha\xcc\x88r", 11},
	/* wide character */
	{"Wide character", "\xe6\x83\x85", 2},
};
/* *INDENT-ON* */

static int test_vstrlen(void)
{
	int ret = PASS;
	int i;

	for (i = 0; i < G_N_ELEMENTS(tests); i++) {
		int returned;

		returned = vstrlen(tests[i].test);
		if (returned != tests[i].expected_len) {
			printf("vstrlen for test of %s returned %d; expected %d\n",
				   tests[i].test_name, returned, tests[i].expected_len);
			ret = FAIL;
		}
	}

	return ret;
}

static int test_str_occupy(void)
{
	int ret = PASS;
	int i, j;

	for (i = 0; i < G_N_ELEMENTS(tests); i++) {
		int lim = vstrlen(tests[i].test);

		g_assert(strlen(tests[i].test) < 255);

		for (j = 0; j < lim; j++) {
			char buf[256];
			int offset_lo, offset_hi, returned_lo, returned_hi;

			strcpy(buf, tests[i].test);
			offset_hi = str_occupy(tests[i].test, j, 1);
			buf[offset_hi] = '\0';
			returned_hi = vstrlen(buf);

			strcpy(buf, tests[i].test);
			offset_lo = str_occupy(tests[i].test, j, 0);
			buf[offset_lo] = '\0';
			returned_lo = vstrlen(buf);

			if (offset_hi == offset_lo) {
				/* Not a multibyte boundary */
				if (j != returned_hi || j != returned_lo) {
					printf("str_occupy for test of %s cutted at %d,%d; expected %d\n",
						   tests[i].test_name, returned_lo, returned_hi, j);
					ret = FAIL;
				}
				/* Include one more character and see if length increases */
				if (tests[i].test[offset_lo]) {
					strcpy(buf, tests[i].test);
					*(char *) g_utf8_next_char(buf + offset_lo) = '\0';
					returned_hi = vstrlen(buf);
					if (returned_hi <= returned_lo || returned_hi > returned_lo + 2) {
						printf("str_occupy for test of %s cutted too early at %d\n",
							   tests[i].test_name, returned_lo);
						ret = FAIL;
					}
				}
			} else if (offset_hi > offset_lo) {
				/* Multibyte boundary */
				if (j != returned_hi - 1 || j != returned_lo + 1) {
					printf("str_occupy for test of %s cutted at %d,%d; expected %d,%d\n",
						   tests[i].test_name, returned_lo, returned_hi, j - 1, j + 1);
					ret = FAIL;
				}
			} else {
				printf("str_occupy returns lower value when greedy is true\n");
				ret = FAIL;
			}
		}
	}

	return ret;
}

static int test_addstr(void)
{
	int ret = PASS;
	int i;

	setlocale(LC_ALL, "");

	if (!g_get_charset(NULL)) {
		if (!setlocale(LC_ALL, "UTF-8") || !g_get_charset(NULL)) {
			puts("ncurses test skipped because locale is not UTF-8");
			return PASS;
		}
	}
	if (!initscr()) {
		puts("Could not initialize ncurses");
		return FAIL;
	}
	for (i = 0; i < G_N_ELEMENTS(tests) - 1; i++) {
		int x, y;

		if (mvaddstr(0, 0, tests[i].test) != OK) {
			endwin();
			printf("ncurses test for %s failed. addstr returned ERR\n", tests[i].test_name);
			fflush(stdout);
			initscr();
			ret = FAIL;
			continue;
		}
		getyx(stdscr, y, x);
		if (x != tests[i].expected_len) {
			endwin();
			printf("ncurses test for %s failed. addstr moved cursor %d steps; expected %d\n",
				   tests[i].test_name, x, tests[i].expected_len);
			fflush(stdout);
			initscr();
			ret = FAIL;
		}
	}

	endwin();

	return ret;
}
