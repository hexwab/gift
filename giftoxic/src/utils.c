/* giFToxic, a GTK2 based GUI for giFT
 * Copyright (C) 2002, 2003 giFToxic team (see AUTHORS)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 */

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "config.h"
#include "common.h"
#include "main.h"

/* compares two unicode strings. return value is the same as in strcmp() */
gint strcmp_utf8(gchar *one, gchar *two)
{
	gchar *a;
	gchar *b;
	gint result;

	a = g_utf8_normalize(one, -1, G_NORMALIZE_ALL);
	b = g_utf8_normalize(two, -1, G_NORMALIZE_ALL);

	result = strcmp(a, b);
	g_free(a);
	g_free(b);

	return result;
}

/* see above */
gint strcasecmp_utf8(gchar *one, gchar *two)
{
	gchar *a;
	gchar *b;
	gint result;

	a = g_utf8_normalize(one, -1, G_NORMALIZE_ALL);
	b = g_utf8_normalize(two, -1, G_NORMALIZE_ALL);

	result = strcasecmp(a, b);
	g_free(a);
	g_free(b);

	return result;
}

/* this is essentially g_strconcat from glib, but string1 is freed */
gchar *strconcat(gchar *string1, ...)
{
	gsize l;
	va_list args;
	gchar *s;
	gchar *concat;
	gchar *ptr;

	if (!string1)
		return NULL;

	l = 1 + strlen(string1);
	va_start(args, string1);
	s = va_arg(args, gchar *);

	while (s) {
		l += strlen(s);
		s = va_arg(args, gchar *);
	}

	va_end(args);

	concat = g_new(gchar, l);
	ptr = concat;

	ptr = g_stpcpy(ptr, string1);
	va_start(args, string1);
	s = va_arg(args, gchar *);

	while (s) {
		ptr = g_stpcpy(ptr, s);
		s = va_arg(args, gchar *);
	}

	va_end(args);

	g_free(string1);

	return concat;
}

void free_searchresult(gpointer data, gpointer user_data)
{
	SearchResult *s = (SearchResult *) data;
	
	g_free(s->user);
	g_free(s->node);
	g_free(s->url);
	g_free(s->filename);
	g_free(s->hash);
	g_free(s->mime);

	g_free(s);

	return;
}


void free_transfer(Transfer *t)
{
	g_free(t->filename);
	g_free(t->user);
	g_free(t->hash);

	g_free(t);

	return;
}

void free_chunk(Chunk *c)
{
	g_free(c->user);
	g_free(c->url);
	g_free(c->status);

	g_free(c);

	return;
}

void escape(gchar **input)
{
	gchar new[3];
	gchar *result = NULL;
	gchar *ptr;

	for (ptr = *input; *ptr; ptr++) {
		switch (*ptr) {
			case '(':
			case ')':
			case '{':
			case '}':
			case '[':
			case ']':
			case ';':
			case '\\':
				new[0] = '\\';
				new[1] = *ptr;
				new[2] = 0;
				break;
			default:
				strncpy(new, ptr, 1);
				new[1] = 0;
				break;
		}

		if (result)
			result = strconcat(result, new, NULL);
		else
			result = g_strdup(new);
	}

	g_free(*input);
	*input = g_strdup(result);
	g_free(result);

	return;
}

void unescape(gchar **input)
{
	gchar new[2];
	gchar *result = NULL;
	gchar *ptr;

	for (ptr = *input; *ptr; ptr++)
		if (*ptr != '\\') {
			strncpy(new, ptr, 1);
			new[1] = 0;

			if (result)
				result = strconcat(result, new, NULL);
			else
				result = g_strdup(new);
		}

	g_free(*input);
	*input = g_strdup(result);
	g_free(result);

	return;
}

/* like strstr(), but here delimiters consist of single characters.
 * returns a pointer to the first delimiter that was found.
 */
