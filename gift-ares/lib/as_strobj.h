/*
 * $Id: as_strobj.h,v 1.3 2004/11/20 10:22:52 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_STRING_H
#define __AS_STRING_H

/*****************************************************************************/

/* Subset of the gift string object. Always manages memory itself. */

typedef struct
{
	char *str;    /* Actual string */
	int alloc;    /* Number pf allocated bytes */
	int len;      /* Number of bytes used by str (there will always be an
	               * additional trailing '\0') */
} String;

/*****************************************************************************/

/* init suplied  string object */
void string_init (String *sobj);

/* clean up supplied string object and free str data */
void string_finish (String *sobj);

/* clean up supplied string object but keep str itself and return it */
char *string_finish_keep (String *sobj);

/*****************************************************************************/

/* Create string object. Always call with string_new (NULL, 0, 0, TRUE) */
String *string_new (char *str, int alloc, int len, as_bool can_resize);

/* free string object and str data */
void string_free (String *sobj);

/* free string object but keep string itself and return it */
char *string_free_keep (String *sobj);

/*****************************************************************************/

/* various append operations */

int string_appendvf (String *sobj, const char *fmt, va_list args);

int string_appendf (String *sobj, const char *fmt, ...);

int string_append (String *sobj, const char *str);

int string_appendu (String *sobj, unsigned char *str, size_t len);

int string_appendc (String *sobj, char c);

/*****************************************************************************/

#endif /* __AS_STRING_H */
