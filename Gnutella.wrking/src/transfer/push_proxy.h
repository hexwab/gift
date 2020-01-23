/*
 * $Id: push_proxy.h,v 1.1 2004/03/24 06:33:09 hipnod Exp $
 *
 * Copyright (C) 2004 giFT project (gift.sourceforge.net)
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

#ifndef GIFT_GT_PUSH_PROXY_H_
#define GIFT_GT_PUSH_PROXY_H_

/*****************************************************************************/

void      gt_push_proxy_add             (GtNode *node, in_addr_t ipv4,
                                         in_port_t port);
void      gt_push_proxy_del             (GtNode *node);
BOOL      gt_push_proxy_get_ggep_block  (uint8_t **block, size_t *block_len);

/*****************************************************************************/

void      gt_push_proxy_init            (void);
void      gt_push_proxy_cleanup         (void);

/*****************************************************************************/

#endif /* GIFT_GT_PUSH_PROXY_H_ */
