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

#include "network.h"
#include "queue.h"

/* #define PRETTY_PRINT */
#include "interface.h"

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

struct _inode
{
	char *key;
	char *keydisp;
	char *value;
};

/*****************************************************************************/

static struct _inode *inode_new (char *key, char *value)
{
	struct _inode *inode;
	char *ptr;

	if (!key)
		return NULL;

	if (!(inode = malloc (sizeof (struct _inode))))
		return NULL;

	inode->key = STRDUP (key);
	inode->keydisp = STRDUP (key);

	if ((ptr = strchr (inode->keydisp, '[')))
		*ptr = 0;

	inode->value = STRDUP (value);

	return inode;
}

static void inode_free (struct _inode *inode)
{
	free (inode->key);
	free (inode->keydisp);
	free (inode->value);
	free (inode);
}

/*****************************************************************************/

Interface *interface_new (char *command, char *value)
{
	Interface *p;

	if (!(p = malloc (sizeof (Interface))))
		return NULL;

	p->command = STRDUP (command);
	p->value   = STRDUP (value);
	p->tree    = NULL;

	return p;
}

static void free_key (TreeNode *node, void *udata, int depth)
{
	inode_free (node->data);
}

void interface_free (Interface *p)
{
	if (!p)
		return;

	tree_foreach (&p->tree, NULL, 0, TRUE, (TreeForeach)free_key, NULL);
	tree_destroy (&p->tree);

	free (p->command);
	free (p->value);
	free (p);
}

/*****************************************************************************/

static void set_data (char **data, char *value)
{
	free (*data);
	*data = STRDUP (value);
}

void interface_set_command (Interface *p, char *command)
{
	if (!p)
		return;

	set_data (&p->command, command);
}

void interface_set_value (Interface *p, char *value)
{
	if (!p)
		return;

	set_data (&p->value, value);
}

/*****************************************************************************/

static int keypathcmp (struct _inode *inode, char *key)
{
	return strcasecmp (inode->key, key);
}

static TreeNode *lookup (Interface *p, char *key)
{
	TreeNode *node = NULL;
	TreeNode *st;
	char     *keydup, *keydup0;
	char     *token;

	/* string_sep is going to modify key, so we have to make sure that this
	 * is safe */
	if (!p || !(keydup = STRDUP (key)))
		return NULL;

	/* string_sep will modify its argument, so we must track the beginning */
	keydup0 = keydup;

	while ((token = string_sep (&keydup, "/")))
	{
		st = (node ? node->child : NULL);

		node = tree_find (&p->tree, st, FALSE,
		                  (TreeNodeCompare)keypathcmp, token);

		if (!node)
			break;
	}

	free (keydup0);
	return node;
}

char *interface_get (Interface *p, char *key)
{
	TreeNode      *node;
	struct _inode *inode;

	if (!(node = lookup (p, key)) || !(inode = node->data))
		return NULL;

	/* we don't want to permit NULL her so that the caller can differentiate
	 * between empty value and non-existant key */
	return STRING_NOTNULL (inode->value);
}

/*****************************************************************************/

int interface_put (Interface *p, char *key, char *value)
{
	TreeNode      *node;
	struct _inode *inode;
	char          *keypath;
	char          *keyname;

	if (!p || !(keypath = STRDUP (key)))
		return FALSE;

	if ((keyname = strrchr (keypath, '/')))
		*keyname++ = 0;
	else
		keyname = keypath;

	node = lookup (p, keypath);

	if ((inode = inode_new (keyname, value)))
		tree_insert (&p->tree, node, NULL, inode);

	free (keypath);
	return TRUE;
}

/*****************************************************************************/

static void foreach_wrapper (TreeNode *node, void **data, int depth)
{
	Interface       *p       = data[0];
	char            *keypath = data[1];
	InterfaceForeach func    = data[2];
	void            *udata   = data[3];
	struct _inode   *inode   = node->data;

	(*func) (p, keypath, node->child ? TRUE : FALSE, inode->key, inode->value,
	         udata);
}

