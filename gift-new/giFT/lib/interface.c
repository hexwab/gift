/*
 * interface.c
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

#include <ctype.h>

#include "gift.h"

#include "queue.h"

#include "parse.h"
#include "conf.h"

#include "network.h"
#include "event.h"
#include "nb.h"

/*****************************************************************************/

enum _token_type
{
	TOKEN_TEXT = 0,
	TOKEN_SPACE,                       /* ' ' */
	TOKEN_PAREN_OPEN,                  /* '(' */
	TOKEN_PAREN_CLOSE,                 /* ')' */
	TOKEN_BRACK_OPEN,                  /* '[' */
	TOKEN_BRACK_CLOSE,                 /* ']' */
	TOKEN_BRACE_OPEN,                  /* '{' */
	TOKEN_BRACE_CLOSE,                 /* '}' */
	TOKEN_SEMICOLON                    /* ';' */
};

struct _token
{
	char            *str;
	enum _token_type type;
};

struct _key
{
	char *name;
	char *attr;
	char *value;
};

/*****************************************************************************/

/* Getopt::Long.pm like. */
char *interface_construct_packet (int *len, char *event, va_list args)
{
	char       *tag;
	char       *value;
	static char string[2048];
	int         string_len = 0;

	string_len += snprintf (string, sizeof (string) - 1, "<%s", event);

	for (;;)
	{
		char *type;

		if (!(tag = va_arg (args, char *)))
			break;

		tag = strdup (tag);

		/* parse TAG=string */
		if (!(type = strchr (tag, '=')))
		{
			free (tag);
			break;
		}

		*type++ = 0;

		switch (*type)
		{
		 case 's':
			value = va_arg (args, char *);
			break;
		 case 'i':
			value = ITOA (va_arg (args, long)); /* promote */
			break;
		 default:
			value = NULL;
			break;
		}

		if (!value)
			value = "";

		if (string_len + strlen (tag) + strlen (value) < sizeof (string) - 12)
		{
			string_len += sprintf (string + string_len,
			                       " %s=\"%s\"", tag, value);
		}

		free (tag);
	}

	string_len += sprintf (string + string_len, "/>\r\n");

	if (len)
		*len = string_len;

	return string;
}

/* constructs a xml-like packet resembling <HEAD TAG=VALUE.../>\n and sends
 * it to the supplied socket via the queue subsystem */
void interface_send (Connection *c, char *event, ...)
{
	va_list args;
	char   *data;
	int     len;

	if (!c || !event)
		return;

	va_start (args, event);
	data = interface_construct_packet (&len, event, args);
	va_end (args);

	queue_add_single (c, NULL, NULL, STRDUP (data), I_PTR (len));
}

/* stupid helper function */
void interface_send_err (Connection *c, char *error)
{
	interface_send (c, "error", "text=s", error, NULL);
}

/* called any time a dc connection was closed.  handles registering with the
 * protocol, flushing the queue, and closing the socket as well as any
 * arbitrary event specific cleanups that must be handled */
void interface_close (Connection *c)
{
	/* dc_close may call this function, clean up the logic in the protocol
	 * by simply avoiding that condition */
	if (!c || c->closing)
		return;

	c->closing = TRUE;

	if (c->protocol && c->protocol->dc_close)
		c->protocol->dc_close (c);

	if_event_close (c);

	connection_close (c);
}

/*****************************************************************************/

/* sure as hell beats anubis' parsing nightmare :) */
int interface_parse_packet (Dataset **dataset, char *packet)
{
	char *tag;
	char *value;

	/*    <THIS TAG=VALUE TAG=VALUE/>    \r */

	trim_whitespace (packet);

	if (*packet != '<')
		return FALSE;

	packet++;

	/* THIS TAG=VALUE TAG=VALUE/> */

	if (!(tag = strrchr (packet, '/')))
		return FALSE;

	*tag = 0;

	/* THIS TAG=VALUE TAG=VALUE */

	if (!(tag = string_sep (&packet, " ")))
		return FALSE;

	dataset_insert (*dataset, "head", STRDUP (tag));

	/* TAG=VALUE TAG=VALUE */

	while ((tag = string_sep (&packet, "=")))
	{
		if (!packet)
			return FALSE;

		/* support tag=value or tag="value with spaces" */
		if (*packet == '\"')
		{
			packet++;
			value = string_sep (&packet, "\"");
		}
		else
			value = string_sep (&packet, " ");

		if (packet && *packet == ' ')
			packet++;

		if (!strcmp (tag, "head"))
		{
			GIFT_WARN (("'head' is an invalid tag!"));
			continue;
		}

		dataset_insert (*dataset, tag, STRDUP (value));
	}

	/* (null) */

	return TRUE;
}

