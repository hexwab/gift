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
 * $Id: parse.c,v 1.140 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#if ENABLE_NLS
# include <locale.h>
#endif

#include "parse.h"

char *trim(char *src)
{
	char *dst = src, *last = src + 666666;
	char *fnord = src;

	while (*src) {
		if (!g_ascii_isspace(*src)) {
			if (src > last)
				*dst++ = ' ';
			*dst++ = *src++;
			last = src;
		} else
			src++;
	}
	*dst = '\0';

	return fnord;
}

void strpad(char *s, size_t length)
{
	char *p = strchr(s, '\0');

	length -= (p - s);
	if (length > 0)
		memset(p, ' ', length);
	p[length] = '\0';
}

const char *const suffixchars = "bkMGTPEZY";

const char *humanify_base(guint64 quantity_l, int base, int limit)
{
	static char ringbuf[5][5];
	static int index = 0;
	char *buf = ringbuf[index++];
	int steps = 0;
	unsigned int quantity;

	if (index == G_N_ELEMENTS(ringbuf))
		index = 0;

	/* quick check for common case of zero */
	if (quantity_l == 0)
		return "   0";

	/* scale it down so it fits into an int */
	while (quantity_l >= UINT_MAX) {
		steps++;
		quantity_l /= base;
	}
	quantity = (unsigned int) quantity_l;

	if (steps == 0 && quantity < limit) {
		sprintf(buf, "%4d", (int) quantity);
		return buf;
	}
	while (quantity >= base * 10) {
		steps++;
		quantity /= base;
	}
	if (quantity < 1000) {
		sprintf(buf, "%3d%c", (int) quantity, suffixchars[steps]);
	} else {
		/* Get the locale decimal point. */
#ifdef ENABLE_NLS
		static char point = 0;

		if (point == 0)
			point = *localeconv()->decimal_point;
#else
		const char point = '.';
#endif
		quantity = quantity * 10 / base;
		/* quantity now in range 09-99, make a decimal point */
		sprintf(buf + 1, "%02d%c", (int) quantity, suffixchars[steps + 1]);
		buf[0] = buf[1];
		buf[1] = point;
	}
	return buf;
}

const char *humanify_time(unsigned int secs)
{
	/* 34d12h, 59m59s ... */
	static char ringbuf[2][6];
	static int index = 0;
	char *buf;
	int minutes, hours, days;

	if (secs == 0)
		return "    0s";

	buf = ringbuf[index++];
	if (index == G_N_ELEMENTS(ringbuf))
		index = 0;

	if (secs < 60) {
		sprintf(buf, "   %2ds", (int) secs);
		return buf;
	}
	minutes = secs / 60;
	if (minutes < 60) {
		sprintf(buf, "%2dm%02ds", minutes, (int) secs % 60);
		return buf;
	}
	hours = minutes / 60;
	if (hours < 24) {
		sprintf(buf, "%2dh%02dm", hours, minutes % 60);
		return buf;
	}
	days = hours / 24;
	if (days < 100)
		sprintf(buf, "%2dd%02dh", days, hours % 24);
	else
		g_snprintf(buf, sizeof ringbuf[0], "%5dd", days);
	return buf;
}

int lookup(const char *needle, const char *haystack[], int items)
{
	int i;

	for (i = 0; i < items; i++)
		if (haystack[i] && !strcmp(needle, haystack[i]))
			return i;
	return -1;
}

char *stristr(const char *haystack, const char *needle)
{
	char *lower, *ret;

	/* If needle is all-lowercase, ignore case. Otherwise match case. */
	ret = strstr(haystack, needle);
	if (ret)
		return ret;

	lower = g_ascii_strdown(haystack, -1);
	ret = strstr(lower, needle);
	g_free(lower);
	if (ret)
		return haystack - lower + ret;
	return NULL;
}

void remove_word(GString * foo, int *posp)
{
	int i = *posp;

	while (foo->str[i - 1] == ' ' && i > 0)
		i--;
	while (foo->str[i - 1] != ' ' && i > 0)
		i--;
	g_string_erase(foo, i, *posp - i);
	*posp = i;
}

unsigned int my_atoi(const char *str)
{
	if (str)
		return strtoul(str, NULL, 10);
	return 0;
}

guint64 my_giga_atof(const char *foo)
{
#if 0
	/* This will not work as we don't know what the decimal character is */
	return (guint64) (1024 * 1024 * 1024 * atof(foo));
#else
	unsigned int intpart = 0, fracpart = 0, scale = 1;

	while (isdigit(*foo))
		intpart = 10 * intpart + *foo++ - '0';
	if (*foo++)
		while (isdigit(*foo) && scale < 1001)
			fracpart = 10 * fracpart + *foo++ - '0', scale *= 10;
	return ((guint64) intpart << 30) + ((fracpart << 18) / scale << 12);
#endif
}

char *itoa(int n)
{
	static char buf[20];

	g_snprintf(buf, sizeof buf, "%u", n);
	return buf;
}

/* Split query into includes and excludes. Example: "-or kill me"
 * becomes 'kill me' and 'or'
 * query is assumed to be trim()'ed already.
 * This does not need to run fast, so pretty expensive functions are used.
 */
void parse_typed_query(const char *query, char **includes, char **excludes)
{
	char **tokens, **token;
	dynarray incl = { 0 }, excl = {
	0};

	tokens = g_strsplit(query, " ", -1);

	for (token = tokens; *token; token++) {
		if ((*token)[0] == '-')
			dynarray_append(&excl, *token + 1);
		else
			dynarray_append(&incl, *token);
	}
	dynarray_append(&excl, NULL);
	dynarray_append(&incl, NULL);

	*includes = g_strjoinv(" ", (char **) incl.entries);
	*excludes = g_strjoinv(" ", (char **) excl.entries);

	dynarray_removeall(&excl);
	dynarray_removeall(&incl);
	g_strfreev(tokens);
}

void wrap_lines(list * result, const char *text, int width)
{
	for (;;) {
		const char *pos = strchr(text, '\n');
		int len;

		if (pos && (width < 0 || pos <= text + width)) {
			list_append(result, g_strndup(text, pos - text));
			text = pos + 1;
			continue;
		}
		len = strlen(text);
		if (width < 0 || len <= width) {
			list_append(result, g_strdup(text));
			break;
		}
		for (pos = text + width; pos > text; pos--) {
			if (*pos == ' ') {
				list_append(result, g_strndup(text, pos - text));
				text = pos + 1;
				break;			/* continue the outer loop */
			}
		}
		if (text == pos + 1)
			continue;
		list_append(result, g_strndup(text, width));
		text += width;
	}
}

void bitmap_set(guchar *bitmap, guint size, guint index, gboolean value)
{
	if (value)
		bitmap[index >> 3] |= 1 << (index & 7);
	else
		bitmap[index >> 3] &= ~(1 << (index & 7));
}

gint bitmap_find_unset(guchar *bitmap, guint size)
{
	int i;
	
	for (i = 0; i < size; i++)
		if (bitmap[i] != 0xff)
			return (i << 3) + g_bit_nth_lsf(~bitmap[i], -1);

	return -1;
}
