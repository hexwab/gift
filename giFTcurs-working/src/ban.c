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
 * $Id: ban.c,v 1.12 2002/10/22 13:18:09 weinholt Exp $
 */
#include "giftcurs.h"

#if HAVE_REGEX_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <regex.h>

#include "parse.h"
#include "screen.h"
#include "settings.h"
#include "ban.h"

#define BANWORD_FILE "ui/banwords"

static regex_t *filter_regex = NULL;

static const int FLAGS = REG_ICASE | REG_NOSUB | REG_EXTENDED;

void banwords_init(void)
{
	FILE *f = open_config(BANWORD_FILE, "r");
	char line[256];
	char *mega_regex = NULL;
	int error;
	int lineno = 0;

	if (!f)
		return;

	while (fgets(line, sizeof line, f)) {
		regex_t tmp;

		lineno++;
		trim(line);
		if (line[0] == '#')
			continue;
		error = regcomp(&tmp, line, FLAGS);
		if (error) {
			char buf[256];

			regerror(error, &tmp, buf, sizeof buf);
			message("%s:%d: %s", BANWORD_FILE, lineno, buf);
		}
		regfree(&tmp);
		if (!error) {
			if (mega_regex) {
				char *new;

				asprintf(&new, "%s|(%s)", mega_regex, line);
				free(mega_regex);
				mega_regex = new;
			} else {
				asprintf(&mega_regex, "(%s)", line);
			}
		}
	}
	if (mega_regex) {
		DEBUG("Filtering %d words from %s '%s'.", lineno, BANWORD_FILE, mega_regex);
		filter_regex = calloc(1, sizeof(regex_t));
		error = regcomp(filter_regex, mega_regex, FLAGS);
		assert(error == 0);
		free(mega_regex);
	}
}

void banwords_cleanup(void)
{
	if (filter_regex) {
		regfree(filter_regex);
		free(filter_regex);
		filter_regex = NULL;
	}
}

int banwords_check(const char *haystack)
{
	if (filter_regex)
		return regexec(filter_regex, haystack, 0, NULL, 0);
	return 1;
}

#else
void banwords_init(void)
{
}
void banwords_cleanup(void)
{
}
int banwords_check(const char *unused)
{
	return 1;
}
#endif
