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
 * $Id: xcommon.h,v 1.22 2002/08/29 19:45:11 chnix Exp $
 */
#ifndef _XCOMMON_H
#define _XCOMMON_H

#include <stdarg.h>

typedef struct {
	char *buf;
	size_t len;
	size_t alloc;
} buffer;

/* open a tcp connection, return -1 on error */
int xconnect(char *host, char *port);

/* reads lines from a fd. Calls handler on each line, without the newline. */
/* see xcommon.c for explanation */
int xgetlines(int fd, buffer * buf, int (*handle) (char *buf), char *(*separator) (char *));

void xputs(const char *s, int fd);

#endif
