/*
 * ft_http_client.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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
void http_client_push  (in_addr_t ip, unsigned short port, char *request,
                        off_t start, off_t stop);
void http_client_reset (Connection *c);

int http_handle_code (FTTransfer *xfer, int code);
void get_read_file     (Protocol *p, Connection *c);

/*****************************************************************************/

#endif /* __HTTP_CLIENT_H */
