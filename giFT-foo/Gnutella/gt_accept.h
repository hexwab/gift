/*
 * $Id: gt_accept.h,v 1.5 2003/05/21 07:32:31 hipnod Exp $
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

#ifndef __GT_ACCEPT_H__
#define __GT_ACCEPT_H__

/*****************************************************************************/

void  gnutella_handle_incoming  (int fd, input_id, TCPC *c);
void  gnutella_determine_method (int fd, input_id, TCPC *c);

int   gnutella_auth_connection  (TCPC *c);

void  http_headers_parse       (char *headers, Dataset **dataset);
int   http_headers_terminated  (char *data, size_t len);

/*****************************************************************************/

#endif /*  __GT_ACCEPT_H__ */