void interface_foreach (Interface *p, char *key,
                        InterfaceForeach func, void *udata)
{
	TreeNode *node;
	void     *data[] = { p, key, func, udata };

	if (!p || !func)
		return;

	node = lookup (p, key);
	tree_foreach (&p->tree, (node ? node->child : NULL), 0, FALSE,
	              (TreeForeach)foreach_wrapper, data);
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

static char *escape (char *str)
{
	String *escaped;
	char   *ptr;

	if (!str || !(*str))
		return STRDUP (str);

	if (!(escaped = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	for (ptr = str; *ptr; ptr++)
	{
		switch (*ptr)
		{
		 case '(':
		 case ')':
		 case '[':
		 case ']':
		 case '{':
		 case '}':
		 case '\\':
		 case ';':
			string_appendf (escaped, "\\%c", *ptr);
			break;
		 default:
			string_appendc (escaped, *ptr);
			break;
		}
	}

	return string_free_keep (escaped);
}

static char *unescape (char *str)
{
	char *pwrite;
	char *pread;

	for (pread = pwrite = str; *pread; pread++, pwrite++)
	{
		if (*pread == '\\')
			pread++;

		/* weinholt: ha. */
		if (pwrite != pread)
			*pwrite = *pread;
	}

	*pwrite = '\0';

	return str;
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
		/* skip over the next char */
		if (*ptr == '\\')
		{
			ptr++;
			continue;
		}

		if (is_special (*ptr, context) != TOKEN_TEXT)
			break;
	}

	*packet = ptr;

	if (!(text = STRDUP_N (str, (ptr - str))))
		text = STRDUP ("");

	string_trim (text);
	unescape (text);

	return new_token (TOKEN_TEXT, text);
}

/*****************************************************************************/

static int last_depth = 0;

static void indent (TreeNode *node, String *s, int depth)
{
#ifdef PRETTY_PRINT
#if 0
	/* not the command */
	if (!node || (node->prev || node->parent))
		string_append (s, "  ");
#endif

	while (depth-- > 0)
		string_append (s, "  ");
#endif /* PRETTY_PRINT */
}

static void show_depth (TreeNode *node, String *s,
                        int depth, int *depth_last)
{
	char after_block;
	int  depth_prev;
	int  depth_in;

	depth_prev = depth;
	depth_in   = depth;

#ifdef PRETTY_PRINT
	after_block = '\n';
#else /* !PRETTY_PRINT */
	after_block = ' ';
#endif /* PRETTY_PRINT */

	if (depth == *depth_last)
		indent (node, s, depth);
	else if (depth > *depth_last)
	{
		indent (node, s, *depth_last);

		while (depth-- > *depth_last)
		{
			string_appendf (s, "{%c", after_block);
			indent (node, s, depth_in++);
		}
	}
	else /* if (depth < *depth_last) */
	{
		indent (node, s, *depth_last - 1);

		while (depth++ < *depth_last)
		{
			string_appendf (s, "}%c", after_block);
			indent (node, s, depth_in--);
		}
	}

	*depth_last = depth_prev;
}

static void append_escaped (String *s, char *fmt, char *str)
{
	char *escaped;

	escaped = escape (str);
	string_appendf (s, fmt, escaped);
	free (escaped);
}

static void appendnode (String *s, char *key, char *value)
{
	char after_block;

	append_escaped (s, "%s", key);

#if 0
	if (inode->attr && inode->attr[0])
		append_escaped (s, "[%s]", inode->attr);
#endif

	if (value && *value)
		append_escaped (s, "(%s)", value);

#ifdef PRETTY_PRINT
	after_block = '\n';
#else
	after_block = ' ';
#endif /* PRETTY_PRINT */

	string_appendc (s, after_block);
}

static void build (TreeNode *node, String *s, int depth)
{
	struct _inode *inode;

	if (!(inode = node->data))
		return;

	show_depth (node, s, depth + 1, &last_depth);
	appendnode (s, inode->keydisp, inode->value);
}

String *interface_serialize (Interface *p)
{
	String *s;
	int     shift_back;

	if (!p || !(s = string_new (NULL, 0, 0, TRUE)))
		return NULL;

	/* put the command information */
	appendnode (s, p->command, p->value);
	last_depth = 1;

	/* then put the children */
	tree_foreach (&p->tree, NULL, 0, TRUE, (TreeForeach) build, s);
	show_depth (NULL, s, 0, &last_depth);

	/* determine how much data can safely be rewound */
#ifdef PRETTY_PRINT
	shift_back = 3;
#else
	shift_back = 3; /* 1; */
#endif

	/* rewind the trailing space(s) for cleanliness */
	if (s->len >= shift_back)
		s->len -= shift_back;

	string_append (s, ";\n");

	return s;
}

/*****************************************************************************/

static TreeNode *flush (Tree **tree, TreeNode *node, TreeNode **r_last,
                        struct _inode *inode, struct _inode **r_inode)
{
	TreeNode *last = NULL;

	if (!inode)
		return NULL;

	if (!tree_find (tree, node, TRUE, NULL, inode))
		last = tree_insert (tree, node, NULL, inode);

	if (r_inode)
		*r_inode = NULL;

	if (r_last && last)
		*r_last = last;

	return last;
}

static int parse (Interface *p, TreeNode *node, char **data)
{
	TreeNode         *last    = NULL;
	enum _token_type  context = TOKEN_TEXT;
	struct _inode    *inode   = NULL;
	struct _token    *token;

	while ((token = get_token (data, context)))
	{
		switch (token->type)
		{
		 case TOKEN_TEXT:
			{
				switch (context)
				{
				 case TOKEN_TEXT:
					{
						flush (&p->tree, node, &last, inode, &inode);

						if (p->command)
							inode = inode_new (token->str, NULL);
						else
							p->command = STRDUP (token->str);
					}
					break;
				 case TOKEN_PAREN_OPEN:
				 case TOKEN_BRACK_OPEN:
					{
						char **attr;

						if (!inode && !p->command)
						{
							free_token (token);
							return FALSE;
						}

						if (!inode)
							attr = &p->value;
						else
						{
							/*
							 * TOKEN_BRACK_OPEN is no longer valid, but we
							 * can't really remove all the extra cruft it
							 * introduces here quite yet.
							 */
							attr = (context == TOKEN_PAREN_OPEN) ?
								&inode->value : NULL;
						}

						if (attr)
							*attr = STRDUP (token->str);
					}
					break;
				 default:
					break;
				}
			}
			break;
		 case TOKEN_PAREN_OPEN:
		 case TOKEN_BRACK_OPEN:
			context = token->type;
			break;
		 case TOKEN_PAREN_CLOSE:
		 case TOKEN_BRACK_CLOSE:
			context = TOKEN_TEXT;
			break;
		 case TOKEN_BRACE_OPEN:
			{
				/* NOTE: we arent really flushing here because
				 * META { ... } (x) is technically valid syntax, but we need
				 * this entry in the tree for parenting */
				flush (&p->tree, node, &last, inode, NULL);
				if (!parse (p, last, data))
				{
					free_token (token);
					return FALSE;
				}
			}
			break;
		 case TOKEN_BRACE_CLOSE:
		 case TOKEN_SEMICOLON:
			{
				flush (&p->tree, node, &last, inode, &inode);
				free_token (token);
				return TRUE;
			}
			break;
		 default:
			break;
		}

		free_token (token);
	}

	if (inode)
		flush (&p->tree, node, NULL, inode, &inode);

	if (!p->tree && !p->command)
		return FALSE;

	return TRUE;
}

Interface *interface_unserialize (char *data, size_t len)
{
	Interface *p;
	char      *datadup, *datadup0;
	int        ret = FALSE;

	if (!(p = interface_new (NULL, NULL)))
	{
		return NULL;
	}

	/* TODO: this NEEDS to be fixed! */
	if ((datadup = STRDUP_N (data, len)))
	{
		datadup0 = datadup;
		ret = parse (p, NULL, &datadup);
		free (datadup0);
	}

	if (!ret)
	{
		interface_free (p);
		return NULL;
	}

	return p;
}

/*****************************************************************************/

static int isend (Connection *c, String *s, void *udata)
{
	if (!s)
		return FALSE;

	net_send (c->fd, s->str, s->len);
	return FALSE;
}

static int ifree (Connection *c, String *s, void *udata)
{
	if (!s)
		return FALSE;

	string_free (s);
	return FALSE;
}

int interface_send (Interface *p, Connection *c)
{
	String *s;

	if (!c || !(s = interface_serialize (p)))
		return -1;

	queue_add_single (c, (QueueWriteFunc)isend, (QueueWriteFunc)ifree,
	                  s, NULL);

	return s->len;
}

/*****************************************************************************/

#if 0
static void foreachdump (TreeNode *node, void *data, int depth)
{
	struct _inode *inode = node->data;

	while (depth-- > 0)
		printf ("  ");

	printf ("%s = %s\n", inode->key, inode->value);
}

static void interface_dump (Interface *p)
{
	tree_foreach (&p->tree, NULL, 0, TRUE, (TreeForeach)foreachdump, NULL);
}
#endif

/*****************************************************************************/

#if 0
static Interface *build_packet ()
{
	Interface *p = NULL;

	if (!(p = interface_new ("search", stringf ("%d", 4))))
		return NULL;

	interface_put (p, "query", "foo (bar)");
	interface_put (p, "meta", NULL);
	interface_put (p, "meta/bitrate", ">=192");
	interface_put (p, "meta/foo", "bar");
	interface_put (p, "source[0]", NULL);
	interface_put (p, "source[0]/user", "someguy");
	interface_put (p, "source[1]", NULL);
	interface_put (p, "source[1]/user", "someotherguy");
	interface_put (p, "source[1]/foo", NULL);
	interface_put (p, "source[1]/foo/bar", "baz");

	return p;
}

static void ffunc (Interface *p, char *keypath, int children,
				   char *key, char *value, void *udata)
{
	printf ("keypath[%i]='%s', key='%s', value='%s'\n",
	        children, keypath, key, value);
}

int main ()
{
	Interface *p;
	String    *s;

	p = build_packet ();

	printf ("FOREACH 1\n");
	interface_foreach (p, "Meta", (InterfaceForeach)ffunc, NULL);
	printf ("\n");

	s = interface_serialize (p);
	interface_free (p);

	printf ("PASS 1\n%s\n", s->str);

	p = interface_unserialize (s->str, s->len);
	string_free (s);
	s = interface_serialize (p);
	interface_free (p);

	printf ("PASS 2\n%s\n", s->str);

	string_free (s);

	return 0;
}
#endif
