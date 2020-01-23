/*
 * $Id: ft_conf.h,v 1.27 2003/12/26 06:44:20 jasta Exp $
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

/**
 * @file ft_conf.h
 *
 * @brief Main configuration interface for OpenFT options
 *
 * This interface simply wraps the interaction with libgift's conf.[ch] to
 * make it easier to abstract future options using constant values.
 */

/*****************************************************************************/

/*
 * Please note that this is a work in progress.  Not all options are
 * defined here.
 */
#define FT_CFG_INITIAL_WEIGHT     (90)
#define FT_CFG_MAINT_TIMER        (2 * MINUTES)

#define FT_CFG_LOCAL_MODE         ft_cfg_get_int("local/lan_mode=0")
#define FT_CFG_LOCAL_ALLOW        ft_cfg_get_str("local/hosts_allow=LOCAL")

#define FT_CFG_SEARCH_PARENTS     ft_cfg_get_int("search/parents=1")
#define FT_CFG_SEARCH_MINPEERS    ft_cfg_get_int("search/minpeers=8")
#define FT_CFG_SEARCH_MAXPEERS    ft_cfg_get_int("search/peers=12")
#define FT_CFG_SEARCH_TTL         ft_cfg_get_int("search/default_ttl=2")
#define FT_CFG_LOCATE_TTL         ft_cfg_get_int("search/locate_ttl=3")
#define FT_CFG_SEARCH_RESULTS     ft_cfg_get_int("search/max_results=800")
#define FT_CFG_SEARCH_RESULTS_REQ ft_cfg_get_int("search/max_results_req=800")
#define FT_CFG_MAX_TTL            ft_cfg_get_int("search/max_ttl=3")
#define FT_CFG_MAX_RESULTS        FT_CFG_SEARCH_RESULTS
#define FT_CFG_MAX_CHILDREN       ft_cfg_get_int("search/children=100")
#define FT_CFG_MIN_CHILDREN       ft_cfg_get_int("search/min_children=30")
#define FT_CFG_MAX_CONNECTIONS    (30)
#define FT_CFG_MAX_ACTIVE         ft_cfg_get_int("connections/max_active=-1")

#define FT_CFG_PROMOTE_CHANCE     (0.1)
#define FT_CFG_DEMOTE_CHANCE      (0.002)

#define FT_CFG_MAX_ACTIVE         ft_cfg_get_int("connections/max_active=-1")

#define FT_CFG_SEARCH_ENV_PATH    ft_cfg_get_path("search/env_path", "OpenFT/db")
#define FT_CFG_SEARCH_ENV_CACHE   ft_cfg_get_int("search/env_cache=31457280")
#define FT_CFG_SEARCH_ENV_PRIV    ft_cfg_get_int("search/env_priv=1")
#define FT_CFG_SEARCH_ENV_TXN     ft_cfg_get_int("search/env_txn=0")

#define FT_CFG_NODE_PORT          ft_cfg_get_int("main/port=1215")
#define FT_CFG_NODE_HTTP_PORT     ft_cfg_get_int("main/http_port=1216")
#define FT_CFG_NODE_CLASS         ft_cfg_get_int("main/class=1")
#define FT_CFG_NODE_CLASS_ALLOW   ft_cfg_get_int("main/class_allow=3");
#define FT_CFG_NODE_ALIAS         ft_cfg_get_str("main/alias")

#define FT_CFG_NODES_CACHE_MAX    (500)

#define FT_CFG_SEARCH_VFY         (0)
#define FT_CFG_SEARCH_VFY_NOISY   (0)

#define FT_CFG_SEARCH_NOISY       BOOL_EXPR (ft_cfg_get_int("search/noisy=0"))

/*****************************************************************************/

int ft_cfg_get_int (char *keypath);
char *ft_cfg_get_str (char *keypath);
char *ft_cfg_get_path (char *keypath, char *def);

/*****************************************************************************/

#endif /* __FT_CONF_H */
