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
 * $Id: parse.c,v 1.108 2002/11/28 20:03:28 chnix Exp $
 */
#include "giftcurs.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

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
		if (!isspace(*src)) {
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
	int i = length - (p - s);

	if (i > 0)
		memset(p, ' ', i);
	p[i] = '\0';
}

const char *prefixchars = "bkMGTPEZY";

char *humanify_scale_base(unsigned int quantity, int steps, int base)
{
	static char ringbuf[5][5];
	static int index = 0;
	char *buf = ringbuf[index++];

	if (index == buflen(ringbuf))
		index = 0;

#if 0
	if (steps == 1 && quantity < 10) {
		/* convert 3k => 3072 */
		steps--;
		quantity *= base;
	}
#endif
	if (quantity == 0)
		return "   0";
	if (steps == 0 && quantity < 1000) {
		sprintf(buf, "%4d", (int) quantity);
		return buf;
	}
	while (quantity >= base * 10) {
		steps++;
		quantity /= base;
	}
	if (quantity < 1000) {
		sprintf(buf, "%3d%c", (int) quantity, prefixchars[steps]);
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
		sprintf(buf, " %02d%c", (int) quantity, prefixchars[steps + 1]);
		buf[0] = buf[1];
		buf[1] = point;
	}
	return buf;
}

char *humanify_time(unsigned int secs)
{
	/* 34d12h, 59m59s ... */
	static char ringbuf[2][6];
	static int index = 0;
	char *buf = ringbuf[index++];
	int minutes, hours, days;

	if (index == buflen(ringbuf))
		index = 0;

	if (secs == 0) {
		strcpy(buf, "    0s");
		return buf;
	}
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
		snprintf(buf, sizeof ringbuf[0], "%dd", days);
	return buf;
}

#if !HAVE_STRNDUP
char *strndup(const char *s, size_t size)
{
	int n = strnlen(s, size);
	char *tmp = malloc(n + 1);

	if (tmp) {
		memcpy(tmp, s, n);
		tmp[n] = '\0';
	}
	return tmp;
}
#endif

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

int strncatf(char *buf, int max, const char *fmt, ...)
{
	va_list ap;
	int len = strlen(buf);

	va_start(ap, fmt);
	len = vsnprintf(buf + len, max - len, fmt, ap);
	va_end(ap);
	return len;
}

#ifndef HAVE_ASPRINTF
# ifndef VA_COPY
#  warning Using assignment instead of va_copy
#  define VA_COPY(dst, src) dst = src
# endif

int vasprintf(char **dst, const char *fmt, va_list ap)
{
	int size = 1024;
	char *p;

	for (;;) {
		int n;
		va_list aq;

		if (!(p = malloc(size)))
			return -1;

		VA_COPY(aq, ap);
		n = vsnprintf(p, size, fmt, aq);
		va_end(aq);

		if (n > -1 && n < size) {
			*dst = p;
			return n;
		}

		/* Try again with more space. */
		if (n > -1)				/* glibc 2.1 */
			size = n + 1;		/* precisely what is needed */
		else					/* glibc 2.0 */
			size *= 2;			/* twice the old size */
		free(p);
	}
}

int asprintf(char **dst, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf(dst, fmt, ap);
	va_end(ap);
	return ret;
}
#endif

void insert_at_ratio(char *buf, char *ins, float ratio, int max)
{
	int pos;
	int len = strlen(ins);

	ratio = ratio < 0.0 ? 0.0 : ratio > 1.0 ? 1.0 : ratio;
	pos = ratio * max;
	memmove(buf + pos + len, buf + pos, strlen(buf) - pos);
	memcpy(buf + pos, ins, len);
}

char *stristr(const char *haystack, const char *needle)
{
	char *lower, *ret;
	int len;

	/* If all-lowercase, ignore case. Otherwise match case. */
	ret = strstr(haystack, needle);
	if (ret)
		return ret;

	len = strlen(haystack) + 1;
	lower = malloc(len);
	while (len--)
		lower[len] = tolower(haystack[len]);
	ret = strstr(lower, needle);
	free(lower);
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

#ifndef HAVE_BASENAME
char *dirname(char *path)
{
	char *s;

	s = strrchr(path, '/');
	if (!s)
		return ".";
	*s = '\0';
	return path;
}

char *basename(char *path)
{
	char *p = strrchr(path, '/');

	return p ? p : path;
}
#endif

unsigned int my_atoi(const char *str)
{
	if (str)
		return strtoul(str, NULL, 10);
	return 0;
}

char *strlower(char *foo)
{
	char *bar = foo;

	if (!foo)
		return NULL;

	while (*bar) {
		*bar = tolower(*bar);
		bar++;
	}

	return foo;
}

char *my_strdup(const char *src)
{
	return src ? strdup(src) : NULL;
}

unsigned long int my_kilo_atof(const char *foo)
{
	unsigned long int res;
	char decimals[] = "000";
	char *endptr, *tmp = decimals;

	if (!foo)
		return 0;

	res = strtoul(foo, &endptr, 10) * 1000;
	while (*++endptr && *tmp)
		*tmp++ = *endptr;

	res += strtoul(decimals, NULL, 10);
	return res;
}
