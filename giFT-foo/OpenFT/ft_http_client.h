/*
 * $Id: ft_http_client.h,v 1.11 2003/05/05 09:49:09 jasta Exp $
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

void http_client_get   (Chunk *chunk, FTTransfer *xfer);
void http_client_push  (in_addr_t ip, in_port_t port, char *request,
                        off_t start, off_t stop);
void http_client_reset (TCPC *c);

int http_handle_code   (FTTransfer *xfer, int code);
void get_read_file     (int fd, input_id id, TCPC *c);

/*****************************************************************************/

#endif /* __HTTP_CLIENT_H */
