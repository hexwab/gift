/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: format.h,v 1.6 2002/11/23 16:23:56 chnix Exp $
 */
#ifndef _FORMAT_H
#define _FORMAT_H

typedef struct precompiled *format_t;

/* This function takes a string, parses it and returns a linked list of
 * atoms. If an error occurs, A message is printed, and NULL is returned. */
format_t format_compile(char *src);

/* Copies a compiled format */
format_t format_ref(format_t n);

/* Frees a compiled format */
void format_unref(format_t n);

/* Define a macro */
format_t format_load(char *id, char *standard);

/* Fetch and reference a macro */
format_t format_get(char *id, char *standard);

#include "search.h"
#include "transfer.h"

/* Formats a hit according to a format and a width */
char *format_hit(format_t format, hit * h, int width);
char *format_subhit(format_t format, subhit * h, int width);

/* Formats a transfer according to a format and a width */
char *format_transfer(format_t format, transfer * t, int width);
char *format_source(format_t format, source * t, int width);

void format_clear(void);

#endif
