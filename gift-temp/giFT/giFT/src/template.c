/*
 * $Id: template.c,v 1.1 2003/03/28 07:32:43 jasta Exp $
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

#include "gift.h"

#include "file.h"

#include "template.h"

#include <ctype.h>

/*****************************************************************************/

Template *template_new (char *file)
{
	Template *tpl;

	if (!(tpl = MALLOC (sizeof (Template))))
		return NULL;

	tpl->file = STRDUP (file);

	return tpl;
}

void template_free (Template *tpl)
{
	if (!tpl)
		return;

	free (tpl->file);
	free (tpl->buf);
	free (tpl);
}

/*****************************************************************************/

void template_data (Template *tpl, char *data)
{
	if (!tpl)
		return;

	/* set the data buffer to use instead of trying to read it from a file */
	assert (tpl->file == NULL);
	tpl->buf = STRDUP (data);
}

/*****************************************************************************/

void template_set (Template *tpl, char *key, char *data)
{
	if (!tpl || !key)
		return;

	if (!tpl->data)
		tpl->data = dataset_new (DATASET_HASH);

	dataset_insertstr (&tpl->data, key, data);
}

char *template_get (Template *tpl, char *key)
{
	if (!tpl || !key)
		return NULL;

	return dataset_lookupstr (tpl->data, key);
}

/*****************************************************************************/

static char *find_subst (char **line, char **pre)
{
	char *begin;
	char *end;

	/* EOL */
	if (!(*line))
		return NULL;

	/* parse this block */
	if (!(begin = strstr (*line, "<(")))
		return NULL;

	*begin = 0;
	begin += 2;

	if (!(end = strstr (begin, ")>")))
		return NULL;

	*end = 0;
	end += 2;

	/* pass back to the caller */
	if (pre)
		*pre = *line;

	*line = end;

	return begin;
}

static void handle_line (Template *tpl, char *line, String *buf)
{
	char *key;
	char *leftover;

	while ((key = find_subst (&line, &leftover)))
	{
		string_append (buf, leftover);
		string_append (buf, template_get (tpl, key));
	}

	string_append (buf, line);
}

static void parse_file (Template *tpl, FILE *f, String *buf)
{
	char *line = NULL;

	while (file_read_line (f, &line))
		handle_line (tpl, line, buf);
}

String *template_exec (Template *tpl)
{
	FILE   *f = NULL;
	String *buf;

	if (!tpl)
		return NULL;

	/* make sure we have at least one source of input */
	assert (tpl->file != NULL || tpl->buf != NULL);

	if (tpl->file)
	{
		if (!(f = fopen (tpl->file, "r")))
			return NULL;
	}

	if ((buf = string_new (NULL, 0, 0, TRUE)))
	{
		if (f)
			parse_file (tpl, f, buf);
		else
			handle_line (tpl, tpl->buf, buf);
	}

	if (f)
		fclose (f);

	return buf;
}

/*****************************************************************************/

#if 0
static char *test_tpl =
	"<html>\n"
	" <head>\n"
	"  <title><(foo)></title>\n"
	" </head>\n"
	" <body>\n"
	"  <h1><(foo)></h1><p><(foo)></p>\n"
	" </body>\n"
	"</html>\n";

int main (int argc, char **argv)
{
	Template *tpl;
	String   *buf;

	tpl = template_new (NULL);
	assert (tpl != NULL);

	template_data (tpl, test_tpl);

	template_set (tpl, "foo", "bar");

	buf = template_exec (tpl);
	assert (buf != NULL);

	printf ("%s\n", buf->str);
	string_free (buf);

	template_free (tpl);

	return 0;
}
#endif
