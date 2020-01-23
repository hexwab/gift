/*
 * $Id: gt_ban.h,v 1.2 2003/12/16 09:32:46 hipnod Exp $
 *
 * Copyright (C) 2003 giFT project (gift.sourceforge.net)
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

#ifndef GIFT__GT_BAN_H__
#define GIFT__GT_BAN_H__

/*****************************************************************************/

#define     BAN_DEBUG            gt_config_get_int("ban/debug=0")

/*****************************************************************************/

BOOL    gt_ban_ipv4              (in_addr_t address, unsigned int netmask);
BOOL    gt_ban_ipv4_is_banned    (in_addr_t address);

void    gt_ban_init              (void);
void    gt_ban_cleanup           (void);

/*****************************************************************************/
#endif /* GIFT__GT_BAN_H__ */
