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

#define STRING_SIZE 128

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

String *string_new (char *str, int alloc, int len, int resizable)
{
	String *s;

	if (!(s = malloc (sizeof (String))))
		return NULL;

	s->str       = str;
	s->alloc     = alloc;
	s->len       = len;
	s->resizable = resizable;

	return s;
}

void string_free (String *s)
{
	if (s->resizable)
		free (s->str);

	free (s);
}

char *string_free_keep (String *s)
{
	char *str;

	str = s->str;

	s->resizable = FALSE;
	string_free (s);

	return str;
}

int string_resize (String *s, int new_alloc)
{
	char *new_str;

	/* hmm. */
	if (new_alloc > 131070)
		return 0;

	if (new_alloc > s->alloc)
	{
		if (!s->resizable)
			return 0;

		if (!(new_str = realloc (s->str, new_alloc)))
			return 0;

		s->str = new_str;
	}

	s->alloc = new_alloc;

	return s->alloc;
}

/*****************************************************************************/

int string_appendf (String *s, char *fmt, ...)
{
	va_list args;
	int     written = 0;

	if (!s)
		return 0;

	if (s->alloc == 0)
	{
		if (!string_resize (s, s->alloc + STRING_SIZE))
			return 0;
	}

	for (;;)
	{
		int max;

		if (s->len < s->alloc)
		{
			max = s->alloc - s->len - 1;

			va_start (args, fmt);
			written = vsnprintf (s->str + s->len, max, fmt, args);
			va_end (args);

			if (written > -1 && written < max)
				break;
		}

#if 0
		if (written > -1)
			max = s->alloc + written + 1;
		else
			max = s->alloc * 2;
#endif

		if (!string_resize (s, s->alloc * 2))
			break;
	}

	/* get the new written length */
	s->len = CLAMP (s->len + written, 0, s->alloc);

	return written;
}

int string_append (String *s, char *str)
{
	/* TODO -- opt */
	return string_appendf (s, "%s", str);
}

int string_appendc (String *s, char c)
{
	return string_appendf (s, "%c", c);
}

/*****************************************************************************/

static char *strctype (char *s, int (*cfunc) (int))
{
	char *ptr;

	if (!s || !cfunc)
		return NULL;

	for (ptr = s; *ptr; ptr++)
		*ptr = cfunc (*ptr);

	return s;
}

char *string_upper (char *s)
{
	return strctype (s, toupper);
}

char *string_lower (char *s)
{
	return strctype (s, tolower);
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

	if (!string || !string_len)
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

	return atol (string);
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

void *memory_dup (void *src, size_t len)
{
	void *dst;

	if (len == 0)
		return NULL;

	if (!(dst = malloc (len)))
		return NULL;

	memcpy (dst, src, len);
	return dst;
}

void string_move (char *dst, const char *src)
{
	if (!dst || !src)
		return;

	while (*src)
		*dst++ = *src++;

	*dst = 0;
}

/*****************************************************************************/

int string_isempty (char *string)
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

#if 0
/* trims leading and trailing whitespace off of a string (using the same
 * storage buffer supplied
 * NOTE: this function is optimized...forgive the unreadability */
char *string_trim2 (char *str)
{
	char *pread;
	char *pwrite;
	char *plast = NULL;

	/* shift past the leading whitespace
	 * NOTE: pread/pwrite are used so that we can tell if there was whitespace
	 * there or not in a generalized manner */
	for (pread = pwrite = str; isspace (*pread); pread++);

	if (*pread)
		for (plast = pread + strlen (*pread) - 1; isspace (*plast); plast--);
	else
	{
		/* nothing but whitespace was supplied */
		*pwrite = 0;
		return str;
	}

	/* skip past all of the non-whitespace characters
	 * NOTE: the compare is optimized outside of the loop.  if pread != pwrite,
	 * then each read must be copied into the write buffer as there is
	 * leading space at pwrite */
	if (pread == pwrite)
		for (; pread < plast; pread++, pwrite++);
	else
	{
		for (; pread < plast; pread++, pwrite++)
			*pwrite = *pread;
	}

	*pwrite = 0;

	return str;
}
#endif

char *string_trim (char *string)
{
	char *ptr;

	if (!string || !string[0])
		return string;

	/* skip leading whitespace */
	for (ptr = string; isspace (*ptr); ptr++);

	/* shift downward */
	if (ptr != string)
		string_move (string, ptr);

	if (!string[0])
		return string;

	/* look backwards */
	ptr = string + strlen (string) - 1;

	if (isspace (*ptr))
	{
		while (ptr >= string && isspace (*ptr))
			ptr--;

		ptr[1] = 0;
	}

	return string;
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

#if 0
int main (int argc, char **argv)
{
	String *s;

	s = string_new (NULL, 0, 0, TRUE);

	string_append (s, "s: ");

	while (argc--)
		string_appendf (s, "%s ", *argv++);

	string_appendc (s, '\n');

	printf ("%s", s->str);

	string_free (s);

	return 0;
}
#endif
