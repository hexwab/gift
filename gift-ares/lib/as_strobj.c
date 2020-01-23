/*
 * $Id: as_strobj.c,v 1.4 2004/11/20 10:22:52 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* default size for string buffer */
#define STRING_SIZE 128

/*****************************************************************************/

/* init suplied  string object */
void string_init (String *sobj)
{
	assert (sobj != NULL);

	sobj->str = NULL;
	sobj->alloc = 0;
	sobj->len = 0;
}

/* clean up supplied string object and free str data */
void string_finish (String *sobj)
{
	assert (sobj != NULL);

	free (sobj->str);

	sobj->str  = NULL;
	sobj->alloc = 0;
	sobj->len = 0;
}

/* clean up supplied string object but keep str itself and return it */
char *string_finish_keep (String *sobj)
{
	char *str = sobj->str;

	assert (sobj != NULL);

	sobj->str  = NULL;
	sobj->alloc = 0;
	sobj->len = 0;

	return str;
}

/*****************************************************************************/

/* Create string object. Always call with string_new (NULL, 0, 0, TRUE) */
String *string_new (char *str, int alloc, int len, as_bool can_resize)
{
	String *sobj;

	/* we only implement a subset of libgift's functionality */
	assert (str == NULL);
	assert (alloc == 0);
	assert (len == 0);
	assert (can_resize == TRUE);

	if (!(sobj = malloc (sizeof (String))))
		return NULL;

	string_init (sobj);

	return sobj;
}

/* free string object and str data */
void string_free (String *sobj)
{
	if (!sobj)
		return;

	string_finish (sobj);
	free (sobj);
}

/* free string object but keep string itself and return it */
char *string_free_keep (String *sobj)
{
	char *str;

	if (!sobj)
		return NULL;

	str = string_finish_keep (sobj);
	free (sobj);

	return str;
}

/*****************************************************************************/

static int string_resize (String *sobj, int new_size)
{
	char *newbuf;

	/* always enlarge */
	if (new_size <= sobj->alloc)
		return sobj->alloc;

	if (!(newbuf = realloc (sobj->str, new_size)))
		return 0;

	sobj->str = newbuf;
	sobj->alloc = new_size;

	return sobj->alloc;
}

/*****************************************************************************/

int string_appendvf (String *sobj, const char *fmt, va_list args)
{
	va_list args_cpy;
	int written = 0;
	int remaining;

	if (!sobj)
		return 0;

	/* make sure we got something to work with */
	if (sobj->alloc == 0)
	{
		if (string_resize (sobj, STRING_SIZE) == 0)
			return 0;
	}

	/* loop until the buffer is big enough */
	for (;;)
	{
		if (sobj->len < sobj->alloc) /* make sure there is room for '\0' */
		{
			remaining = sobj->alloc - sobj->len;

			/*
			 * We have to make a copy of the va_list because we may pass this
			 * point multiple times. Note that simply calling va_start again is
			 * not a good idea since the va_start/va_end should be in the same
			 * stack frame because of some obscure implementations.
			 */
			VA_COPY (args_cpy, args);
			written = vsnprintf (sobj->str + sobj->len, remaining, fmt,
			                     args_cpy);
			va_end (args_cpy);

			/* break if we have written everything */
			if (written > -1 && written < remaining)
				break;
		}

		/* if we got here, we don't have enough memory in the underlying buffer
		 * to fit this fmt buffer */
		if (string_resize (sobj, sobj->alloc * 2) == 0)
			return 0;
	}

	sobj->len += written;

	return written;
}

int string_appendf (String *sobj, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start (args, fmt);
	ret = string_appendvf (sobj, fmt, args);
	va_end (args);

	return ret;
}

int string_append (String *sobj, const char *str)
{
	return string_appendf (sobj, "%s", str);
}

int string_appendu (String *sobj, unsigned char *str, size_t len)
{
	if (!string_resize (sobj, sobj->len + len + 1))
		return 0;

	memcpy (sobj->str + sobj->len, str, len);
	sobj->len += len;

	/* make sure there is a traling NUL */
	sobj->str[sobj->len] = 0;

	return len;
}

int string_appendc (String *sobj, char c)
{
	return string_appendf (sobj, "%c", c);
}

/*****************************************************************************/
