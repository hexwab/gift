/*
 * $Id: gt_http_server.h,v 1.7 2004/02/23 04:20:07 hipnod Exp $
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

#ifndef GIFT_GT_HTTP_SERVER_H_
#define GIFT_GT_HTTP_SERVER_H_

/*****************************************************************************/

void    gt_http_server_incoming  (int fd, input_id id, TCPC *c);

int     gt_server_setup_upload   (GtTransfer *xfer);
void    gt_server_upload_file    (int fd, input_id id, GtTransfer *xfer);

void    gt_http_server_dispatch  (int fd, input_id id, TCPC *c);
void    gt_http_server_reset     (TCPC *c);

/*****************************************************************************/

#endif /* GIFT_GT_HTTP_SERVER_H_ */
