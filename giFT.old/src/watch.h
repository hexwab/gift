/*
 * watch.h
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

#ifndef __WATCH_H
#define __WATCH_H

/*****************************************************************************/

void  watch_add    (Connection *c);
void  watch_remove (Connection *c);
List *watch_copy   ();

/*****************************************************************************/

#endif /* __WATCH_H */
