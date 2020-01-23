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
 * $Id: ban.c,v 1.26 2003/07/12 16:01:44 weinholt Exp $
 */
#include "giftcurs.h"

#if HAVE_REGEX_H

#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "settings.h"
#include "ban.h"

static regex_t *filter_regex = NULL;

static const int FLAGS = REG_ICASE | REG_NOSUB | REG_EXTENDED;

void banwords_init(void)
{
	GString mega_regex = { 0 };
	int error;
	int lineno = 0;
	GSList *parts = get_config_multi("ignore", "filename");

	for (; parts; parts = parts->next) {
		regex_t tmp;
		const char *line = parts->data;

		lineno++;
		error = regcomp(&tmp, line, FLAGS);
		if (error) {
			char buf[256];

			regerror(error, &tmp, buf, sizeof buf);
			g_message("ignore filename %s: %s", line, buf);
		} else {
			g_string_append_printf(&mega_regex, mega_regex.len ? "|(%s)" : "(%s)", line);
		}
		regfree(&tmp);
	}
	if (mega_regex.len) {
		DEBUG("Filtering %d words; regex '%s'.", lineno, mega_regex.str);
		filter_regex = g_new(regex_t, 1);
		error = regcomp(filter_regex, mega_regex.str, FLAGS);
		g_assert(error == 0);
		g_free(mega_regex.str);
	}
}

void banwords_cleanup(void)
{
	if (filter_regex) {
		regfree(filter_regex);
		g_free(filter_regex);
		filter_regex = NULL;
	}
}

gboolean banwords_check(const char *haystack)
{
	if (filter_regex)
		return regexec(filter_regex, haystack, 0, NULL, 0);
	return TRUE;
}

#else
void banwords_init(void)
{
}
void banwords_cleanup(void)
{
}
G_GNUC_CONST gboolean banwords_check(const char *unused)
{
	return 1;
}
#endif
