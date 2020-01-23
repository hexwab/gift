/*
 * $Id: gt_http_server.h,v 1.4 2003/06/01 09:16:18 hipnod Exp $
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

#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

/*****************************************************************************/

void   gt_http_server_indirect_add    (Chunk *chunk, in_addr_t ip,
                                    char *request);
void   gt_http_server_indirect_remove (Chunk *chunk, in_addr_t ip, char *request);
Chunk *gt_http_server_indirect_lookup (in_addr_t ip, char *request);

/*****************************************************************************/

void gt_http_server_incoming (int fd, input_id id, TCPC *c);

int  gt_server_setup_upload  (GtTransfer *xfer);
void gt_server_upload_file   (int fd, input_id id, TCPC *c);

void gt_http_server_reset    (TCPC *c);

void gt_get_client_request (int fd, input_id id, TCPC *c);

/*****************************************************************************/

#endif /* __HTTP_SERVER_H */
