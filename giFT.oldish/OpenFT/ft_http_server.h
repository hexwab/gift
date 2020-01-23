/*
 * ft_http_server.h
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

#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

/*****************************************************************************/

void   http_server_indirect_add    (Chunk *chunk, in_addr_t ip,
                                    char *request);
void   http_server_indirect_remove (in_addr_t ip, char *request);
Chunk *http_server_indirect_lookup (in_addr_t ip, char *request);

/*****************************************************************************/

void http_server_incoming (Protocol *p, Connection *c);

int  server_setup_upload  (FTTransfer *xfer);
void server_upload_file   (Protocol *p, Connection *c);

void http_server_reset    (Connection *c);

/*****************************************************************************/

#endif /* __HTTP_SERVER_H */
