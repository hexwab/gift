/*
 * $Id: gt_conf.h,v 1.2 2004/03/24 06:27:40 hipnod Exp $
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

#ifndef GIFT_GT_CONF_H_
#define GIFT_GT_CONF_H_

/*****************************************************************************/

char   *gt_config_get_str   (const char *key);
int     gt_config_get_int   (const char *key);

/*****************************************************************************/

BOOL    gt_config_load_file (const char *path, BOOL update, BOOL force);

/*****************************************************************************/

BOOL    gt_config_init      (void);
void    gt_config_cleanup   (void);

/*****************************************************************************/

#endif /* GIFT_GT_CONF_H_ */
