/*
 * conf.c
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

#ifdef WIN32
#include <windows.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <ctype.h>

#include "gift.h"

#include "conf.h"
#include "file.h"
#include "parse.h"

/*****************************************************************************/

static void config_headers_read (Config *conf);

/*****************************************************************************/
/* helpers */

static char *config_parse_keypath (char *keypath, char **header_out,
								   char **key_out)
{
	char *header;
	char *key;
	char *value;
	char *defvalue;

	if ((defvalue = strchr (keypath, '=')))
		defvalue++;

	header = strdup (keypath);
	if (!(key = strchr (header, '/')))
	{
		free (header);
		return NULL;
	}

	*key++ = 0;

	if ((value = strchr (key, '=')))
		*value++ = 0;

	if (header_out)
		*header_out = header;

	if (key_out)
		*key_out = key;

	return defvalue;
}

/*****************************************************************************/

static ConfigHeader *config_header_new (char *line)
{
	ConfigHeader *confhdr;

	confhdr = malloc (sizeof (ConfigHeader));
	confhdr->name = strdup (line);
	confhdr->keys = NULL;

	return confhdr;
}

/*****************************************************************************/

/* if the file on disk changes, reload it */
static void config_update (Config *conf)
{
	time_t mtime = 0;

	file_exists (conf->path, NULL, &mtime);

	if (conf->mtime != mtime)
	{
		TRACE (("conf.c: resynching %s", conf->path));

		conf->mtime = mtime;

		/* sync with disk */
		config_headers_read (conf);
	}
}

/*****************************************************************************/

static int cmp_config_header (ConfigHeader *a, char *b)
{
	if (!a->name || !b)
		return -1;

	return strcmp (a->name, b);
}

static char *config_lookup (Config *conf, ConfigHeader **confhdr_out,
                            char *header, char *key)
{
	ConfigHeader *confhdr;
	List         *lheader;

	if (!conf)
		return NULL;

	config_update (conf);

	if (confhdr_out)
		*confhdr_out = NULL;

	lheader = list_find_custom (conf->headers, header,
	                            (CompareFunc) cmp_config_header);

	if (!lheader)
		return NULL;

	confhdr = lheader->data;

	if (confhdr_out)
		*confhdr_out = confhdr;

	return dataset_lookup (confhdr->keys, key);
}

/*****************************************************************************/
/* data insertion methods */

void config_set_str (Config *conf, char *keypath, char *value)
{
	char *header, *key, *defvalue;
	ConfigHeader *confhdr;

	if (!conf)
		return;

	defvalue = config_parse_keypath (keypath, &header, &key);

	/* find the header */
	config_lookup (conf, &confhdr, header, key);

	if (!confhdr)
	{
		conf->confhdr = config_header_new (header);
		conf->headers = list_append (conf->headers, conf->confhdr);

		confhdr = conf->confhdr;
	}

	dataset_insert (confhdr->keys, key, strdup (value));

	free (header);
}

void config_set_int (Config *conf, char *keypath, int value)
{
	char str[20];

	sprintf (str, "%i", value);

	config_set_str (conf, keypath, str);
}

/*****************************************************************************/
/* data retrieval methods */

char *config_get_str (Config *conf, char *keypath)
{
	char *header, *key, *defvalue;
	char *value;

	defvalue = config_parse_keypath (keypath, &header, &key);

	/* call the private locater function */
	value = config_lookup (conf, NULL, header, key);

	if (!value && defvalue)
	{
		value = STRDUP (defvalue);
		config_set_str (conf, keypath, value);
	}

	free (header);

	return value;
}

int config_get_int (Config *conf, char *keypath)
{
	char *value;

	value = config_get_str (conf, keypath);

	if (!value)
		return 0;

	return atoi (value);
}

/*****************************************************************************/

static int free_key (unsigned long key, void *value, void *data)
{
	free (value);

	return 1;
}

static void config_headers_clear (Config *conf)
{
	List *next;

	if (!conf)
		return;

	while (conf->headers)
	{
		ConfigHeader *confhdr = conf->headers->data;

		next = conf->headers->next;

		hash_table_foreach_remove (confhdr->keys, (HashFunc) free_key, NULL);
		dataset_clear (confhdr->keys);
		free (confhdr->name);
		free (confhdr);

		/* free the actual list element */
		free (conf->headers);

		conf->headers = next;
	}
}

/*****************************************************************************/

static void config_keys_read (Config *conf)
{
#ifdef WIN32
	int i = 0;
	char name[4096], value[4096];
	DWORD name_len, value_len, type;
	HKEY key_h;
#else /* !WIN32 */
	char *line = NULL;
	char *key;
	char *value;
#endif /* WIN32 */

#ifdef WIN32
	if (RegOpenKeyEx (conf->key_h, conf->confhdr->name, 0, KEY_READ,
			  &key_h) != ERROR_SUCCESS)
		return;

	name_len = sizeof (name);
	value_len = sizeof (value);

	while (RegEnumValue (key_h, i, name, &name_len, NULL, &type,
			     value, &value_len) == ERROR_SUCCESS) {
		if (type == REG_DWORD)
			sprintf (value, "%ld", *((DWORD *) value));
		
		dataset_insert (conf->confhdr->keys, name, strdup (value));
		
		name_len = sizeof (name);
		value_len = sizeof (value);
		i++;
	}

	RegCloseKey (key_h);
#else /* !WIN32 */
	while (file_read_line (conf->file, &line))
	{
		if (str_isempty (line) || *line == '[')
			return;

		key = line;
		if (!(value = strchr (key, '=')))
			continue;

		*value++ = 0;

		trim_whitespace (key);
		trim_whitespace (value);

		dataset_insert (conf->confhdr->keys, key, strdup (value));
	}
#endif /* WIN32 */
}

