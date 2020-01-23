/*
 * $Id: gt_netorg.h,v 1.5 2003/04/08 01:15:23 hipnod Exp $
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

#ifndef __GT_NETORG_H__
#define __GT_NETORG_H__

/*****************************************************************************/

struct _gt_node;

typedef struct _gt_node* (*GtConnForeachFunc) (Connection *c, 
                                               struct _gt_node *node, 
                                               void *udata);

/* backward compatibility */
typedef GtConnForeachFunc ConnForeachFunc;

/*****************************************************************************/

void                 gt_conn_add      (struct _gt_node *c);
void                 gt_conn_remove   (struct _gt_node *c);
struct _gt_node     *gt_conn_foreach  (GtConnForeachFunc func, void *udata,
                                       GtNodeClass klass, GtNodeState state, 
                                       int iter);

/*****************************************************************************/

int         gt_conn_maintain ();
int         gt_conn_length   (GtNodeClass klass, 
                              GtNodeState state);
GtNode     *gt_conn_random   (GtNodeClass klass, 
                              GtNodeState state);

void        gt_conn_sort     (CompareFunc func);
int         gt_conn_sort_vit (struct _gt_node *a, struct _gt_node *b);

#if 0
void        conn_clear    (GtConnForeachFunc func);
int         conn_auth     (Connection *c, int outgoing);
#endif

int         gt_conn_need_connections ();

/*****************************************************************************/

#endif /* __GT_NETORG_H__ */
