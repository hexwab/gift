/*
 * $Id: gt_node_cache.h,v 1.4 2004/01/04 06:01:20 hipnod Exp $
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

#ifndef GIFT__GT_NODE_CACHE_H__
#define GIFT__GT_NODE_CACHE_H__

/*****************************************************************************/

struct ipv4_addr
{
	in_addr_t ip;
	in_port_t port;
};

struct cached_node
{
	struct ipv4_addr  addr;
	gt_node_class_t   klass;
	time_t            timestamp;
	time_t            uptime;
	in_addr_t         src_ip;
};

/*****************************************************************************/

void         gt_node_cache_add_ipv4    (in_addr_t ipv4, in_port_t port,
                                        gt_node_class_t klass,
                                        time_t timestamp, time_t uptime,
                                        in_addr_t src_ip);
void         gt_node_cache_del_ipv4    (in_addr_t ipv4, in_port_t port);

List        *gt_node_cache_get_remove  (size_t max_len);
List        *gt_node_cache_get         (size_t max_len);

void         gt_node_cache_trace       (void);

/*****************************************************************************/

void         gt_node_cache_load        (void);
void         gt_node_cache_save        (void);

/*****************************************************************************/

void         gt_node_cache_init        (void);
void         gt_node_cache_cleanup     (void);

/*****************************************************************************/

#endif /* GIFT__GT_NODE_CACHE_H__ */
