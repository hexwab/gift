/*
 * $Id: as_parse.c,v 1.1 2004/09/03 16:18:14 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"


/*****************************************************************************/

/* Behaves like sprintf but returns static buffer. Beware threading problems!
 */
char *stringf (const char *fmt, ...)
{
	static char buf[4096];
	va_list args;

	va_start (args, fmt);
	vsnprintf (buf, sizeof (buf) - 2, fmt, args);
	va_end (args);

	buf[sizeof (buf) - 1] = 0;

	return buf;
}

/* Like stringf but returns allocated string caller must free. Thread safe! */
char *stringf_dup (const char *fmt, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, fmt);
	vsnprintf (buf, sizeof (buf) - 2, fmt, args);
	va_end (args);

	buf[sizeof (buf) - 1] = 0;

	return strdup (buf);
}

/*****************************************************************************/

/* tolower()s entire string and returns it */
char *string_upper (char *s)
{
	char *p;

	if (!s)
		return NULL;

	for (p = s; *p; p++)
		*p = toupper (*p);

	return s;
}

/* toupper()s entire string and returns it */
char *string_lower (char *s)
{
	char *p;

	if (!s)
		return NULL;

	for (p = s; *p; p++)
		*p = toupper (*p);

	return s;
}

/*****************************************************************************/

/* Wrapper around strdup which checks s == NULL first. */
char *gift_strdup (const char *s)
{
	if (!s)
		return NULL;

	return strdup (s);
}

/* Like gift_strdup but copies len number of bytes. A '\0' will be appended
 * in all cases. If s == NULL or len == 0 the result will be NULL.
 */
char *gift_strndup (const char *s, size_t len)
{
	char *buf;

	if (!s || len == 0)
		return NULL;

	if (!(buf = malloc (len + 1)))
		return NULL;

	return gift_strncpy (buf, s, len);
}

/* Wrapper for strncpy which guarantees a '\0' at the end. dst must be at
 * least of size len + 1
 */
char *gift_strncpy (char *dst, const char *src, size_t len)
{
	assert (dst != NULL);
	assert (src != NULL);
	assert (len > 0);

	strncpy (dst, src, len);
	dst[len] = 0;

	return dst;
}

/*****************************************************************************/

/* Wrapper around strcmp which checks for s1 and/or s2 being NULL.
 * A non-NULL str is greater than a NULL one. If both are NULL s1 is greater.
 */
int gift_strcmp (const char *s1, const char *s2)
{
	if (!s1)
		return 1;
	else if (!s2)
		return -1;

	return strcmp (s1, s2);
}

/* Like gift_strcmp but case insensitive */
int gift_strcasecmp (const char *s1, const char *s2)
{
	if (!s1)
		return 1;
	else if (!s2)
		return -1;

#ifndef _MSC_VER
	return strcasecmp (s1, s2);
#else
	return _stricmp (s1, s2);
#endif
}

/*****************************************************************************/

/* Wrapper for strtol which return 0 if str == NULL. */
long gift_strtol (const char *str)
{
	if (!str)
		return 0;

	return strtol (str, NULL, 10);
}

/*****************************************************************************/

/* Looks for delim in *string and if found replaces first char of delimiter
 * with '\0' and moves *string past the delimiter. Returns pointer to the 
 * token before the delimiter or NULL if string, *string or **string are NULL.
 */
char *string_sep (char **string, const char *delim)
{
	char *p, *str;

	assert (delim);
	assert (*delim != '\0');

	if (!string || !*string || **string == '\0')
		return NULL;

	str = *string;

	if ((p = strstr (str, delim)))
	{
		*p = 0;
		p += strlen (delim);
	}

	*string = p;

	return str;
}

/*****************************************************************************/
