/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
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
 * $Id: protocol.c,v 1.29 2002/11/22 20:48:32 weinholt Exp $
 */
#include "giftcurs.h"

#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "parse.h"
#include "protocol.h"
#include "screen.h"

/* The tree structure used by the protocol is defined here. It is
 * stored backwards, with the last items first in the linked list.
 * (and Yes, it's only to confuse you :)
 */
struct _ntree {
	char *name;
	char *value;
	ntree *next;
	ntree *subtree;
};

static char *get_unescaped_until(char **packet, char limiter)
{
	char *str, *ptr, *txt;

	txt = ptr = str = *packet;

	while (*str != limiter) {
		if ((unsigned char) *str < ' ') {
			str++;
			continue;
		}
		if (*str == '\\')
			str++;
		if (*str == '\0')
			break;
		*ptr++ = *str++;
	}

	*packet = str + 1;
	return strndup(txt, ptr - txt);
}

static char *get_string(char **packet)
{
	char *str, *txt;

	txt = str = *packet;
	while (isalnum(*str) || *str == '_')
		str++;
	*packet = str;
	return strndup(txt, str - txt);
}

static ntree *new_key(char *name, ntree * next)
{
	ntree *key;

	key = calloc(1, sizeof(ntree));

	assert(key);

	key->name = name;
	key->next = next;

	return key;
}

static ntree *parse(char **packet)
{
	ntree *key = NULL;

	while (1)
		switch (**packet) {
		case ' ':
		case '\t':
		case '\n':
		case '\v':
		case '\r':
			(*packet)++;
			continue;
			/* Do not fret, for it is merely a wonderful gcc extension. */
		case 'A'...'Z':
		case 'a'...'z':
		case '0'...'9':
		case '_':
			key = new_key(get_string(packet), key);
			continue;
		case '(':
			if (!key)
				goto error;
			if (key->value)
				goto error;
			(*packet)++;
			key->value = get_unescaped_until(packet, ')');
			continue;
		case '[':
			/* FIXME: remove this when the time comes, kept for backwards
			   compatibility for the moment. */
			if (!key)
				goto error;
			(*packet)++;
			get_unescaped_until(packet, ']');
			continue;
		case '{':
			if (!key)
				goto error;
			(*packet)++;
			key->subtree = parse(packet);
			continue;
		default:
		  error:
			DEBUG("Parse error at %s", *packet);
		case '}':
		case ';':
			(*packet)++;
		case '\0':
			return key;
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
		assert(second_last && second_last->next == last);

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

	free(key->name);
	free(key->value);
	interface_free(key->subtree);
	interface_free(key->next);
	free(key);
}

struct _string {
	char *str;
	int alloc;
	int len;
};

static void escape(struct _string *s, char *src)
{
	int n = s->len;
	char *dst = s->str + n;

	for (;;) {
		switch (*src) {
		case '\0':
			*dst = '\0';
			s->len = n;
			return;
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case ';':
			*dst++ = '\\';
			n++;
		default:
			*dst++ = *src++;
			n++;
		}
	}
}

static void build(ntree * key, struct _string *s)
{
	int required_space;

	if (!key)
		return;

	if (key->next)
		build(key->next, s);

	required_space = 2 * strlen(key->name) + 32;	/* just to be sure */
	if (key->value)
		required_space += 2 * strlen(key->value) + 2;

	if (s->len + required_space > s->alloc) {
		/* gotta alloc some more. double size in each reallocation */
		s->alloc = (s->len + required_space) * 2;
		s->str = realloc(s->str, s->alloc);
		assert(s->str);
	}

	escape(s, key->name);

	if (key->value) {
		s->str[s->len++] = '(';
		escape(s, key->value);
		s->str[s->len++] = ')';
	}

	if (key->subtree) {
		s->str[s->len++] = '{';
		build(key->subtree, s);
		s->str[s->len++] = '}';
	}
	s->str[s->len++] = ' ';
}

char *interface_construct(ntree * tree)
{
	struct _string s = { NULL, 0, 0 };

	build(tree, &s);

	/* rewind the trailing space for cleanliness */
	/* also, giFT 020825 seems to _require_ no space before ; */
	s.len--;
	s.str[s.len++] = ';';
	s.str[s.len] = '\0';

	return s.str;
}

char *interface_lookup(ntree * tree, char *key)
{
	assert(key);

	/* search for the appropriate node */
	tree = tree->subtree;
	while (tree) {
		if (!strcasecmp(tree->name, key))
			return tree->value;
		tree = tree->next;
	}
	return NULL;
}

char *interface_name(ntree * tree)
{
	return tree->name;
}

char *interface_value(ntree * tree)
{
	return tree->value;
}

ntree *interface_append(ntree ** packet, char *key_name, char *value)
{
	ntree *key;

	key = new_key(strdup(key_name), packet ? *packet : NULL);

	key->value = my_strdup(value);

	if (packet)
		*packet = key;
	return key;
}

ntree *interface_append_int(ntree ** packet, char *key_name, unsigned int value)
{
	char pulvermos[40];

	sprintf(pulvermos, "%u", value);

	return interface_append(packet, key_name, pulvermos);
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
