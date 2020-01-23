/*
 * $Id: ft_sharing.h,v 1.2 2003/05/26 11:47:40 jasta Exp $
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

#ifndef __FT_SHARING_H
#define __FT_SHARING_H

/*****************************************************************************/

FT_HANDLER (ft_child_request);
FT_HANDLER (ft_child_response);
FT_HANDLER (ft_addshare_request);
FT_HANDLER (ft_addshare_response);
FT_HANDLER (ft_remshare_request);
FT_HANDLER (ft_remshare_response);
FT_HANDLER (ft_modshare_request);
FT_HANDLER (ft_modshare_response);
FT_HANDLER (ft_stats_request);
FT_HANDLER (ft_stats_response);

/*****************************************************************************/

#endif /* __FT_SHARING_H */
