/*
 * conf.h
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

#ifndef __CONF_H
#define __CONF_H

#ifdef WIN32
#include <windows.h>
#define GIFT_REG_ROOT HKEY_CURRENT_USER
#define GIFT_REG_PATH "Software\\giFT"
#else
#define GIFT_CONFIG_PATH ".giFT"
#endif

struct _hash_table;
struct _list;

/*****************************************************************************/

typedef struct
{
	char               *name;
    struct _hash_table *keys;
} ConfigHeader;

typedef struct
{
	char         *path;
#ifdef WIN32
	HKEY          key_h;
#else
	FILE         *file;
#endif

	/* mtime of the file when opened, used to reload */
	time_t        mtime;

	struct _list *headers;
	ConfigHeader *confhdr; /* header "cursor" */
} Config;

/*****************************************************************************/

Config *config_new     (char *file);
void    config_free    (Config *conf);

void    config_set_str (Config *conf, char *keypath, char *value);
void    config_set_int (Config *conf, char *keypath, int value);
char   *config_get_str (Config *conf, char *keypath);
int     config_get_int (Config *conf, char *keypath);

/*****************************************************************************/

Config *gift_config_new  (char *module);
char   *gift_conf_path   (char *fmt, ...);
int     gift_create_path (char *);
char   *gift_expand_path (char *);
char   *gift_find_home   ();

/*****************************************************************************/

#endif /* __CONF_H */
