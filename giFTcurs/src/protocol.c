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
 * $Id: protocol.c,v 1.47 2003/06/27 11:20:14 weinholt Exp $
 */
#include "giftcurs.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "protocol.h"

/* The tree structure used by the protocol is defined here. It is
 * stored backwards, with the last items first in the linked list.
 * (and Yes, it's only to confuse you :)
 */
struct _ntree {
	const char *name;
	const char *value;
	ntree *next;
	ntree *subtree;
	unsigned is_integer:1;
};

static char *get_unescaped_until(char **packet, char limiter)
{
	char *str, *ptr, *txt;

	for (txt = ptr = str = *packet; *str != limiter && *str; str++) {
		if (*str == '\\')
			if (!*++str)
				break;
		if ((unsigned char) *str >= ' ')
			*ptr++ = *str;
	}

	*packet = str + 1;

	*ptr = '\0';
	return txt;
}

static char get_string(char **packet)
{
	char *str, *txt, ch;

	txt = str = *packet;
	while (isalnum(*str) || *str == '_')
		str++;
	*packet = str;
	ch = *str;
	*str = '\0';
	return ch;
}

static ntree *parse(char **packet)
{
	ntree *key = NULL;
	char ch = **packet;

	while (1) {
		switch (ch) {
		case ' ':
		case '\t':
		case '\n':
		case '\v':
		case '\r':
			(*packet)++;
			break;
			/* Do not fret, for it is merely a wonderful gcc extension. */
		case 'A'...'Z':
		case 'a'...'z':
		case '0'...'9':
		case '_':
			interface_append(&key, *packet, NULL);
			ch = get_string(packet);
			/* ch contains the next char and **packet is overwritten with \0 */
			continue;
		case '(':
			if (!key)
				goto error;
			if (key->value)
				goto error;
			(*packet)++;
			key->value = get_unescaped_until(packet, ')');
			break;
#if 0
		case '[':
			if (!key)
				goto error;
			(*packet)++;
			get_unescaped_until(packet, ']');
			break;
#endif
		case '{':
			if (!key)
				goto error;
			(*packet)++;
			key->subtree = parse(packet);
			break;
		default:
		  error:
			DEBUG("Parse error at char %c, followed by %-30.30s", ch, *packet + 1);
		case '}':
		case ';':
			(*packet)++;
		case '\0':
			return key;
		}
		ch = **packet;
	}
}

ntree *interface_parse(char *packet)
{
	ntree *result = parse(&packet);

	if (result && result->next) {
		/* Relink so that for example SHARE[0] action(sync);
		 * becomes                    SHARE[0] { action(sync) } ;
		 * which is more tree-like */
		/* sync action->SHARE */
		/* SHARE->{ sync action } */
		ntree *last, *second_last = NULL;

		for (last = result; last->next; last = last->next)
			second_last = last;
		g_assert(second_last && second_last->next == last);

		last->subtree = result;
		second_last->next = NULL;

		result = last;
	}

	return result;
}

void interface_free(ntree * key)
{
	if (!key)
		return;

	interface_free(key->subtree);
	interface_free(key->next);
	g_free(key);
}

static void escape(GString * s, const char *src)
{
	for (;;) {
		switch (*src) {
		case '\0':
			return;
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '\\':
		case ';':
			g_string_append_c(s, '\\');
		default:
			g_string_append_c(s, *src++);
		}
	}
}

static void build(ntree * key, GString * s)
{
	if (!key)
		return;

	if (key->next)
		build(key->next, s);

	escape(s, key->name);

	if (key->is_integer) {
		g_string_append_printf(s, "(%u)", (unsigned int) key->value);
	} else if (key->value) {
		g_string_append_c(s, '(');
		escape(s, key->value);
		g_string_append_c(s, ')');
	}

	if (key->subtree) {
		g_string_append_c(s, '{');
		build(key->subtree, s);
		g_string_append_c(s, '}');
	}
	g_string_append_c(s, ' ');
}

char *interface_construct(ntree * tree)
{
	GString s = { 0 };

	build(tree, &s);

	/* rewind the trailing space for cleanliness */
	/* also, giFT 020825 seems to _require_ no space before ; */
	s.str[s.len - 1] = ';';

	return s.str;
}

const char *interface_lookup(ntree * tree, const char *key)
{
	g_assert(key);

	/* search for the appropriate node */
	tree = tree->subtree;
	while (tree) {
		if (!strcmp(tree->name, key))
			return tree->value;
		tree = tree->next;
	}
	return NULL;
}

const char *interface_name(ntree * tree)
{
	return tree->name;
}

const char *interface_value(ntree * tree)
{
	return tree->value;
}

int interface_isempty(ntree * tree)
{
	return !(tree && tree->subtree);
}

void interface_append(ntree ** packet, const char *key_name, const char *value)
{
	ntree *key;

	g_assert(packet);

	key = g_new0(ntree, 1);
	key->name = key_name;
	key->next = *packet;
	key->value = value;

	*packet = key;
}

void interface_append_int(ntree ** packet, const char *key_name, unsigned int value)
{
	interface_append(packet, key_name, (const char *) value);
	(*packet)->is_integer = 1;
}

void interface_foreach(ntree * key, PForEachFunc func, void *udata)
{
	for (key = key->subtree; key; key = key->next)
		if (key->subtree)
			func(key, udata);
}

void interface_foreach_key(ntree * key, PForEachFunc func, void *udata)
{
	for (key = key->subtree; key; key = key->next)
		func(key, udata);
}
