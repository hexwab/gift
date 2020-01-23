/*
 * ft_conf.h
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

#ifndef __FT_CONF_H
#define __FT_CONF_H

/*****************************************************************************/

/* this is a work in progress */
#define FT_INITIAL_WEIGHT  60
#define FT_LOCAL_MODE      ft_config_get_int("local/lan_mode=0")
#define FT_LOCAL_ALLOW     ft_config_get_str("local/hosts_allow=LOCAL")
#define FT_MAX_PARENTS     ft_config_get_int("sharing/parents=3")
#define FT_MAX_CONNECTIONS 30

/*****************************************************************************/

int ft_config_get_int (char *keypath);
char *ft_config_get_str (char *keypath);

/*****************************************************************************/

#endif /* __FT_CONF_H */