/*****************************************************************************/
/* INTERFACE PROTOCOL REVISION 2 */
/*****************************************************************************/

char *interface_keylist (int *len, va_list args)
{
	static char str[2048];
	int         str_len = 0;

	for (;;)
	{
		char *key;                     /* SEARCH */
		char *key_def;                 /* SEARCH=si */
		char *type;                    /* si */
		char *block = NULL;            /* { ... } */
		char *mod   = NULL;            /* >= */
		char *value = NULL;            /* 4 */

		if (!(key_def = STRDUP (va_arg (args, char *))))
			break;

		key = key_def;

		if ((type = strchr (key_def, '=')))
		{
			*type++ = 0;

			for (; *type; type++)
			{
				char **attr;

				/* some days i just shouldnt be writing code */
				attr = (type[1]) ? &mod : &value;

				switch (*type)
				{
				 case 's':
					*attr = va_arg (args, char *);
					break;
				 case 'i':
					*attr = ITOA (va_arg (args, long));   /* promote */
					break;
				 case 'k':
					block = va_arg (args, char *);
					break;
				 default:
					*attr = NULL;
					break;
				}
			}
		}

		string_append (str, sizeof (str), &str_len, "%s", key);

		if (mod)
			string_append (str, sizeof (str), &str_len, "[%s]", mod);

		if (value)
			string_append (str, sizeof (str), &str_len, "(%s)", value);

		if (block)
			string_append (str, sizeof (str), &str_len, " { %s }", block);

		string_append (str, sizeof (str), &str_len, " ");
	}

	return NULL;
}

char *interface2_construct (int *len, va_list args)
{
	return NULL;
}

/*****************************************************************************/

static struct _token *new_token (enum _token_type type, char *str)
{
	struct _token *token;

	if (!(token = malloc (sizeof (struct _token))))
		return NULL;

	token->type = type;
	token->str  = str;

	return token;
}

static void free_token (struct _token *token)
{
	free (token->str);
	free (token);
}

static enum _token_type is_special (char c, enum _token_type context)
{
	enum _token_type type = TOKEN_TEXT;

	if (isspace (c))
		c = ' ';

	switch (c)
	{
	 case '(': type = TOKEN_PAREN_OPEN;  break;
	 case ')': type = TOKEN_PAREN_CLOSE; break;
	 case '[': type = TOKEN_BRACK_OPEN;  break;
	 case ']': type = TOKEN_BRACK_CLOSE; break;
	 case '{': type = TOKEN_BRACE_OPEN;  break;
	 case '}': type = TOKEN_BRACE_CLOSE; break;
	 case ';': type = TOKEN_SEMICOLON;   break;
	 case ' ':
		if (context == TOKEN_TEXT)
			type = TOKEN_SPACE;
		break;
	}

	return type;
}

static struct _token *get_token (char **packet, enum _token_type context)
{
	enum _token_type  type;
	char             *text;
	char             *ptr;
	char             *str;

	if (!(str = *packet) || !str[0])
		return NULL;

	/* shift past senseless whitespace */
	while ((is_special (*str, context)) == TOKEN_SPACE)
		str++;

	/* first things first, lets check if we're currently sitting on some kind
	 * of special character */
	if ((type = is_special (*str, context)) != TOKEN_TEXT)
	{
		(*packet) = str + 1;
		return new_token (type, STRDUP_N (str, 1));
	}

