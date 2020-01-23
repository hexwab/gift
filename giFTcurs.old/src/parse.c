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
 * $Id: parse.c,v 1.129 2003/05/14 10:51:57 chnix Exp $
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
#include "screen.h"

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

void strpad(char *s, int length)
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

#if !HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
	const char *end = memchr(s, '\0', maxlen);

	return end ? end - s : maxlen;
}
#endif

int lookup(const char *needle, const char *haystack[], int items)
{
	int i;

	for (i = 0; i < items; i++)
		if (haystack[i] && !strcmp(needle, haystack[i]))
			return i;
	return -1;
}

void insert_at_ratio(char *buf, char *ins, float ratio, int max)
{
	int pos;
	int len = strlen(ins);

	ratio = CLAMP(ratio, 0.0, 1.0);
	pos = ratio * max;
	memmove(buf + pos + len, buf + pos, strlen(buf) - pos);
	memcpy(buf + pos, ins, len);
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

char *remove_word(char *foo, int *posp)
{
	int i = *posp;

	while (foo[i - 1] == ' ' && i > 0)
		i--;
	while (foo[i - 1] != ' ' && i > 0)
		i--;
	strcpy(foo + i, foo + *posp);
	*posp = i;

	return foo;
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
	char *dot, *ptr;
	unsigned int intpart;
	char decimals[] = "000";

	intpart = strtoul(foo, &dot, 10);
	if (*dot)
		for (ptr = decimals; *ptr; ptr++) {
			if (!*++dot)
				break;
			*ptr = *dot;
		}
	return ((guint64) intpart << 30) + 1024 * 1024 * atoi(decimals) / 1000 * 1024;
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
 * This does not need to run fast, so pretty expensive glib functions are used.
 */
void parse_typed_query(const char *query, char **includes, char **excludes)
{
	char **tokens, **token;
	GPtrArray *incl = g_ptr_array_new(), *excl = g_ptr_array_new();

	tokens = g_strsplit(query, " ", -1);

	for (token = tokens; *token; token++) {
		if ((*token)[0] == '-')
			g_ptr_array_add(excl, *token + 1);
		else
			g_ptr_array_add(incl, *token);
	}
	g_ptr_array_add(excl, NULL);
	g_ptr_array_add(incl, NULL);

	*includes = g_strjoinv(" ", (char **) incl->pdata);
	*excludes = g_strjoinv(" ", (char **) excl->pdata);

	g_ptr_array_free(excl, FALSE);
	g_ptr_array_free(incl, FALSE);
	g_strfreev(tokens);
}
