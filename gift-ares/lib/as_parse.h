/*
 * $Id: as_parse.h,v 1.1 2004/09/03 16:18:14 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_PARSE_H
#define __AS_PARSE_H

/*****************************************************************************/

/* Behaves like sprintf but returns static buffer. Beware threading problems!
 */
char *stringf (const char *fmt, ...);

/* Like stringf but returns allocated string caller must free. Thread safe! */
char *stringf_dup (const char *fmt, ...);

/*****************************************************************************/

/* tolower()s entire string and returns it */
char *string_upper (char *s);

/* toupper()s entire string and returns it */
char *string_lower (char *s);

/*****************************************************************************/

/* Wrapper around strdup which checks s == NULL first. */
char *gift_strdup (const char *s);

/* Like gift_strdup but copies len number of bytes. A '\0' will be appended
 * in all cases. If s == NULL or len == 0 the result will be NULL.
 */
char *gift_strndup (const char *s, size_t len);

/* Wrapper for strncpy which guarantees a '\0' at the end. dst must be at
 * least of size len + 1
 */
char *gift_strncpy (char *dst, const char *src, size_t len);

/*****************************************************************************/

/* Wrapper around strcmp which checks for s1 and/or s2 being NULL.
 * A non-NULL str is greater than a NULL one. If both are NULL s1 is greater.
 */
int gift_strcmp (const char *s1, const char *s2);

/* Like gift_strcmp but case insensitive */
int gift_strcasecmp (const char *s1, const char *s2);

/*****************************************************************************/

/* Wrapper for strtol which return 0 if str == NULL. */
long gift_strtol (const char *str);

/*****************************************************************************/

/* Looks for delim in *string and if found replaces first char of delimiter
 * with '\0' and moves *string past the delimiter. Returns pointer to the 
 * token before the delimiter or NULL if string, *string or **string are NULL.
 */
char *string_sep (char **string, const char *delim);

/*****************************************************************************/

#endif /* __AS_PARSE_H */