	/* gobble up the waiting text token */
	for (ptr = str; *ptr; ptr++)
	{
		if (is_special (*ptr, context) != TOKEN_TEXT)
			break;
	}

	*packet = ptr;

	text = STRDUP_N (str, (ptr - str));
	trim_whitespace (text);
	return new_token (TOKEN_TEXT, text);
}

/*****************************************************************************/

static struct _key *new_key (char *name)
{
	struct _key *key;

	if (!(key = malloc (sizeof (struct _key))))
		return NULL;

	memset (key, 0, sizeof (struct _key));
	key->name = STRDUP (name);

	return key;
}

static TreeNode *flush (Tree **tree, TreeNode *node,
                        struct _key *key, struct _key **r_key)
{
	TreeNode *last;

	if (!key)
		return NULL;

	if (!tree_find (tree, node, TRUE, NULL, key))
		last = tree_insert (tree, node, NULL, key);

	if (r_key)
		*r_key = NULL;

	return last;
}

static int parse (Tree **tree, TreeNode *node, char **packet)
{
	TreeNode         *last    = NULL;
	enum _token_type  context = TOKEN_TEXT;
	struct _key      *key     = NULL;
	struct _token    *token;

	while ((token = get_token (packet, context)))
	{
		switch (token->type)
		{
		 case TOKEN_TEXT:
			{
				switch (context)
				{
				 case TOKEN_TEXT:
					last = flush (tree, node, key, &key);
					key  = new_key (token->str);
					break;
				 case TOKEN_PAREN_OPEN:
				 case TOKEN_BRACK_OPEN:
					{
						char **attr;

						if (!key)
						{
							free_token (token);
							return FALSE;
						}

						attr = (context == TOKEN_PAREN_OPEN) ?
						    &key->value : &key->attr;

						*attr = STRDUP (token->str);
					}
					break;
				 default:
					break;
				}
			}
			break;
		 case TOKEN_PAREN_OPEN:
		 case TOKEN_BRACK_OPEN:      context = token->type;   break;
		 case TOKEN_PAREN_CLOSE:
		 case TOKEN_BRACK_CLOSE:     context = TOKEN_TEXT;    break;
		 case TOKEN_BRACE_OPEN:
			{
				/* NOTE: we arent really flushing here because
				 * META { ... } (x) is technically valid syntax, but we need
				 * this entry in the tree for parenting */
				last = flush (tree, node, key, NULL);
				parse (tree, last, packet);
			}
			break;
		 case TOKEN_BRACE_CLOSE:
		 case TOKEN_SEMICOLON:
			{
				last = flush (tree, node, key, &key);
				free_token (token);
				return TRUE;
			}
			break;
		 default:
			break;
		}

		free_token (token);
	}

	return TRUE;
}

int interface2_parse (Tree **tree, char *packet)
{
	if (!tree)
		return FALSE;

	printf ("parsing:\n%s\n", packet);

	return parse (tree, NULL, &packet);
}

static void dump_func (TreeNode *node, void *udata, int depth)
{
	struct _key *key = node->data;

	while (depth--)
		printf ("\t");

	printf ("%s ", key->name);

	if (key->attr)
		printf ("(%s) ", key->attr);

	if (key->value)
		printf ("= %s ", key->value);

	printf ("\n");
}

void interface2_dump (Tree **tree)
{
	printf ("\ndumping:\n");
	tree_foreach (tree, NULL, 0, TRUE, (TreeForeach) dump_func, NULL);
}

/*****************************************************************************/

#if 0
int main ()
{
	Tree *tree = NULL;
	char *search;

	search = "SEARCH(4)\n"
	         "  query (foo bar)\n"
	         "  realm [==] (  audio  )\n"
	         "  META {\n"
	         "  \tbitrate[>=](192)\n"
	         "  \tfoo[bar](bla)\n"
	         "  } [c owns]\n"
	         "    (me)\n"
	         "  bla       (       blum!   )\n"
	         ";\n";

	interface2_parse (&tree, search);
	interface2_dump  (&tree);

	return 0;
}
#endif
