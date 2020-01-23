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

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

typedef struct
{
	char *str;
	int   alloc;
	int   len;
	int   resizable;
} String;

/*****************************************************************************/

char         *stringf          (char *fmt, ...);
char         *stringf_dup      (char *fmt, ...);

String       *string_new       (char *str, int alloc, int len, int resizable);
void          string_free      (String *s);
char         *string_free_keep (String *s);
int           string_resize    (String *s, int new_alloc);

int           string_appendf   (String *s, char *fmt, ...);
int           string_append    (String *s, char *str);
int           string_appendc   (String *s, char c);

char         *string_upper     (char *s);
char         *string_lower     (char *s);

#ifndef TCG_LEAK_DETECT
char         *STRDUP           (char *s);
#else
#define       STRDUP           strdup
#endif

#define       STRLEN(str)      (str ? strlen (str) : 0)
#define       STRLEN_0(str)    (str ? (strlen (str) + 1) : 0)

#define       STRING_NULL(str)    ((str && *str) ? str : NULL)
#define       STRING_NOTNULL(str) ((str) ? str : "")

/* this is a poorly named function :/ */
char         *STRDUP_N         (char *string, size_t string_len);

int           STRCMP           (char *s1, char *s2);
long          ATOI             (char *string);
unsigned long ATOUL            (char *string);
char         *ITOA             (long integer);

void         *memory_dup       (void *src, size_t len);
void          string_move      (char *dst, const char *src);
int           string_isempty   (char *string);
char         *string_trim      (char *string);
char         *string_sep_set   (char **string, char *charset);
char         *string_sep       (char **string, char *delim);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __PARSE_H */