gchar *my_strstr(gchar *string, gchar *delimiters)
{
	gchar *ptr;
	gchar *ptr2;

	for (ptr = string; *ptr; ptr++)
		for (ptr2 = delimiters; *ptr2; ptr2++)
			if (*ptr == *ptr2)
				return ptr;

	return NULL;
}


/* parses an user's alias out of a alias@ip construct */
gchar *get_user_from_ident(gchar *str)
{
	gchar *ptr = strrchr(str, '@');

	return (ptr) ? g_strndup(str, ptr - str) : g_strdup(str);
}

gchar *get_dir_from_path(gchar *str)
{
	gchar *ptr = strrchr(str, '/');
	
	return (ptr) ? g_strndup(str, ptr - str) : g_strdup(str);
}

gchar *get_filename_from_path(gchar *str)
{
	gchar *ptr = strrchr(str, '/');

	return (ptr) ? g_strdup(ptr + 1) : g_strdup(str);
}

gint my_strtoul(gchar *c)
{
	gint result;

	result = strtoul(c, NULL, 16);

	if (result >= 32 && result <= 122)
		return result;
	else
		return 32;
}

/* reverses the URL escape signs (%20 etc) to their ASCII values */
void url_decode(gchar **input)
{
	gchar code[3];
	gchar *result = NULL;
	gchar *new = NULL;
	gchar *old = NULL;
	gchar *ptr;

	result = g_strdup(*input);
	g_free(*input);

	for (ptr = strstr(result, "%"); ptr; ptr = strstr(result, "%")) {
		strncpy(code, ptr + 1, 2);
		code[2] = 0;

		old = g_strndup(result, ptr - result);
		new = g_strdup_printf("%c%s", my_strtoul(code), ptr + 3);

		g_free(result);
		result = g_strconcat(old, new, NULL);

		g_free(old);
		g_free(new);
	}

	*input = g_strdup(result);
	g_free(result);

	return;
}

Options *read_settings()
{
	FILE *f;
	Options *options = g_new0(Options, 1);
	gchar *config;
	gchar buf[256];
	gchar item1[256];
	gchar item2[256];

	/* read giFT settings */
	config = g_build_filename(g_get_home_dir(), ".giFT", "ui", "ui.conf", NULL);

	if ((f = fopen(config, "r"))) {
		while (fgets(buf, sizeof(buf), f))
			if ((sscanf(buf, "%s = %[^\n]", item1, item2)) == 2) {
				if (!strcmp(item1, "host"))
					options->target_host = g_strdup(item2);

				if (!strcmp(item1, "port"))
					options->target_port = atoi(item2);
			}

		fclose(f);
	}

	g_free(config);

	/* read our own settings */
	config = g_build_filename(g_get_home_dir(), ".giFToxicrc", NULL);

	if ((f = fopen(config, "r"))) {
		while (fgets(buf, sizeof(buf), f))
			if ((sscanf(buf, "%[^=] = %[^\n]", item1, item2)) == 2) {
				if (!strcmp(g_strstrip(item1), "autoclean transfers")) {
					options->autoclean = atoi(item2);
				}
			}

		fclose(f);
	}

	g_free(config);

	return options;
}

void apply_settings(Options *options)
{
	set_autoclean_status(options->autoclean);
	
	return;
}

void save_settings(Options *options)
{
	FILE	*f;
	gchar	*config;

	config = g_build_filename(g_get_home_dir(), ".giFToxicrc", NULL);

	if ((f = fopen(config, "w"))) {
		fprintf(f, "autoclean transfers = %i\n", options->autoclean);
		fclose(f);
	}

	g_free(config);

	return;
}

gchar *number_human(gulong size)
{
	gchar format[100];
	gchar *qtys[3][2]={
		{" ", " "},
		{N_(" thousand"), N_(" thousand ")},
		{N_(" million"), N_(" million ")}};
		
	gulong divisor = 1;
	gint type = 0;
		
	gulong mod; 
	gulong nice; 

	if (size >= 1000000){
		divisor = 1000000;
		type = 2;
	} else if (size >= 1000){
		divisor = 1000;
		type = 1;
	}
	
	mod = size % divisor;
	nice = size / divisor;
	strcpy(format, (mod == 0)?"%.0f":"%.2f");
	strcat(format, (nice == 1 && mod == 0?gettext(qtys[type][0]):gettext(qtys[type][1])));
	return g_strdup_printf(format, (gfloat) size / divisor);
}

