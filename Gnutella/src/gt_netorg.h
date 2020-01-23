/*
 * $Id: gt_netorg.h,v 1.16 2004/01/04 06:01:20 hipnod Exp $
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

#ifndef GIFT_GT_NETORG_H_
#define GIFT_GT_NETORG_H_

/*****************************************************************************/

/* number of ultrapeers we try to maintain connections to if we are a leaf */
#define GT_SHIELDED_CONNECTIONS    gt_config_get_int("main/connections=3")

/* number of leaf connections we will maintain as ultrapeer */
#define GT_LEAF_CONNECTIONS        gt_config_get_int("main/leaf_connections=0")

/* number of ultrapeer<->ultrapeer connections we maintain if we're one */
#define GT_PEER_CONNECTIONS        gt_config_get_int("main/peer_connections=3")

/* timeouts for connecting to nodes at different stages */
#define TIMEOUT_1                  gt_config_get_int("handshake/timeout1=20")
#define TIMEOUT_2                  gt_config_get_int("handshake/timeout2=40")
#define TIMEOUT_3                  gt_config_get_int("handshake/timeout3=60")

/*****************************************************************************/

/* return how many connections we need for the given node class */
int      gt_conn_need_connections (gt_node_class_t klass);

/*****************************************************************************/

void     gt_netorg_init      (void);
void     gt_netorg_cleanup   (void);

/*****************************************************************************/

#endif /* GIFT_GT_NETORG_H_ */
