/*
 * $Id: ft_conf.h,v 1.14 2003/05/27 04:15:49 jasta Exp $
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

#ifndef __FT_CONF_H
#define __FT_CONF_H

/*****************************************************************************/

/* this is a work in progress */
#define FT_INITIAL_WEIGHT     60
#define FT_LOCAL_MODE         ft_config_get_int("local/lan_mode=0")
#define FT_LOCAL_ALLOW        ft_config_get_str("local/hosts_allow=LOCAL")
#define FT_SEARCH_PARENTS     ft_config_get_int("search/parents=1")
#define FT_SEARCH_PEERS       ft_config_get_int("search/peers=8")
#define FT_SEARCH_TTL         ft_config_get_int("search/default_ttl=3")
#define FT_SEARCH_RESULTS     ft_config_get_int("search/max_results=800")
#define FT_SEARCH_RESULTS_REQ ft_config_get_int("search/max_results_req=800")
#define FT_MAX_TTL            ft_config_get_int("search/max_ttl=3")
#define FT_MAX_RESULTS        FT_SEARCH_RESULTS
#define FT_MAX_CHILDREN       ft_config_get_int("search/children=500")
#define FT_MAX_CONNECTIONS    30 /* (WARNING: misleading value) */

#define FT_SEARCH_ENV_PATH    ft_config_get_path("search/env_path", "OpenFT/db")
#define FT_SEARCH_ENV_CACHE   ft_config_get_int("search/env_cache=83886080")
#define FT_SEARCH_ENV_PRIV    ft_config_get_int("search/env_priv=0")
#define FT_SEARCH_ENV_TXN     ft_config_get_int("search/env_txn=0")

/*****************************************************************************/

int ft_config_get_int (char *keypath);
char *ft_config_get_str (char *keypath);
char *ft_config_get_path (char *key, char *def);

/*****************************************************************************/

#endif /* __FT_CONF_H */
