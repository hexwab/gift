/*
 * $Id: gt_connect.h,v 1.9 2004/03/24 06:21:13 hipnod Exp $
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

#ifndef GIFT_GT_CONNECT_H_
#define GIFT_GT_CONNECT_H_

/*****************************************************************************/

#define HANDSHAKE_DEBUG         gt_config_get_int("handshake/debug=0")

/*****************************************************************************/

struct gt_node;

BOOL  gnutella_send_connection_headers (TCPC *c, const char *header);
void  gnutella_start_connection        (int fd, input_id id, TCPC *c);
BOOL  gnutella_parse_response_headers  (char *response, Dataset **headers);
void  gnutella_set_handshake_timeout   (TCPC *c, time_t time);

void  gt_connect_test     (struct gt_node *node, in_port_t port);
int   gt_connect          (struct gt_node *node);

/*****************************************************************************/

#endif /* GIFT_GT_CONNECT_H_ */
