/*
 * parse.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __PARSE_H
#define __PARSE_H

/*****************************************************************************/

char         *string_lower (char *string);

#ifndef TCG_LEAK_DETECT
char         *STRDUP       (char *string);
#else
#define       STRDUP       strdup
#endif

/* this is a poorly named function :/ */
char         *STRDUP_N     (char *string, size_t string_len);

int           STRCMP       (char *s1, char *s2);
long          ATOI         (char *string);
unsigned long ATOUL        (char *string);
char         *ITOA         (long integer);

#if 0
char *strcasestr      (char *haystack, const char *needle);
#endif

void  strmove         (char *dst, const char *src);
int   str_isempty     (char *string);
void  trim_whitespace (char *string);
char *string_sep_set  (char **string, char *charset);
char *string_sep      (char **string, char *delim);

/*****************************************************************************/

#endif /* __PARSE_H */
