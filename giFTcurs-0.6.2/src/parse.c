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
 * $Id: parse.c,v 1.157 2003/11/16 16:15:36 weinholt Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#if ENABLE_NLS
# define __USE_XOPEN
# include <locale.h>
#endif

#include "parse.h"
#include "wcwidth.h"

#ifdef WIDE_NCURSES
gboolean utf8 = FALSE;
#endif

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

const char *const suffixchars = "bkMGTPEZY";

const char *humanify_base(guint64 quantity_l, int base, int limit)
{
	static char ringbuf[5][5];
	static int idx = 0;
	char *buf = ringbuf[idx++];
	int steps = 0;
	unsigned int quantity;

	if (idx == G_N_ELEMENTS(ringbuf))
		idx = 0;

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
	static int idx = 0;
	char *buf;
	int minutes, hours, days;

	if (secs == 0)
		return "    0s";

	buf = ringbuf[idx++];
	if (idx == G_N_ELEMENTS(ringbuf))
		idx = 0;

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

/* FIXME: make this one recognize G_UNICODE_BREAK_SPACE or something? */
void remove_word(GString *foo, int *posp)
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

const char *itoa(int n)
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
void parse_typed_query(const char *query, char **includes, char **excludes, char **protocols)
{
	char **tokens, **token;
	/* *INDENT-OFF* */
	dynarray incl = { 0 }, excl = { 0 }, prot = { 0 };
	/* *INDENT-ON* */

	tokens = g_strsplit(query, " ", -1);

	for (token = tokens; *token; token++) {
		if ((*token)[0] == '-')
			dynarray_append(&excl, *token + 1);
		else if (!g_ascii_strncasecmp("protocol:", *token, 9)) {
			char **proto, **protos = g_strsplit((*token) + 9, ",", -1);

			for (proto = protos; *proto; proto++)
				dynarray_append(&prot, *proto);
			/* Just free the array, g_strfreev() destroys the strings */
			g_free(protos);
		} else
			dynarray_append(&incl, *token);
	}
	dynarray_append(&excl, NULL);
	dynarray_append(&incl, NULL);
	dynarray_append(&prot, NULL);

	*includes = g_strjoinv(" ", (char **) incl.entries);
	*excludes = g_strjoinv(" ", (char **) excl.entries);
	*protocols = g_strjoinv(" ", (char **) prot.entries);

	dynarray_removeall(&excl);
	dynarray_removeall(&incl);
	dynarray_foreach(&prot, g_free);
	dynarray_removeall(&prot);
	g_strfreev(tokens);
}

void wrap_lines(list *result, const char *text, int width)
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

void bitmap_set(guchar *bitmap, guint size, guint idx, gboolean value)
{
	if (value)
		bitmap[idx >> 3] |= 1 << (idx & 7);
	else
		bitmap[idx >> 3] &= ~(1 << (idx & 7));
}

gboolean bitmap_get(guchar *bitmap, guint size, guint idx)
{
	return bitmap[idx >> 3] & (1 << (idx & 7));
}

gint bitmap_find_unset(guchar *bitmap, guint size)
{
	int i;

	for (i = 0; i < size; i++)
		if (bitmap[i] != 0xff)
			return (i << 3) + g_bit_nth_lsf(~bitmap[i], -1);

	return -1;
}

char *convert_to_locale(char *str)
{
	const char *from, *to;
	char *tmp;

	if (!str)
		return NULL;

	if (G_UNLIKELY(utf8)) {
		/* TODO: autodetect the locale somehow? */
		if (!g_utf8_validate(str, -1, NULL))
			from = "iso-8859-1", to = "utf-8";
		else
			return str;
	} else {
		if (g_utf8_validate(str, -1, NULL))
			from = "utf-8", g_get_charset(&to);
		else
			return str;
	}

	tmp = g_convert_with_fallback(str, -1, to, from, NULL, NULL, NULL, NULL);

	if (tmp) {
		/* Conversion succeeded. */
		g_free(str);
		return tmp;
	}
	return str;
}

#ifdef WIDE_NCURSES
/* Visual strlen(). */
glong vstrlen(const char *str)
{
	if (utf8) {
		const char *p;
		int len;

		/* g_unichar_iswide does not detect combining characters */
		/* FIXME: We couldn't get mbrtowc/wcwidth to work. If anybody
		 *        know how to do with them, contact us.
		 */
		for (p = str, len = 0; *p; p = g_utf8_next_char(p))
			len += mk_wcwidth(g_utf8_get_char(p));
		return len;
	} else {
		return strlen(str);
	}
}

int str_occupy(const char *str, int visual, int greedy)
{
	const char *p;

	if (!utf8)
		return visual;

	/* Note. continue scanning even if visual == 0, so that
	 * combining characters are includes as well. */

	for (p = str; *p; p = g_utf8_next_char(p)) {
		int w = mk_wcwidth(g_utf8_get_char(p));

		if (visual == 1 && w == 2 && greedy)
			visual--;
		else if ((visual -= w) < 0)
			break;
	}
	return p - str;
}

char *utf8_find_next_char(gchar *p, gchar *end)
{
	for (;;) {
		p = g_utf8_find_next_char(p, end);
		if (!p || !*p || mk_wcwidth(g_utf8_get_char(p)) > 0)
			return p;
	}
}

char *utf8_find_prev_char(gchar *str, gchar *p)
{
	for (;;) {
		p = g_utf8_find_prev_char(str, p);
		if (!p || mk_wcwidth(g_utf8_get_char(p)) > 0)
			return p;
	}
}
#else
glong vstrlen(const char *str)
{
	return strlen(str);
}

int str_occupy(const char *str, int visual, int greedy)
{
	return visual;
}

char *utf8_find_next_char(gchar *p, gchar *end)
{
	return g_utf8_find_next_char(p, end);
}

char *utf8_find_prev_char(gchar *str, gchar *p)
{
	return g_utf8_find_prev_char(str, p);
}
#endif