gchar *size_human(gulong size)
{
	if (size > (1024 * 1024 * 1024))
		return g_strdup_printf("%.1f GB", (gfloat) size / (1024 * 1024 * 1024));
	else if (size > (1024 * 1024))
		return g_strdup_printf("%.1f MB", (gfloat) size / (1024 * 1024));
	else
		return g_strdup_printf("%.1f KB", (gfloat) size / 1024);
}

gchar *time_human(gulong secs)
{
	gint	minutes;
	gint	hours;
	gint	days;
	
	minutes = secs / 60;
	hours = minutes / 60;
	days = hours / 24;
	
	if (!secs)
		return g_strdup("0s");
	else if (secs < 60)
		return g_strdup_printf("%lis", secs);
	else if (minutes < 60)
		return g_strdup_printf("%im %lis", minutes, secs % 60);
	else if (hours < 24)
		return g_strdup_printf("%ih %im", hours, minutes % 60);
	else
		return g_strdup_printf("%id %ih", days, hours % 24);
}

void usage()
{
	printf("Usage: %s [options]\n", g_get_prgname());
	printf("Options:\n"
		   "    -s, --server=host:port        specify host:ip giFToxic to connect to\n"
		   "                                  default is to use the information found in\n"
		   "                                  ~/.giFT/ui/ui.conf\n\n"
		   "                                  If that file can't be read and --server isn't\n"
		   "                                  used, giFToxic will try to spawn a giFT daemon\n"
		   "                                  locally.\n\n");

	exit(0);
}


void parse_parameters(gint argc, gchar **argv, Options **options)
{
	gint			c;
	gchar			*colon;
	struct option	long_opts[] =	{{"help", no_argument, 0, 'h'},
	                                 {"server", required_argument, 0, 's'},
	                                 {"version", no_argument, 0, 'v'},
	                                 {NULL, 0, NULL, 0}};

	while ((c = getopt_long(argc, argv, "hs:", long_opts, NULL)) != -1) {
		switch (c) {
			case 'h':
				usage();
				break;
			case 'v':
				printf("giFToxic %s\n", VERSION);
				exit(0);
				break;
			case 's':
				colon = strchr(optarg, ':');

				if (colon) {
					*colon = 0;
					(*options)->target_port = atoi(colon + 1);
				}

				if (*optarg) {
					g_free((*options)->target_host);
					(*options)->target_host = g_strdup(optarg);
				}
				
				break;
		}
	}

	return;
}

void show_dialog(const gchar *msg, GtkMessageType type)
{
	GtkWidget		*dialog;
	
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, msg);
	gtk_dialog_run(GTK_DIALOG(dialog));

	if (type == GTK_MESSAGE_ERROR)
		exit(1);

	gtk_widget_destroy(dialog);
 
	return;
}

gchar *locale_to_utf8(const gchar *src)
{
	const gchar	*charset;

	if (g_get_charset(&charset)) /* charset is UTF-8 so we don't need to convert src */
		return g_strdup(src);
	else
		return g_convert_with_fallback(src, strlen(src), "UTF-8", charset, " ", NULL, NULL, NULL);
}

gchar *utf8_to_locale(const gchar *src)
{
	const gchar	*charset;

	if (g_get_charset(&charset)) /* charset is UTF-8 so we don't need to convert src */
		return g_strdup(src);
	else
		return g_convert_with_fallback(src, strlen(src), charset, "UTF-8", " ", NULL, NULL, NULL);
}

gboolean is_local_host(const gchar *host)
{
	return (!strncmp(host, "127.", 4) || !strcasecmp(host, "localhost"));
}

