/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
 * $Id: settings.c,v 1.33 2003/05/14 09:09:35 chnix Exp $
 */
#include "giftcurs.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "parse.h"
#include "settings.h"
#include "list.h"
#include "screen.h"

typedef struct {
	char *section;
	char *name;
	char *value;

	unsigned int save:1;
} setting;

static int rename_config(const char *oldpath, const char *newpath);
static char *conffile_alloc(const char *file);
static int parse_configuration_line(char *line, char **section, char **name, char **value);
static setting *find_config(const char *section, const char *name);

static list settings = { 0 };

FILE *open_config(const char *file, const char *mode)
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

char *read_gift_config(const char *file, const char *section, const char *option)
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
	char line[256];
	FILE *f;

	if (!(f = open_config("giFTcurs.conf", "r")))
		return;

	while (fgets(line, sizeof line, f)) {
		char *section, *name, *value;

		if (parse_configuration_line(line, &section, &name, &value))
			set_config(section, name, value, 0);
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
		fputs(_("# Available colors:"), out);

		for (i = 0; i < G_N_ELEMENTS(colornames); i++)
			fprintf(out, " %s", colornames[i]);

		/* NOTE: do not translate 'default' here. */
		fputs(_("\n# 'default' means no color, i.e. transparent on some terminals.\n"), out);
		fputs(_("# See giFTcurs.conf(5) for more information.\n"), out);
		for (i = 0; i < settings.num; i++) {
			setting *fnord = list_index(&settings, i);

			fprintf(out, "%s %s %s\n", fnord->section, fnord->name, fnord->value);
		}
	} else {
		char line[256], copy[256];
		char *section, *name, *value;

		while (fgets(line, sizeof line, in)) {
			strncpy(copy, line, sizeof copy);

			if (parse_configuration_line(line, &section, &name, &value)) {
				/* Alter the line if it's needed */
				setting *fnord = find_config(section, name);

				if (fnord && fnord->save) {
					fnord->save = 0;
					if (strcmp(fnord->value, value)) {
						char *comment = strchr(copy, '#');

						fprintf(out, "%s %s %s", fnord->section, fnord->name, fnord->value);
						if (comment)
							fprintf(out, "\t%s", comment);
						else
							fputc('\n', out);
						continue;
					}
				}
			}
			fputs(copy, out);
		}
		fclose(in);

		/* Write out all new settings */
		for (i = 0; i < settings.num; i++) {
			setting *fnord = list_index(&settings, i);

			if (fnord->save) {
				fprintf(out, "%s %s %s\n", fnord->section, fnord->name, fnord->value);
				fnord->save = 0;
			}
		}
	}

	fclose(out);

	return rename_config("giFTcurs.conf.new", "giFTcurs.conf");
}

const char *get_config(const char *section, const char *name, const char *standard)
{
	setting *fnord = find_config(section, name);

	return fnord ? fnord->value : standard;
}

void set_config(const char *section, const char *name, const char *value, int save)
{
	setting *fnord = find_config(section, name);

	if (fnord) {
		if (!strcmp(value, fnord->value))
			return;
		g_free(fnord->value);
	} else {
		fnord = g_new(setting, 1);
		fnord->section = g_strdup(section);
		fnord->name = g_strdup(name);
		list_append(&settings, fnord);
	}
	fnord->value = g_strdup(value);
	fnord->save = save;
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
		g_free(fnord->value);
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
