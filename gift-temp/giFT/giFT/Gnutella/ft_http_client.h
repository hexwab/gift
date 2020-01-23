/*
 * $Id: ft_http_client.h,v 1.4 2003/03/20 05:01:09 rossta Exp $
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

#ifndef __HTTP_CLIENT_H
#define __HTTP_CLIENT_H

/*****************************************************************************/

void gt_http_client_get   (HTTP_Protocol *http, Chunk *chunk, GtTransfer *xfer);
void gt_http_client_push  (HTTP_Protocol *http, in_addr_t ip, unsigned short port,
                        char *request, off_t start, off_t stop);
void gt_http_client_reset (Connection *c);

int gt_http_handle_code (GtTransfer *xfer, int code);
void gt_http_client_start (int fd, input_id id, Connection *c);
void gt_get_read_file     (int fd, input_id id, Connection *c);

/*****************************************************************************/

#endif /* __HTTP_CLIENT_H */
