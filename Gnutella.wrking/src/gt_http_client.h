/*
 * $Id: gt_http_client.h,v 1.9 2004/01/18 05:40:56 hipnod Exp $
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

#ifndef GIFT_GT_HTTP_CLIENT_H_
#define GIFT_GT_HTTP_CLIENT_H_

/*****************************************************************************/

void    gt_http_client_get     (Chunk *chunk, GtTransfer *xfer);
void    gt_http_client_push    (in_addr_t ip, in_port_t port,
                                char *request, off_t start, off_t stop);
void    gt_http_client_reset   (TCPC *c);

int     gt_http_handle_code    (GtTransfer *xfer, int code);
void    gt_http_client_start   (int fd, input_id id, GtTransfer *xfer);
void    gt_get_read_file       (int fd, input_id id, GtTransfer *xfer);

/*****************************************************************************/

#endif /* GIFT_GT_HTTP_CLIENT_H_ */
