/*
 * $Id: gt_conf.h,v 1.1 2003/06/07 05:24:14 hipnod Exp $
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

#ifndef __GT_CONF_H__
#define __GT_CONF_H__

/*****************************************************************************/

char   *gt_config_get_str   (char *key);
int     gt_config_get_int   (char *key);

/*****************************************************************************/

BOOL    gt_config_load_file (char *path, BOOL update, BOOL force);

/*****************************************************************************/

BOOL    gt_config_init      (void);
void    gt_config_cleanup   (void);

/*****************************************************************************/

#endif /* __GT_CONF_H__ */
