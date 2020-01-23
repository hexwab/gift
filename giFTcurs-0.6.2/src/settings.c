/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: settings.c,v 1.42 2003/09/08 17:52:06 saturn Exp $
 */
#include "giftcurs.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "parse.h"
#include "settings.h"
#include "list.h"

typedef struct {
	char *section;
	char *name;
	GSList *values;
	GSList *copy;				/* only used when saving the config file */

	unsigned int save:1;
} setting;

static int rename_config(const char *oldpath, const char *newpath);
static char *conffile_alloc(const char *file);
static int parse_configuration_line(char *line, char **section, char **name, char **value);
static setting *find_config(const char *section, const char *name);

static list settings = { 0 };

static FILE *open_config(const char *file, const char *mode)
{
	char *path = conffile_alloc(file);
	FILE *f = fopen(path, mode);

	if (!f) {
		if (mode[0] == 'w')
			g_message("%s: %s", path, g_strerror(errno));
		else
			DEBUG("%s: %s", path, g_strerror(errno));
	}
	g_free(path);
	return f;
}

char *read_gift_config(const char *file, const char *section G_GNUC_UNUSED, const char *option)
{
	/* Note. This ignores [section]. Hopefully giFT will never have
	 * two identically options in different sections.
	 */
	char *ret = NULL;
	char line[256];
	FILE *f = open_config(file, "r");

	if (!f)
		return NULL;

	while (fgets(line, sizeof line, f)) {
		char *foo, *equals, *value;

		if (!parse_configuration_line(line, &foo, &equals, &value))
			continue;

		if (!strcmp(foo, option)) {
			ret = g_strdup(value);
			break;
		}
	}
	fclose(f);

	return ret;
}

void load_configuration(void)
{
	char line[1024];
	FILE *f;

	if (!(f = open_config("giFTcurs.conf", "r")))
		return;

	while (fgets(line, sizeof line, f)) {
		char *section, *name, *value;

		if (parse_configuration_line(line, &section, &name, &value))
			set_config_multi(section, name, value, 0);
	}
	fclose(f);
}

static int rename_config(const char *oldpath, const char *newpath)
{
	char *path1, *path2;
	int status;

	path1 = conffile_alloc(oldpath);
	path2 = conffile_alloc(newpath);

	if ((status = rename(path1, path2)) < 0)
		g_message(_("Could not rename %s to %s: %s"), path1, path2, g_strerror(errno));

	g_free(path1);
	g_free(path2);
	return status;
}

int save_configuration(void)
{
	FILE *in, *out;
	int i;
	int changes_made = 0;

	for (i = 0; i < settings.num; i++) {
		setting *fnord = list_index(&settings, i);

		if (fnord->save)
			changes_made = 1;
	}
	if (!changes_made)
		return 0;

	if (!(out = open_config("giFTcurs.conf.new", "w")))
		return -1;

	if (!(in = open_config("giFTcurs.conf", "r"))) {
		fprintf(out, _("# %s configuration file created by v%s.\n"), PACKAGE, VERSION);

		/* NOTE: do not translate 'default' here. */
		fputs(_("\n# 'default' means no color, i.e. transparent on some terminals.\n"), out);
		fputs(_("# See giFTcurs.conf(5) for more information.\n"), out);
		for (i = 0; i < settings.num; i++) {
			setting *fnord = list_index(&settings, i);
			GSList *value;

			for (value = fnord->values; value; value = value->next)
				fprintf(out, "%s %s %s\n", fnord->section, fnord->name, (char *) value->data);
		}
	} else {
		char line[1024], copy[1024];
		char *section, *name, *value;
		GSList *item;

		/* Duplicate the list to keep track of which values we have written */
		for (i = 0; i < settings.num; i++) {
			setting *fnord = list_index(&settings, i);

			if (fnord->save)
				fnord->copy = g_slist_copy(fnord->values);
		}

		while (fgets(line, sizeof line, in)) {
			setting *fnord = NULL;

			g_strlcpy(copy, line, sizeof copy);

			if (!parse_configuration_line(line, &section, &name, &value)) {
				fputs(copy, out);
				continue;
			}
			/* Alter the line if it's needed */
			fnord = find_config(section, name);

			if (!fnord || !fnord->save) {
				fputs(copy, out);
				continue;
			}

			item = g_slist_find_custom(fnord->copy, value, (GCompareFunc) strcmp);
			if (item) {
				/* found in list and not modified */
				fnord->copy = g_slist_remove_link(fnord->copy, item);
				g_slist_free_1(item);
				fputs(copy, out);
			} else if (fnord->copy) {
				/* either a duplicate entry or a changed value */
				/* substitute with the first entry in list */
				char *comment = strchr(copy, '#');

				item = fnord->copy;
				fnord->copy = g_slist_remove_link(item, item);
				fprintf(out, "%s %s %s", fnord->section, fnord->name, (char *) item->data);
				if (comment)
					fprintf(out, "\t%s", comment);
				else
					fputc('\n', out);
				g_slist_free_1(item);
			} else {
				/* this setting was removed... just comment the line */
				fprintf(out, "#<deleted>#%s", copy);
			}
		}
		fclose(in);

		/* Write out all new settings */
		for (i = 0; i < settings.num; i++) {
			setting *fnord = list_index(&settings, i);

			if (!fnord->save)
				continue;
			for (item = fnord->copy; item; item = item->next)
				fprintf(out, "%s %s %s\n", fnord->section, fnord->name, (char *) item->data);
			fnord->save = 0;
			g_slist_free(fnord->copy);
			fnord->copy = NULL;
		}
	}

	fclose(out);

	return rename_config("giFTcurs.conf.new", "giFTcurs.conf");
}

