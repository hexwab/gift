/*
 * $Id: gt_node_list.h,v 1.6 2004/01/04 06:01:20 hipnod Exp $
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

#ifndef GIFT_GT_NODE_LIST_H_
#define GIFT_GT_NODE_LIST_H_

/*****************************************************************************/

typedef GtNode* (*GtConnForeachFunc) (TCPC *c, GtNode *node, void *udata);
#define GT_CONN_FOREACH(func) ((GtConnForeachFunc)func)

/*****************************************************************************/

void        gt_conn_add          (GtNode *node);
void        gt_conn_remove       (GtNode *node);
GtNode     *gt_conn_foreach      (GtConnForeachFunc func, void *udata,
                                  gt_node_class_t klass, gt_node_state_t state,
                                  int iter);

/*****************************************************************************/

int         gt_conn_length       (gt_node_class_t klass,
                                  gt_node_state_t state);
GtNode     *gt_conn_random       (gt_node_class_t klass,
                                  gt_node_state_t state);
void        gt_conn_trim         (void);

void        gt_conn_sort         (CompareFunc func);
int         gt_conn_sort_vit     (GtNode *a, GtNode *b);
int         gt_conn_sort_vit_neg (GtNode *a, GtNode *b);

/*****************************************************************************/

void        gt_conn_set_state    (GtNode *node, gt_node_state_t old_state,
                                  gt_node_state_t new_state);
void        gt_conn_set_class    (GtNode *node, gt_node_class_t old_class,
                                  gt_node_class_t new_class);

/*****************************************************************************/

/* update or load the list of nodes in ~/.giFT/Gnutella/nodes */
void        gt_node_list_save    (void);
void        gt_node_list_load    (void);

/*****************************************************************************/

#endif /* GIFT_GT_NODE_LIST_H_ */
