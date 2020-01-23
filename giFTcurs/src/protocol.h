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
 * $Id: protocol.h,v 1.17 2003/06/27 11:20:14 weinholt Exp $
 */
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

struct _ntree;
typedef struct _ntree ntree;
typedef void (*PForEachFunc) (ntree *, void *udata);

/* return a newly allocated string with the given packet printed */
char *interface_construct(ntree *);

/* convert a string to packet. The given string is destroyed an used
 * as data storage for the tree values */
ntree *interface_parse(char *packet_str);

/* Append an (key, value) pair to a packet. Packet should be NULL the first
 * time. The strings are not copied, so make sure they are intact until
 * the packet is destroyed.
 */
void interface_append(ntree **, const char *key, const char *value);
void interface_append_int(ntree ** packet, const char *key_name, unsigned int value);

/* Get the name or value of an item */
const char *interface_lookup(ntree *, const char *key);
const char *interface_name(ntree *);
const char *interface_value(ntree *);
int interface_isempty(ntree *);

/* Iterate through a ntree */
void interface_foreach(ntree *, PForEachFunc func, void *udata);
void interface_foreach_key(ntree *, PForEachFunc func, void *udata);

/* destroy a tree. The values are not freed, only the structure */
void interface_free(ntree *);

#endif
