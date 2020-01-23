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
 * $Id: parse.h,v 1.95 2003/11/06 23:36:36 weinholt Exp $
 */
#ifndef _PARSE_H
#define _PARSE_H

/* removes extra whitespace */
char *trim(char *foo);

/* translates quantity to a "human-friendly" string, same format as "df -H"
 * for example: 1231424 1000   10000  10239   10240  1023999 1024000
 * becomes    : "1.1M"  "1000" "9.7k" "9.9k"  " 10k" "999k"  "0.9M"
 * the return value points to a ring buffer and is always 4 chars wide
 * Use at most 5 of theese in a printf-call, or increase the ring buffer size.
 */

extern const char *const suffixchars /* = "bkMGTPEZY" */ ;

/* limit is the lowest number that should be suffixed,
 * somewhere between 1000-10000 is a good value */
const char *humanify_base(guint64 quantity, int base, int limit);

/* some often used shortcuts */
#define humanify(q) humanify_base(q, 1024, 1000)
#define humanify_1000(q) humanify_base(q, 1000, 10000)

/* translate time to a "human-friendly" string, for example
 * 34s, 5m45s, 23h4m, 1d0h, etc.. */
const char *humanify_time(unsigned int secs);

/* If all-lowercase, ignore case. Otherwise match case. */
char *stristr(const char *haystack, const char *needle);

/* returns the index of string needle in array haystack, or -1 */
int lookup(const char *needle, const char *haystack[], int items);

/* Remove the word at the given position */
void remove_word(GString *foo, int *pos);

/* the same as atoi but can handle NULL */
unsigned int my_atoi(const char *str);

/* Converts an integer to a static string. (not thread-safe!) */
const char *itoa(int n);

/* This is like atof but gives the result times 2^30, and can handle
   any character as decimal point. */
guint64 my_giga_atof(const char *foo);

/* Parse a typed query into the real query and the excludes */
void parse_typed_query(const char *query_, char **includes, char **excludes, char **protocols);

#include "list.h"

/* wrap the lines in 'text' and append the result onto a list
 * width == -1 means do not wrap at all */
void wrap_lines(list *result, const char *text, int width);

/* Size means sizeof(bitmap) */
void bitmap_set(guchar *bitmap, guint size, guint idx, gboolean value);
gboolean bitmap_get(guchar *bitmap, guint size, guint idx);
gint bitmap_find_unset(guchar *bitmap, guint size);

/* Tries to convert from 'from' to 'to', freeing 'str' and returning the
   newly allocated string if it worked. */
char *convert_to_locale(char *str);

/* Visual strlen(). Returns how many chars will be viewed on the screen. */
glong vstrlen(const char *str);

/* Returns the number of bytes from str required to fill n slots on screen */
/* greedy tells whether or not to include wide characters on the boundary */
int str_occupy(const char *str, int n, int greedy);

/* Find the previous/next character in the string that has a non-zero width. */
char *utf8_find_next_char(gchar *p, gchar *end);
char *utf8_find_prev_char(gchar *str, gchar *p);

#endif
