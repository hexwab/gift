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

/* uhh.  0.  of course I could just do = { 0 }, but where's the fun in that? */
static unsigned char string_set[256] =
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

#define STRINGF(buf,fmt) \
	char        buf[4096];                        \
	va_list     args;                             \
	                                              \
	va_start (args, fmt);                         \
	vsnprintf (buf, sizeof (buf) - 1, fmt, args); \
	va_end (args)

char *stringf (char *fmt, ...)
{
	static STRINGF (buf, fmt);

	return buf;
}

char *stringf_dup (char *fmt, ...)
{
	STRINGF (buf, fmt);

	return STRDUP (buf);
}

/*****************************************************************************/

int string_append (char *str, size_t str_size, int *str_len, char *fmt, ...)
{
	va_list args;
	int     written;

	if (!str || !str_size || !str_len)
		return 0;

	va_start (args, fmt);
	written = vsnprintf (str + (*str_len), str_size - (*str_len) - 1, fmt, args);
	va_end (args);

	*str_len += written;

	return written;
}

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

char *STRDUP_N (char *string, size_t string_len)
{
	char *buffer;

	if (!string)
		return NULL;

	if (!(buffer = malloc (string_len + 1)))
		return NULL;

	memcpy (buffer, string, string_len);
	buffer[string_len] = 0;

	return buffer;
}

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
	if (!dst || !src)
		return;

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

	if (!string || !string[0])
		return;

	ptr = string;
	while (isspace (*ptr))
		ptr++;

	if (ptr != string)
		strmove (string, ptr);

	if (!string[0])
		return;

	ptr = string + strlen (string) - 1;

	if (isspace (*ptr))
	{
		while (ptr >= string && isspace (*ptr))
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
	unsigned char *str_ptr;
	unsigned char *ptr;

	for (ptr = charset; *ptr; ptr++)
		string_set[(int)*ptr] = 1;

	for (str_ptr = string; *str_ptr; str_ptr++)
	{
		if (string_set[(int)*str_ptr])
			break;
	}

	for (ptr = charset; *ptr; ptr++)
		string_set[(int)*ptr] = 0;

	if (!str_ptr[0])
		str_ptr = NULL;

	return str_ptr;
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
	char *test = strdup ("/music/paul_oakenfold!!!!/paul_oakenfold_-_perfecto_presents_'another_world'/cd1/08_-_paul_oakenfold_-_planet_perfecto_-_bullet_in_a_gun_[rabbit_in_the_moon_remix].mp3");
	char *token;

	while ((token = string_sep_set (&test, "\\/_-.[]()")))
	{
		char *ptr, *token_ptr;

		if (!token[0])
			continue;

		token_ptr = token;

		while ((ptr = string_sep_set (&token_ptr, ",`'!?*")))
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
