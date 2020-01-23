/*
 * $Id: gt_netorg.h,v 1.11 2003/06/07 07:13:01 hipnod Exp $
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

/* number of ultrapeers we try to maintain connections to if we are a leaf */
#define GT_SHIELDED_CONNECTIONS         gt_config_get_int("main/connections=3")

/* number of leaf connections we will maintain as ultrapeer */
#define GT_LEAF_CONNECTIONS          gt_config_get_int("main/leaf_connections=0")

/* number of ultrapeer<->ultrapeer connections we maintain if we're one */
#define GT_PEER_CONNECTIONS          gt_config_get_int("main/peer_connections=3")

/*****************************************************************************/

struct gt_node;

typedef struct gt_node* (*GtConnForeachFunc) (TCPC *c, 
                                               struct gt_node *node, 
                                               void *udata);

/* backward compatibility */
typedef GtConnForeachFunc ConnForeachFunc;
#define GT_CONN_FOREACH(func) ((GtConnForeachFunc)func)

/*****************************************************************************/

void                 gt_conn_add      (struct gt_node *c);
void                 gt_conn_remove   (struct gt_node *c);
struct gt_node     *gt_conn_foreach  (GtConnForeachFunc func, void *udata,
                                       GtNodeClass klass, GtNodeState state, 
                                       int iter);

/*****************************************************************************/

int         gt_conn_length   (GtNodeClass klass, 
                              GtNodeState state);
GtNode     *gt_conn_random   (GtNodeClass klass, 
                              GtNodeState state);

void        gt_conn_sort     (CompareFunc func);
int         gt_conn_sort_vit (struct gt_node *a, struct gt_node *b);

#if 0
void        conn_clear    (GtConnForeachFunc func);
int         conn_auth     (TCPC *c, int outgoing);
#endif

/* return how many connections we need for the given node class */
int         gt_conn_need_connections (GtNodeClass klass);

/*****************************************************************************/

void     gt_netorg_init      (void);
void     gt_netorg_cleanup   (void);

/*****************************************************************************/

#endif /* __GT_NETORG_H__ */