const char *get_config(const char *section, const char *name, const char *standard)
{
	setting *fnord = find_config(section, name);

	return fnord ? fnord->values->data : standard;
}

GSList *get_config_multi(const char *section, const char *name)
{
	setting *fnord = find_config(section, name);

	return fnord ? fnord->values : NULL;
}

static void set_config_common(const char *section, const char *name, const char *value, int save,
							  gboolean multi)
{
	setting *fnord = find_config(section, name);

	if (!fnord) {
		fnord = g_new(setting, 1);
		fnord->section = g_strdup(section);
		fnord->name = g_strdup(name);
		fnord->values = NULL;
		list_append(&settings, fnord);
	} else if (multi) {
		if (g_slist_find_custom(fnord->values, value, (GCompareFunc) strcmp))
			return;
	} else {
		if (!strcmp(value, fnord->values->data))
			return;
		/* destroy all old entries */
		g_slist_foreach(fnord->values, (GFunc) g_free, NULL);
		g_slist_free(fnord->values);
		fnord->values = NULL;
	}
	fnord->values = g_slist_prepend(fnord->values, g_strdup(value));
	fnord->save = save;
}

void set_config_multi(const char *section, const char *name, const char *value, int save)
{
	set_config_common(section, name, value, save, TRUE);
}

void set_config(const char *section, const char *name, const char *value, int save)
{
	set_config_common(section, name, value, save, FALSE);
}

void set_config_int(const char *section, const char *name, int value, int save)
{
	set_config(section, name, itoa(value), save);
}

static char *conffile_alloc(const char *file)
{
	const char *home = g_get_home_dir();

	return g_strdup_printf("%s/.giFT/ui/%s", home ? home : "", file);
}

static setting *find_config(const char *section, const char *name)
{
	int i;

	for (i = 0; i < settings.num; i++) {
		setting *fnord = list_index(&settings, i);

		if (!strcmp(fnord->section, section) && !strcmp(fnord->name, name))
			return fnord;
	}
	return NULL;
}

void config_cleanup(void)
{
	int i;

	for (i = 0; i < settings.num; i++) {
		setting *fnord = list_index(&settings, i);

		g_free(fnord->section);
		g_free(fnord->name);
		g_slist_foreach(fnord->values, (GFunc) g_free, NULL);
		g_slist_free(fnord->values);
	}
	list_free_entries(&settings);
}

static int parse_configuration_line(char *line, char **section, char **name, char **value)
{
	char *p;

	if ((p = strchr(line, '#')))
		*p = '\0';

	/* Make sure there aren't any tabs or any other junk in here. */
	trim(line);

	if (!(*section = strtok(line, " ")))
		return 0;
	if (!(*name = strtok(NULL, " ")))
		return 0;
	if (!(*value = strtok(NULL, "")))
		return 0;
	return 1;
}
