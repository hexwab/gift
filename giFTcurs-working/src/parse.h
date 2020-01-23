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
 * $Id: parse.h,v 1.56 2002/11/28 19:52:23 chnix Exp $
 */
#ifndef _PARSE_H
#define _PARSE_H

/* removes extra whitespace */
char *trim(char *foo);

/* right-pads a string with spaces, until strlen(foo) == length */
void strpad(char *s, int length);

/* translates quantity to a "human-friendly" string, same format as "df -H"
 * for example: 1231424 1000   10000  10239   10240  1023999 1024000
 * becomes    : "1.1M"  "1000" "9.7k" "9.9k"  " 10k" "999k"  "0.9M"
 * the return value points to a ring buffer and is always 4 chars wide
 * Use at most 5 of theese in a printf-call, or increase the ring buffer size.
 */
extern const char *prefixchars /* = "bkMGTPEZY" */ ;

/* This uses a predefined scale. 1 => kilo 2 => Mega etc. */
char *humanify_scale_base(unsigned int quantity, int scale, int base);

/* some often used shortcuts */
#define humanify(q) humanify_scale_base(q, 0, 1024)
#define humanify_scale(q, scale) humanify_scale_base(q, scale, 1024)
#define humanify_1000(q) humanify_scale_base(q, 0, 1000)

/* translate time to a "human-friendly" string, for example
 * 34s, 5m45s, 23h4m, 1d0h, etc.. */
char *humanify_time(unsigned int secs);

#if !HAVE_STRNDUP
# ifndef strndup
char *strndup(const char *s, size_t size);
# endif
#endif

#if !HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen);
#endif

/* If all-lowercase, ignore case. Otherwise match case. */
char *stristr(const char *haystack, const char *needle);

/* returns the index of string needle in array haystack, or -1 */
int lookup(const char *needle, const char *haystack[], int items);

/* concatenate buf with a formatted string, buf is `max' bytes in size */
int strncatf(char *buf, int max, const char *fmt, ...)
	__attribute__ ((format(printf, 3, 4)));

#ifndef HAVE_ASPRINTF
/* vsprintf to a malloc'ated string */
int vasprintf(char **dst, const char *fmt, va_list ap);
int asprintf(char **dst, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));
#endif

#ifndef HAVE_SNPRINTF
int snprintf(char *, int, const char *, ...);
int vsnprintf(char *, int, const char *, va_list);
#endif

void insert_at_ratio(char *buf, char *ins, float ratio, int max);

/* Remove the word at the given position */
char *remove_word(char *foo, int *pos);

#ifdef HAVE_BASENAME
# include <libgen.h>
#else
char *basename(char *path);
char *dirname(char *path);
#endif

/* the same as atoi but can handle NULL */
unsigned int my_atoi(const char *str);
char *my_strdup(const char *src);

/* Modifies the original string and returns it */
char *strlower(char *str);

/* This is like atof but gives the result times 1000, and can handle
   any character as decimal point. */
unsigned long int my_kilo_atof(const char *foo);

#endif
