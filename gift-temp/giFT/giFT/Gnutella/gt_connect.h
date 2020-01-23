/*
 * $Id: gt_connect.h,v 1.4 2003/04/16 04:35:56 hipnod Exp $
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

#ifndef __GT_CONNECT_H__
#define __GT_CONNECT_H__

/*****************************************************************************/

struct _gt_node;

int   gnutella_send_connection_headers (Connection *c);
void  gnutella_start_connection        (int fd, input_id id, Connection *c);


void  gt_connect_test     (struct _gt_node *node, unsigned short port);
int   gt_connect          (struct _gt_node *node);

/*****************************************************************************/

#endif /* __GT_CONNECT_H__ */
