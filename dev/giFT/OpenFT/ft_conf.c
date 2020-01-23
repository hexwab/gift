/*
 * ft_conf.c
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

#include "ft_openft.h"

#include "ft_conf.h"

/*****************************************************************************/

/* simple (and pointless) abstraction */
#define FT_CONF OPENFT->conf

/*****************************************************************************/

int ft_config_get_int (char *keypath)
{
	return config_get_int (FT_CONF, keypath);
}

char *ft_config_get_str (char *keypath)
{
	return config_get_str (FT_CONF, keypath);
}
