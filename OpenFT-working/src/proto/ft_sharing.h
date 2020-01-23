/*
 * $Id: ft_sharing.h,v 1.1 2003/07/18 01:06:39 jasta Exp $
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

#ifndef __FT_SHARING_PROTO_H
#define __FT_SHARING_PROTO_H

/*****************************************************************************/

FT_HANDLER (ft_child_request);
FT_HANDLER (ft_child_response);
FT_HANDLER (ft_child_prop);
FT_HANDLER (ft_share_sync_begin);
FT_HANDLER (ft_share_sync_end);
FT_HANDLER (ft_share_add_request);
FT_HANDLER (ft_share_add_error);
FT_HANDLER (ft_share_remove_request);
FT_HANDLER (ft_share_remove_error);

/*****************************************************************************/

#endif /* __FT_SHARING_PROTO_H */
