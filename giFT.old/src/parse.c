/*
 * parse.c
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

#include <ctype.h>

#include "gift.h"

#include "file.h"
#include "conf.h"
#include "parse.h"

/*****************************************************************************/

/* uhh.  0. */
static char string_set[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*****************************************************************************/

char *string_lower (char *string)
{
	char *string0 = string;

	if (!string)
		return NULL;

	while (*string)
	{
		*string = tolower (*string);
		string++;
	}

	return string0;
}

/*****************************************************************************/

#ifndef TCG_LEAK_DETECT
char *STRDUP (char *string)
{
	if (!string)
		return NULL;

	return strdup (string);
}
#endif /* !TCG_LEAK_DETECT */

int STRCMP (char *s1, char *s2)
{
	if (!s1)
		return 1;
	else if (!s2)
		return -1;

	return strcmp (s1, s2);
}

long ATOI (char *string)
{
	if (!string)
		return 0;

	return atoi (string);
}

unsigned long ATOUL (char *string)
{
	if (!string)
		return 0;

	return strtoul (string, (char **) NULL, 10);
}

char *ITOA (long integer)
{
	static char x[32];

	sprintf (x, "%li", integer);

	return x;
}

/*****************************************************************************/

#if 0
/* modified from /usr/src/linux/lib/string.c:strstr() */
char *strcasestr (char *haystack, const char *needle)
{
	int haystack_len, needle_len;

	needle_len = strlen (needle);
	if (!needle_len)
		return haystack;

	haystack_len = strlen (haystack);
	while (haystack_len >= needle_len) {
		haystack_len--;

		if (!strncasecmp (haystack, needle, needle_len))
			return haystack;

		needle_len++;
	}

	return NULL;
}
#endif

void strmove (char *dst, const char *src)
{
	while (*src)
		*dst++ = *src++;

	*dst = 0;
}

/*****************************************************************************/

int str_isempty (char *string)
{
	if (!string)
		return 1;

	switch (*string)
	{
	 case '\r':
	 case '\n':
	 case '\0':
		return 1;
		break;
	}

	return 0;
}

void trim_whitespace (char *string)
{
	char *ptr;

	ptr = string;
	while (isspace (*ptr))
		ptr++;

	if (ptr != string)
		strmove (string, ptr);

	ptr = string + strlen (string) - 1;

	if (isspace (*ptr))
	{
		while (isspace (*ptr))
			ptr--;

		ptr[1] = 0;
	}
}

/*****************************************************************************/

typedef char* (*StringFunc) (char *string, char *needle);

static char *string_sep_ex (char **string, char *needle, size_t needle_len,
							StringFunc search)
{
	char *iter, *str;

	if (!string || !*string || !**string)
		return NULL;

	str = *string;

	if ((iter = (*search) (str, needle)))
	{
		*iter = 0;
		iter += needle_len;
	}

	*string = iter;

	return str;
}

/*****************************************************************************/

static char *string_sep_set_func (char *string, char *charset)
{
	char *ptr;

	for (ptr = charset; *ptr; ptr++)
		string_set[(int)*ptr] = 1;

	while (*string)
	{
		if (string_set[(int)*string])
			break;

		string++;
	}

	for (ptr = charset; *ptr; ptr++)
		string_set[(int)*ptr] = 0;

	if (!string[0])
		string = NULL;

	return string;
}

char *string_sep_set (char **string, char *charset)
{
	return string_sep_ex (string, charset, sizeof (char), string_sep_set_func);
}

/*****************************************************************************/

char *string_sep (char **string, char *delim)
{
	return string_sep_ex (string, delim, strlen (delim), (StringFunc) strstr);
}

/*****************************************************************************/

#if 0
int main ()
{
	char *test = strdup ("/music/paul_oakenfold/paul_oakenfold_-_perfecto_presents_another_world/cd1/08_-_paul_oakenfold_-_planet_perfecto_-_bullet_in_a_gun_[rabbit_in_the_moon_remix].mp3");
	char *token;

	while ((token = string_sep_set (&test, "\\/_-.[]()")))
	{
		char *ptr, *token_ptr;

		if (!token[0] || !test)
			continue;

		token_ptr = token;

		while ((ptr = string_sep_set (&token_ptr, "aeiouy")))
		{
			if (!token_ptr)
				continue;

			strmove (token_ptr - 1, token_ptr);
			token_ptr--;
		}

		printf ("token = %s\n", token);
	}

	return 0;
}
#endif

#if 0
int main ()
{
	char *x = strdup ("GET /testing_this_stuff\r\n"
					  "Key-Number-1: This is a sample value\r\n"
					  "Key2: bla=bla\r\n"
					  "Something: Something else\n"
					  "User-Agent: Mozilla\r\n");
	char *token;

	while ((token = string_sep_set (&x, "\r\n")))
	{
		char *key;

		trim_whitespace (token);

		key = string_sep (&token, ": ");

		printf ("key = %s, value = %s\n", key, token);
	}

	return 0;
}
#endif
