/*
 * netorg.h
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

#ifndef __NETORG_H
#define __NETORG_H

/*****************************************************************************/

typedef Connection* (*ConnForeachFunc) (Connection *c, Node *node, void *data);

/*****************************************************************************/

void        conn_add      (Connection *c);
void        conn_change   (Connection *c, unsigned short prev_state);
void        conn_remove   (Connection *c);
Connection *conn_lookup   (unsigned long ip);
void        conn_sort     (CompareFunc func);
void        conn_maintain ();
Connection *conn_foreach  (ConnForeachFunc func, void *user_data,
                           unsigned short klass, unsigned short state,
                           unsigned short iter);
void        conn_clear    (ConnForeachFunc func);
int         conn_length   (unsigned short klass, unsigned short state);
int         conn_auth     (Connection *c, int outgoing);

/*****************************************************************************/

#endif /* __NETORG_H */
