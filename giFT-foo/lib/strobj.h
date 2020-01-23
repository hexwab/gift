/*
 * $Id: strobj.h,v 1.5 2003/06/21 18:54:45 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#ifndef __STRING_H
#define __STRING_H

/*****************************************************************************/

/**
 * @file strobj.h
 *
 * @brief Dynamic string manipulation.
 */

/*****************************************************************************/

typedef struct
{
	char *str;
	int   alloc;
	int   len;
	int   resizable;
} String;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

String       *string_new       (char *str, int alloc, int len, int resizable);
void          string_free      (String *s);
char         *string_free_keep (String *s);

int           string_appendf   (String *s, const char *fmt, ...);
int           string_append    (String *s, const char *str);
int           string_appendu   (String *s, unsigned char *str, size_t len);
int           string_appendc   (String *s, char c);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __STRING_H */
