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
 * $Id: protocol.h,v 1.11 2002/11/22 20:48:32 weinholt Exp $
 */

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

struct _ntree;

typedef struct _ntree ntree;

typedef void (*PForEachFunc) (ntree *, void *udata);

char *interface_construct(ntree *);
ntree *interface_parse(char *packet_str);
void interface_free(ntree *);
char *interface_lookup(ntree *, char *key);
ntree *interface_append(ntree **, char *key, char *value);
ntree *interface_append_int(ntree ** packet, char *key_name, unsigned int value);

void interface_foreach(ntree *, PForEachFunc func, void *udata);
void interface_foreach_key(ntree *, PForEachFunc func, void *udata);
char *interface_name(ntree *);
char *interface_value(ntree *);

#endif
