/*
 * $Id: gt_accept.h,v 1.9 2004/02/23 04:20:07 hipnod Exp $
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

#ifndef GIFT_GT_ACCEPT_H_
#define GIFT_GT_ACCEPT_H_

/*****************************************************************************/

void  gnutella_handle_incoming  (int fd, input_id, TCPC *c);

BOOL  gnutella_will_deflate     (struct gt_node *node);
void  gnutella_mark_compression (struct gt_node *node);

BOOL  gnutella_auth_connection  (TCPC *c);

void  gt_http_header_parse      (char *headers, Dataset **dataset);
BOOL  gt_http_header_terminated (char *data, size_t len);

void  gt_handshake_dispatch_incoming (int fd, input_id id, TCPC *c);

/*****************************************************************************/

#endif /* GIFT_GT_ACCEPT_H_ */
