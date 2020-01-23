/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: format.h,v 1.16 2003/06/27 11:20:13 weinholt Exp $
 */
#ifndef _FORMAT_H
#define _FORMAT_H

typedef struct precompiled *format_t;

/* This union can hold all kinds of values used by format strings */
typedef union {
	const char *string;			/* type = ATTR_STRING (nul-terminated) */
	unsigned int intval;		/* type = ATTR_INT */
	guint64 longval;			/* type = ATTR_LONG */
	struct {					/* type = ATTR_STRLEN (fixed length) */
		const char *string;		/* - pointer to first character */
		int len;				/* - string length */
	} strlen;
} attr_value;

enum attr_type { ATTR_STRING, ATTR_INT, ATTR_LONG, ATTR_NONE, ATTR_STRLEN };

/* This describes how to get meta data from an object.
 * The return values of the different meta data types are:
 */
typedef enum attr_type (*getattrF) (const void *udata, const char *key, attr_value * value);

/* This function takes a string, parses it and returns a linked list of
 * atoms. If an error occurs, A message is printed, and NULL is returned. */
format_t format_compile(const char *src);

/* Copies a compiled format */
format_t format_ref(format_t n);

/* Frees a compiled format */
void format_unref(format_t n);

/* Define a macro */
format_t format_load(const char *id, const char *standard);

/* Fetch and reference a macro */
format_t format_get(const char *id, const char *standard);

#include "list.h"

/* sorts according to a format string */
int make_sortkey(char **res, const void *item, const char *format, getattrF getattr);

/* Formats a hit according to a format and a width */
char *format_expand(format_t format, getattrF, int maxlen, const void *udata);

/* returns the free disk space in giFT download directory or -1 */
guint64 disk_free(void);

/* free some memory allocated by macros */
void format_clear(void);

#endif
