/*
 * $Id: ft_handshake.h,v 1.3 2003/06/24 19:57:20 jasta Exp $
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

#ifndef __FT_HANDSHAKE_PROTO_H
#define __FT_HANDSHAKE_PROTO_H

/*****************************************************************************/

FT_HANDLER (ft_version_request);
FT_HANDLER (ft_version_response);
FT_HANDLER (ft_nodeinfo_request);
FT_HANDLER (ft_nodeinfo_response);
FT_HANDLER (ft_nodelist_request);
FT_HANDLER (ft_nodelist_response);
FT_HANDLER (ft_nodecap_request);
FT_HANDLER (ft_nodecap_response);
FT_HANDLER (ft_ping_request);
FT_HANDLER (ft_ping_response);
FT_HANDLER (ft_session_request);
FT_HANDLER (ft_session_response);

/*****************************************************************************/

#endif /* __FT_HANDSHAKE_PROTO_H */