static void config_headers_read (Config *conf)
{
#ifdef WIN32
	int i = 0;
	char name[4096];
	DWORD name_len;
#else
	char *line = NULL;
#endif

	config_headers_clear (conf);

#ifdef WIN32
	if (RegOpenKeyEx (GIFT_REG_ROOT, conf->path, 0,
			  KEY_READ, &conf->key_h) != ERROR_SUCCESS)
		return;

	i = 0;
	name_len = sizeof (name);
	
	while (RegEnumKeyEx (conf->key_h, i, name, &name_len,
			     NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
		conf->confhdr = config_header_new (name);
		conf->headers = list_append (conf->headers, conf->confhdr);

		config_keys_read (conf);

		name_len = sizeof (name);
		i++;
	}

	RegCloseKey (conf->key_h);
	conf->key_h = NULL;
#else /* !WIN32 */
	if (!(conf->file = fopen (conf->path, "r")))
		return;

	while (file_read_line (conf->file, &line))
	{
		char *ptr;

		/* ignore blank lines */
		if (str_isempty (line) || *line == '#')
			continue;

		if (*line != '[')
			continue;

		ptr = strchr (line, ']');

		if (!ptr)
			continue;

		*ptr = 0;

		conf->confhdr = config_header_new (line + 1);
		conf->headers = list_append (conf->headers, conf->confhdr);

		config_keys_read (conf);
	}

	fclose (conf->file);
	conf->file = NULL;
#endif /* WIN32 */
}

Config *config_new (char *file)
{
	Config     *conf;
#ifdef WIN32
	HKEY key_h;
#else
	time_t      mtime = 0;
#endif

#ifdef WIN32
	if (RegOpenKeyEx (GIFT_REG_ROOT, file, 0, KEY_READ,
			  &key_h) != ERROR_SUCCESS)
		return NULL;
	
	RegCloseKey (key_h);
#else
	if (!file_exists (file, NULL, &mtime))
		return NULL;
#endif

	conf = malloc (sizeof (Config));
	memset (conf, 0, sizeof (Config));

	conf->path  = strdup (file);
#ifndef WIN32
	conf->mtime = mtime;
#endif

	config_headers_read (conf);

	return conf;
}

void config_free (Config *conf)
{
	free (conf->path);

#ifndef WIN32
	if (conf->file)
		fclose (conf->file);
#endif

	config_headers_clear (conf);

	free (conf);
}

/*****************************************************************************/

Config *gift_config_new (char *module)
{
#ifdef WIN32
	char reg_path[4096];
#else
	char *file;
#endif

	if (!module)
		return NULL;
	
#ifdef WIN32
	snprintf (reg_path, sizeof (reg_path) - 1, "%s\\%s", GIFT_REG_PATH,
		  module);
	return config_new (reg_path);
#else /* !WIN32 */
	if (!strcmp (module, "giFT"))
		file = gift_conf_path ("gift.conf");
	else
		file = gift_conf_path ("%s/%s.conf", module, module);

	return config_new (file);
#endif /* WIN32 */
}

/*****************************************************************************/

char *gift_conf_path(char *fmt, ...)
{
	static char retval[4096], tmp[4096];
	char *expanded;
	va_list args;

	va_start(args, fmt);
	vsnprintf (tmp, sizeof (retval) - 1, fmt, args);
	va_end(args);

#ifdef WIN32
	snprintf (retval, sizeof (retval) - 1, "~/%s", tmp);
#else
	snprintf (retval, sizeof (retval) - 1, "~/%s/%s", GIFT_CONFIG_PATH, tmp);
#endif
	expanded = gift_expand_path (retval);

	gift_create_path (expanded);

	return expanded;
}

char *gift_find_home ()
{
#ifdef WIN32
	return win32_gift_dir ();
#else
	static char ph[4096];
	char *h;

	h = getenv ("HOME");
	if (h)
		sprintf (ph, "%s", h);
	else
	{
		h = getenv ("USER");

		if (h)
			sprintf (ph, "/home/%s", h);
		else
			sprintf (ph, ".");
	}

	return ph;
#endif /* WIN32 */
}

char *gift_expand_path (char *path)
{
	static char newp[4096];
	char *p1;

	path = (char *) strdup (path);

	/* TODO - convert to while loop */
	if ((p1 = strchr (path, '~')) != NULL)
	{
		char *p0 = path;

		*p1++ = 0;

		if (strlen(p0))
			sprintf (newp, "%s%s%s", p0, gift_find_home(), p1);
		else
			sprintf (newp, "%s%s", gift_find_home(), p1);
	}
	else
		strcpy (newp, path);	// possible buffer overflow

	free (path);

	return newp;
}

int gift_create_path (char *p)
{
	// create all subdirectories needed to open p
	char *p0, *p1;
	int ret = 0;

	p = (char *) strdup (p);

	p0 = p1 = p;

	while (*p1 == '/')
		p1++;

	while ((p1 = strchr (p1, '/')) != NULL)
	{
		*p1 = 0;

		/* Really should stat() here,
		 * but if the directory already exists,
		 * mkdir() will just fail anyway
		 */
#ifdef WIN32
		mkdir (p0);
#else
		mkdir (p0, 0755);
#endif

		*p1 = '/';
		while (*p1 == '/')
			p1++;
	}

	free (p);

	return ret;
}
