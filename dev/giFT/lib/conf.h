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

/*****************************************************************************/

/**
 * @file conf.h
 *
 * @brief Configuration file routines
 *
 * Provides routines that can be used to manipulate INI-style configuration
 * files.  These routines properly preserve formatting and comments within
 * the original files.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

/**
 * Holds the name and all children of a conf file "[header]"
 */
typedef struct
{
	char    *name;                     /**< header name */
	Dataset *keys;                     /**< dataset holding key/value pairs */
} ConfigHeader;

/**
 * Basic config structure, you should avoid manipulating this structure
 * directly if possible
 */
typedef struct
{
	char         *path;                /**< fully qualified path to .conf */
	FILE         *file;                /**< open file pointer */
	time_t        mtime;               /**< last known modification time of
	                                    *   the file path, used to reread when
	                                    *   on disk changes are detected */
	List         *headers;             /**< list of ConfigHeader's */
	ConfigHeader *confhdr;             /**< header "cursor", do not touch */
} Config;

/*****************************************************************************/

/**
 * Loads a configuration file into memory
 *
 * @param file fully qualified path of the conf file
 *
 * @return Dynamically allocated Config pointer
 */
Config *config_new (char *file);

/**
 * Destroys the memory associated with a Config pointer
 *
 * @param conf Location of the Config structure
 */
void config_free (Config *conf);

/**
 * Attempts to synch the possibly formatted configuration file on disk with
 * the data loaded into memory
 *
 * @param conf Location of the Config structure
 */
void config_write (Config *conf);

/**
 * Sets a configuration key in memory using a string value
 *
 * @param conf    Location of the Config structure
 * @param keypath Key path in the form "header/key"
 * @param value   Value to assign to the key when found
 */
void config_set_str (Config *conf, char *keypath, char *value);

/**
 * Sets a configuration key in memory using a signed integer value
 *
 * @see config_set_str
 */
void config_set_int (Config *conf, char *keypath, int value);

/**
 * Gets a configuration key's value from memory as a string
 *
 * @note When reading data from the configuration structure the key path
 * format may optionally include append "=DEFAULT".  If the key was not found
 * to exist in the structure, "DEFAULT" will be returned as if it did.
 *
 * @param conf    Location of the Config structure
 * @param keypath Key path in the form "header/key"
 *
 * @return Configuration value
 *
 * @retval !NULL
 *   pointer to internally used memory, do not write to this pointer or God
 *   will kill an innocent puppy.
 * @retval NULL
 *   failure to lookup the requested key (and no default was provided)
 */
char *config_get_str (Config *conf, char *keypath);

/**
 * Gets a configuration key's value from memory converted to a signed integer
 *
 * @see config_get_str
 */
int config_get_int (Config *conf, char *keypath);

/*****************************************************************************/

/**
 * Extension specific to giFT that is used to load configuration modules
 * from the ~/.giFT/ directory path
 *
 * @note Requires \ref platform_init before usage
 *
 * @param module module name to load (one of giFT, OpenFT, or ui)
 *
 * @see config_new
 */
Config *gift_config_new (char *module);

/**
 * Completely unrelated extension that merely interprets the supplied
 * printf format and prepends ~/.giFT/
 *
 * @note Requires \ref platform_init before usage
 *
 * @param fmt printf format string
 * @param ... c'mon, you know
 *
 * @return pointer to local (static) memory describing the path
 */
char *gift_conf_path (char *fmt, ...);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __CONF_H */
