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

	if (!keypath)
		return NULL;

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

	if (!conf)
		return;

	file_exists (conf->path, NULL, &mtime);

	if (conf->mtime != mtime)
	{
		TRACE (("resynching %s", conf->path));

		conf->mtime = mtime;

		/* sync with disk */
		config_headers_read (conf);
	}
}

/*****************************************************************************/

static int cmp_config_header (ConfigHeader *a, char *b)
{
	if (!a || !a->name || !b)
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

	if (!conf || !keypath)
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

	if (!conf)
		return;

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

static void config_headers_clear (Config *conf)
{
	List *next;

	if (!conf)
		return;

	while (conf->headers)
	{
		ConfigHeader *confhdr = conf->headers->data;

		next = conf->headers->next;

		dataset_clear_free (confhdr->keys);
		free (confhdr->name);
		free (confhdr);

		/* free the actual list element */
		free (conf->headers);

		conf->headers = next;
	}
}

/*****************************************************************************/

/* determines whether or not this line is a comment.  blank whitelines are
 * also considered comments. */
static int line_comment (char *line)
{
	char *ptr;

	if (!line)
		return TRUE;

	/* hack comments off */
	if ((ptr = strchr (line, '#')))
		*ptr = 0;

	/* remove all trailing/leading whitespace */
	trim_whitespace (line);

	/* check if it's empty...after all this both a comment and a blank line
	 * will be considered exactly the same */
	if (str_isempty (line))
		return TRUE;

	return FALSE;
}

/*****************************************************************************/

/* synchs the configuration structure with the keys on disk.  This function
 * preserves all original formatting. */
void config_write (Config *conf)
{
	char *line = NULL;
	char *out_line, *out_line0;
	char *curr_hdr = NULL;
	char  tmp_file[PATH_MAX];
	FILE *tmp_f;

	if (!conf)
		return;

	/* make sure we have the conf file open */
	if (!conf->file)
	{
		if (!(conf->file = fopen (conf->path, "r")))
		{
			perror ("fopen");
			return;
		}
	}

	/* create a temporary file for writing out the new conf file */
	snprintf (tmp_file, sizeof (tmp_file) - 1, "%s.tmp", conf->path);

	if (!(tmp_f = fopen (tmp_file, "w")))
	{
		perror ("fopen");
		fclose (conf->file);
		conf->file = NULL;
		return;
	}

	while (file_read_line (conf->file, &line))
	{
		/* duplicate this current line so that we may mangle it and still
		 * preserve the original for writing */
		out_line0 = out_line = STRDUP (line);

		/* attempt to identify one of three types that this line could be and
		 * parse it
		 * NOTE: the condition below is funky.  If there is an equal operator
		 * inside the comment it will move to evaluation of the key lookup
		 * code, which can check for "#key = ..." and uncomment it.  If
		 * that exact condition is not found, it will simply write the
		 * comment as normal */
		if (!strchr (out_line, '=') && line_comment (out_line))
		{
			out_line = NULL;
		}
		else if (*out_line == '[')
		{
			char *ptr;

			/* track which current header we are one */
			if ((ptr = strchr (out_line + 1, ']')))
			{
				*ptr = 0;

				free (curr_hdr);
				curr_hdr = STRDUP (out_line + 1);
			}

			out_line = NULL;
		}
		else
		{
			char *key_lookup;
			char *key;
			char *value;


			key = string_sep (&out_line, "=");

			trim_whitespace (key);

			/* check for the special condition "#key = ..." */
			if (*key == '#')
			{
				/* check if the key is sane.  if not, there's pretty much
				 * no way in hell this is going to succeed a key lookup, and
				 * will this be written as a comment */
				if (!strchr (key, ' '))
					key++;
			}

			if (!curr_hdr || !key || !(*key))
			{
				TRACE (("removing garbage"));
				free (out_line0);
				continue;
			}

			/* it must be a key, look for it */
			key_lookup = malloc (strlen (curr_hdr) + strlen (key) + 2);
			sprintf (key_lookup, "%s/%s", curr_hdr, key);

			if (!(value = config_get_str (conf, key_lookup)))
				out_line = NULL;
			else
			{
				/* write the new output */
				out_line =
					malloc (strlen (key) + strlen (value) + 12);
				sprintf (out_line, "%s = %s\n", key, value);

				/* destroy the old duplication and reset it to this one */
				free (out_line0);
				out_line0 = out_line;
			}

			free (key_lookup);
		}

		/* if out line is provided, we wanted to change something...otherwise
		 * we wanna use the original */
		fprintf (tmp_f, "%s", (out_line ? out_line : line));
		free (out_line0);
	}

	fclose (tmp_f);
	fclose (conf->file);
	conf->file = NULL;

	/* move the new conf file to the updated */
	file_mv (tmp_file, conf->path);
}

/*****************************************************************************/

static void config_keys_read (Config *conf)
{
	char  *line = NULL;
	size_t line_len = 0;
	char  *key;
	char  *value;

	if (!conf)
		return;

	while (file_read_line (conf->file, &line))
	{
		/* figure out how much we read so that it may be rewinded if
		 * needed */
		line_len = strlen (line);

		if (line_comment (line))
			continue;

		if (*line == '[')
		{
			/* we just started reading the next header, rewind the file and
			 * return to the header reading function
			 * TODO -- this is quite possibly a bad idea */
			if (fseek (conf->file, -((off_t) line_len + 1), SEEK_CUR) == -1)
			{
				perror ("fseek");
				return;
			}

			return;
		}

		key = line;

		if (!(value = strchr (key, '=')))
			continue;

		*value++ = 0;

		trim_whitespace (key);
		trim_whitespace (value);

		dataset_insert (conf->confhdr->keys, key, strdup (value));
	}
}

static void config_headers_read (Config *conf)
{
	char *line = NULL;

	if (!conf)
		return;

	config_headers_clear (conf);

	if (!(conf->file = fopen (conf->path, "r")))
		return;

	while (file_read_line (conf->file, &line))
	{
		char *ptr;

		/* ignore blank lines */
		if (line_comment (line))
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
}

/*****************************************************************************/

Config *config_new (char *file)
{
	Config     *conf;

	time_t      mtime = 0;

	if (!file_exists (file, NULL, &mtime))
		return NULL;

	conf = malloc (sizeof (Config));
	memset (conf, 0, sizeof (Config));

	conf->path  = strdup (file);
	conf->mtime = mtime;

	config_headers_read (conf);

	return conf;
}

void config_free (Config *conf)
{
	if (!conf)
		return;

	free (conf->path);

	if (conf->file)
		fclose (conf->file);

	config_headers_clear (conf);

	free (conf);
}

/*****************************************************************************/

Config *gift_config_new (char *module)
{
	char *file;

	if (!module)
		return NULL;

	if (!strcmp (module, "giFT"))
		file = gift_conf_path ("gift.conf");
	else
		file = gift_conf_path ("%s/%s.conf", module, module);

	return config_new (file);
}

/*****************************************************************************/

char *gift_conf_path(char *fmt, ...)
{
	static char retval[PATH_MAX];
	static char tmp[PATH_MAX];
	va_list     args;

	va_start (args, fmt);
	vsnprintf (tmp, sizeof (retval) - 1, fmt, args);
	va_end (args);

	/* prepend the local directory */
	snprintf (retval, sizeof (retval) - 1, "%s/%s",
	          platform_local_dir (), tmp);

	/* make sure the path exists */
	file_create_path (retval);

	return retval;
}

/*****************************************************************************/

#if 0
int main ()
{
	Config *conf;

	conf = config_new ("/home/jasta/.giFT/gift.conf");

	printf ("set root...\n");
	config_set_str (conf, "sharing/root", "/data/temp_mp3s:/data/.porn");

	printf ("get root...%s\n",
			config_get_str (conf, "sharing/root"));

	printf ("write...\n");
	config_write (conf);

	config_free (conf);

	return 0;
}
#endif
